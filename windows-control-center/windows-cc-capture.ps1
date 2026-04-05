<#
.SYNOPSIS
  No-install collector for Mechrevo Control Center on Windows.

.DESCRIPTION
  Collects package metadata, candidate registry keys, running processes,
  app data file changes, and localhost connection changes while you toggle
  fan modes in the Windows Control Center.

  This script uses only built-in PowerShell and Windows commands.

.EXAMPLE
  powershell -ExecutionPolicy Bypass -File .\windows-cc-capture.ps1

.EXAMPLE
  powershell -ExecutionPolicy Bypass -File .\windows-cc-capture.ps1 -DurationSec 90 -IntervalMs 500

.EXAMPLE
  powershell -ExecutionPolicy Bypass -File .\windows-cc-capture.ps1 -SnapshotOnly
#>

param(
    [int]$DurationSec = 60,
    [int]$IntervalMs = 500,
    [string]$OutDir = "",
    [switch]$SnapshotOnly
)

$ErrorActionPreference = "Continue"
Set-StrictMode -Version 2.0

if ([string]::IsNullOrWhiteSpace($OutDir)) {
    $stamp = Get-Date -Format "yyyyMMdd-HHmmss"
    $OutDir = Join-Path -Path (Get-Location) -ChildPath ("cc-capture-" + $stamp)
}

New-Item -ItemType Directory -Path $OutDir -Force | Out-Null

$MainLog = Join-Path $OutDir "capture.log"
$FileChangeLog = Join-Path $OutDir "file-changes.log"
$RegChangeLog = Join-Path $OutDir "registry-changes.log"
$NetChangeLog = Join-Path $OutDir "net-changes.log"
$ProcChangeLog = Join-Path $OutDir "process-changes.log"
$TranscriptPath = Join-Path $OutDir "transcript.txt"

function Write-Log {
    param([string]$Message)
    $line = "{0} {1}" -f (Get-Date -Format "yyyy-MM-dd HH:mm:ss.fff"), $Message
    $line | Tee-Object -FilePath $MainLog -Append
}

function Write-Section {
    param(
        [string]$Path,
        [string]$Title,
        [string[]]$Lines
    )
    Add-Content -Path $Path -Value ("[{0}] {1}" -f (Get-Date -Format "yyyy-MM-dd HH:mm:ss.fff"), $Title)
    foreach ($line in $Lines) {
        Add-Content -Path $Path -Value $line
    }
    Add-Content -Path $Path -Value ""
}

function Export-CsvSafe {
    param(
        [string]$Path,
        [object[]]$Items
    )
    if ($null -eq $Items -or @($Items).Count -eq 0) {
        "" | Out-File -FilePath $Path -Encoding utf8
        return
    }
    $Items | Export-Csv -Path $Path -NoTypeInformation -Encoding utf8
}

function Get-ControlCenterPackage {
    $pkgs = @()
    try {
        $pkgs = Get-AppxPackage -ErrorAction Stop |
            Where-Object {
                $_.Name -like "ControlCenter3*" -or
                $_.PackageFamilyName -like "ControlCenter3*" -or
                $_.InstallLocation -match "GamingCenter3|ControlCenter3"
            }
    } catch {
        Write-Log ("WARN Get-AppxPackage failed: {0}" -f $_.Exception.Message)
    }

    if (@($pkgs).Count -gt 0) {
        return $pkgs | Select-Object -First 1
    }
    return $null
}

function Get-SystemInfo {
    $rows = @()

    try {
        $cs = Get-CimInstance Win32_ComputerSystemProduct -ErrorAction Stop
        $rows += [pscustomobject]@{ Key = "SystemProductName"; Value = [string]$cs.Name }
        $rows += [pscustomobject]@{ Key = "SystemSKU"; Value = [string]$cs.SKUNumber }
        $rows += [pscustomobject]@{ Key = "SystemVendor"; Value = [string]$cs.Vendor }
    } catch {
        Write-Log ("WARN Win32_ComputerSystemProduct failed: {0}" -f $_.Exception.Message)
    }

    try {
        $bb = Get-CimInstance Win32_BaseBoard -ErrorAction Stop
        $rows += [pscustomobject]@{ Key = "BaseBoardProduct"; Value = [string]$bb.Product }
        $rows += [pscustomobject]@{ Key = "BaseBoardManufacturer"; Value = [string]$bb.Manufacturer }
    } catch {
        Write-Log ("WARN Win32_BaseBoard failed: {0}" -f $_.Exception.Message)
    }

    try {
        $os = Get-CimInstance Win32_OperatingSystem -ErrorAction Stop
        $rows += [pscustomobject]@{ Key = "OSCaption"; Value = [string]$os.Caption }
        $rows += [pscustomobject]@{ Key = "OSVersion"; Value = [string]$os.Version }
        $rows += [pscustomobject]@{ Key = "OSBuildNumber"; Value = [string]$os.BuildNumber }
    } catch {
        Write-Log ("WARN Win32_OperatingSystem failed: {0}" -f $_.Exception.Message)
    }

    $rows += [pscustomobject]@{ Key = "CurrentUser"; Value = [string][System.Security.Principal.WindowsIdentity]::GetCurrent().Name }
    $rows += [pscustomobject]@{ Key = "IsAdmin"; Value = [string]([bool](([Security.Principal.WindowsPrincipal] [Security.Principal.WindowsIdentity]::GetCurrent()).IsInRole([Security.Principal.WindowsBuiltInRole] "Administrator"))) }

    return $rows
}

function Get-TargetProcessSnapshot {
    $items = @()
    try {
        $items = Get-CimInstance Win32_Process -ErrorAction Stop |
            Where-Object {
                $_.Name -match "(?i)GamingCenter|ControlCenter|SystrayComponent"
            } |
            Select-Object @{
                    Name = "ProcessId"
                    Expression = { [int]$_.ProcessId }
                }, @{
                    Name = "Name"
                    Expression = { [string]$_.Name }
                }, @{
                    Name = "ExecutablePath"
                    Expression = { [string]$_.ExecutablePath }
                }, @{
                    Name = "CommandLine"
                    Expression = { [string]$_.CommandLine }
                }
    } catch {
        Write-Log ("WARN process snapshot failed: {0}" -f $_.Exception.Message)
    }

    return $items | Sort-Object Name, ProcessId
}

function Get-TargetPids {
    param([object[]]$ProcessSnapshot)
    return @($ProcessSnapshot | ForEach-Object { [int]$_.ProcessId })
}

function Get-NetSnapshot {
    param([int[]]$TargetPids)

    $rows = @()

    if (Get-Command Get-NetTCPConnection -ErrorAction SilentlyContinue) {
        try {
            $conns = Get-NetTCPConnection -ErrorAction Stop
            foreach ($c in $conns) {
                $isTarget = @($TargetPids).Count -gt 0 -and (@($TargetPids) -contains [int]$c.OwningProcess)
                $isLoopback = $c.LocalAddress -in @("127.0.0.1", "::1") -or $c.RemoteAddress -in @("127.0.0.1", "::1")
                if (-not $isTarget -and -not $isLoopback) {
                    continue
                }

                $rows += [pscustomobject]@{
                    Proto = "TCP"
                    Local = ("{0}:{1}" -f $c.LocalAddress, $c.LocalPort)
                    Remote = ("{0}:{1}" -f $c.RemoteAddress, $c.RemotePort)
                    State = [string]$c.State
                    OwningProcess = [int]$c.OwningProcess
                }
            }
        } catch {
            Write-Log ("WARN Get-NetTCPConnection failed: {0}" -f $_.Exception.Message)
        }
    } else {
        Write-Log "WARN Get-NetTCPConnection not available"
    }

    return $rows | Sort-Object Proto, Local, Remote, State, OwningProcess
}

function Get-RegistryRoots {
    $roots = @(
        "HKLM:\SOFTWARE\OEM\GamingCenter2",
        "HKLM:\SOFTWARE\WOW6432Node\OEM\GamingCenter2",
        "HKCU:\SOFTWARE\OEM\GamingCenter2"
    )

    return @($roots | Where-Object { Test-Path -LiteralPath $_ })
}

function Get-RegistrySnapshot {
    param([string[]]$Roots)

    $rows = @()

    foreach ($root in @($Roots)) {
        try {
            $keys = @(Get-Item -LiteralPath $root -ErrorAction Stop)
            $childKeys = Get-ChildItem -LiteralPath $root -Recurse -ErrorAction SilentlyContinue
            if ($childKeys) {
                $keys += $childKeys
            }

            foreach ($key in $keys) {
                try {
                    $props = Get-ItemProperty -Path $key.PSPath -ErrorAction Stop
                    foreach ($prop in $props.PSObject.Properties) {
                        if ($prop.Name -like "PS*") {
                            continue
                        }

                        $value = $prop.Value
                        if ($value -is [array]) {
                            $value = ($value -join ";")
                        }

                        $rows += [pscustomobject]@{
                            Path = [string]$key.Name
                            Name = [string]$prop.Name
                            Value = [string]$value
                        }
                    }
                } catch {
                    Write-Log ("WARN registry read failed for {0}: {1}" -f $key.Name, $_.Exception.Message)
                }
            }
        } catch {
            Write-Log ("WARN registry root failed for {0}: {1}" -f $root, $_.Exception.Message)
        }
    }

    return $rows | Sort-Object Path, Name
}

function Get-CandidateDataRoots {
    param($Package)

    $roots = New-Object System.Collections.Generic.List[string]

    if ($Package -and $Package.PackageFamilyName) {
        $pkgData = Join-Path $env:LOCALAPPDATA "Packages"
        if (Test-Path -LiteralPath $pkgData) {
            $dir = Join-Path $pkgData $Package.PackageFamilyName
            foreach ($sub in @("LocalState", "RoamingState", "TempState", "Settings", "LocalCache")) {
                $path = Join-Path $dir $sub
                if (Test-Path -LiteralPath $path) {
                    $roots.Add($path)
                }
            }
        }
    }

    foreach ($extra in @(
            (Join-Path $env:LOCALAPPDATA "OEM"),
            (Join-Path $env:APPDATA "OEM"),
            (Join-Path $env:PROGRAMDATA "OEM"),
            (Join-Path $env:PROGRAMDATA "ControlCenter"),
            (Join-Path $env:PROGRAMDATA "GamingCenter")
        )) {
        if ($extra -and (Test-Path -LiteralPath $extra)) {
            $roots.Add($extra)
        }
    }

    return @($roots | Select-Object -Unique | Sort-Object)
}

function Get-FileSnapshot {
    param([string[]]$Roots)

    $rows = @()

    foreach ($root in @($Roots)) {
        try {
            $files = Get-ChildItem -LiteralPath $root -File -Recurse -Force -ErrorAction Stop
            foreach ($f in $files) {
                $rows += [pscustomobject]@{
                    Root = [string]$root
                    Path = [string]$f.FullName
                    Length = [int64]$f.Length
                    LastWriteUtc = [string]$f.LastWriteTimeUtc.ToString("o")
                }
            }
        } catch {
            Write-Log ("WARN file snapshot failed for {0}: {1}" -f $root, $_.Exception.Message)
        }
    }

    return $rows | Sort-Object Path
}

function To-Map {
    param(
        [object[]]$Items,
        [scriptblock]$KeyScript,
        [scriptblock]$ValueScript
    )

    $map = @{}
    foreach ($item in @($Items)) {
        $key = & $KeyScript $item
        $value = & $ValueScript $item
        $map[[string]$key] = [string]$value
    }
    return $map
}

function Compare-Maps {
    param(
        [hashtable]$OldMap,
        [hashtable]$NewMap,
        [string]$Kind
    )

    $changes = @()

    foreach ($key in $NewMap.Keys) {
        if (-not $OldMap.ContainsKey($key)) {
            $changes += ("ADD {0} {1} => {2}" -f $Kind, $key, $NewMap[$key])
            continue
        }

        if ($OldMap[$key] -ne $NewMap[$key]) {
            $changes += ("MOD {0} {1} => OLD [{2}] NEW [{3}]" -f $Kind, $key, $OldMap[$key], $NewMap[$key])
        }
    }

    foreach ($key in $OldMap.Keys) {
        if (-not $NewMap.ContainsKey($key)) {
            $changes += ("DEL {0} {1} => {2}" -f $Kind, $key, $OldMap[$key])
        }
    }

    return @($changes | Sort-Object)
}

function Get-PackageInfoText {
    param($Package)

    $lines = @()
    if (-not $Package) {
        return @("Package: not found")
    }

    $lines += ("Name: {0}" -f $Package.Name)
    $lines += ("PackageFamilyName: {0}" -f $Package.PackageFamilyName)
    $lines += ("Version: {0}" -f $Package.Version)
    $lines += ("Publisher: {0}" -f $Package.Publisher)
    $lines += ("InstallLocation: {0}" -f $Package.InstallLocation)

    if ($Package.InstallLocation -and (Test-Path -LiteralPath $Package.InstallLocation)) {
        foreach ($rel in @("AppxManifest.xml", "appsettings.json", "Win32\SystrayComponent.exe", "GamingCenter3_Cross.dll")) {
            $full = Join-Path $Package.InstallLocation $rel
            if (Test-Path -LiteralPath $full) {
                try {
                    $item = Get-Item -LiteralPath $full -ErrorAction Stop
                    $hash = (Get-FileHash -Algorithm SHA256 -LiteralPath $full -ErrorAction Stop).Hash
                    $lines += ("FILE {0}" -f $full)
                    $lines += ("  Length: {0}" -f $item.Length)
                    $lines += ("  LastWriteUtc: {0}" -f $item.LastWriteTimeUtc.ToString("o"))
                    $lines += ("  SHA256: {0}" -f $hash)
                } catch {
                    $lines += ("FILE {0}" -f $full)
                    $lines += ("  ERROR: {0}" -f $_.Exception.Message)
                }
            }
        }

        $fanRoot = Join-Path $Package.InstallLocation "UserFanTables"
        if (Test-Path -LiteralPath $fanRoot) {
            $lines += "UserFanTables:"
            try {
                $dirs = Get-ChildItem -LiteralPath $fanRoot -Directory -ErrorAction Stop |
                    Select-Object -ExpandProperty Name |
                    Sort-Object
                foreach ($dir in $dirs) {
                    $lines += ("  {0}" -f $dir)
                }
            } catch {
                $lines += ("  ERROR: {0}" -f $_.Exception.Message)
            }
        }
    }

    return $lines
}

try {
    Start-Transcript -Path $TranscriptPath -Force | Out-Null
} catch {
    Write-Log ("WARN Start-Transcript failed: {0}" -f $_.Exception.Message)
}

Write-Log ("Output directory: {0}" -f $OutDir)
Write-Log ("DurationSec={0} IntervalMs={1} SnapshotOnly={2}" -f $DurationSec, $IntervalMs, [bool]$SnapshotOnly)

$systemInfo = Get-SystemInfo
$package = Get-ControlCenterPackage
$packageInfo = Get-PackageInfoText -Package $package
$registryRoots = Get-RegistryRoots
$dataRoots = Get-CandidateDataRoots -Package $package
$processBaseline = Get-TargetProcessSnapshot
$pidBaseline = Get-TargetPids -ProcessSnapshot $processBaseline
$netBaseline = Get-NetSnapshot -TargetPids $pidBaseline
$registryBaseline = Get-RegistrySnapshot -Roots $registryRoots
$fileBaseline = Get-FileSnapshot -Roots $dataRoots

$systemInfo | Format-Table -AutoSize | Out-File -FilePath (Join-Path $OutDir "system.txt") -Encoding utf8
$packageInfo | Out-File -FilePath (Join-Path $OutDir "package.txt") -Encoding utf8
$registryRoots | Out-File -FilePath (Join-Path $OutDir "registry-roots.txt") -Encoding utf8
$dataRoots | Out-File -FilePath (Join-Path $OutDir "data-roots.txt") -Encoding utf8
Export-CsvSafe -Path (Join-Path $OutDir "processes-baseline.csv") -Items $processBaseline
Export-CsvSafe -Path (Join-Path $OutDir "net-baseline.csv") -Items $netBaseline
Export-CsvSafe -Path (Join-Path $OutDir "registry-baseline.csv") -Items $registryBaseline
Export-CsvSafe -Path (Join-Path $OutDir "files-baseline.csv") -Items $fileBaseline

Write-Log "Baseline snapshots written"

if ($SnapshotOnly) {
    Write-Log "SnapshotOnly requested, exiting"
    try {
        Stop-Transcript | Out-Null
    } catch {
    }
    return
}

Write-Log "During capture, open Control Center and switch fan mode / boost / custom fan settings"

$processMap = To-Map -Items $processBaseline `
    -KeyScript { param($x) "{0}|{1}" -f $x.ProcessId, $x.Name } `
    -ValueScript { param($x) "{0}|{1}" -f $x.ExecutablePath, $x.CommandLine }
$netMap = To-Map -Items $netBaseline `
    -KeyScript { param($x) "{0}|{1}|{2}|{3}|{4}" -f $x.Proto, $x.Local, $x.Remote, $x.State, $x.OwningProcess } `
    -ValueScript { param($x) "{0}|{1}|{2}|{3}|{4}" -f $x.Proto, $x.Local, $x.Remote, $x.State, $x.OwningProcess }
$registryMap = To-Map -Items $registryBaseline `
    -KeyScript { param($x) "{0}|{1}" -f $x.Path, $x.Name } `
    -ValueScript { param($x) $x.Value }
$fileMap = To-Map -Items $fileBaseline `
    -KeyScript { param($x) $x.Path } `
    -ValueScript { param($x) "{0}|{1}" -f $x.Length, $x.LastWriteUtc }

$deadline = (Get-Date).AddSeconds($DurationSec)
$sample = 0

while ((Get-Date) -lt $deadline) {
    Start-Sleep -Milliseconds $IntervalMs
    $sample += 1

    $processNow = Get-TargetProcessSnapshot
    $pidNow = Get-TargetPids -ProcessSnapshot $processNow
    $netNow = Get-NetSnapshot -TargetPids $pidNow
    $registryNow = Get-RegistrySnapshot -Roots $registryRoots
    $fileNow = Get-FileSnapshot -Roots $dataRoots

    $processNowMap = To-Map -Items $processNow `
        -KeyScript { param($x) "{0}|{1}" -f $x.ProcessId, $x.Name } `
        -ValueScript { param($x) "{0}|{1}" -f $x.ExecutablePath, $x.CommandLine }
    $netNowMap = To-Map -Items $netNow `
        -KeyScript { param($x) "{0}|{1}|{2}|{3}|{4}" -f $x.Proto, $x.Local, $x.Remote, $x.State, $x.OwningProcess } `
        -ValueScript { param($x) "{0}|{1}|{2}|{3}|{4}" -f $x.Proto, $x.Local, $x.Remote, $x.State, $x.OwningProcess }
    $registryNowMap = To-Map -Items $registryNow `
        -KeyScript { param($x) "{0}|{1}" -f $x.Path, $x.Name } `
        -ValueScript { param($x) $x.Value }
    $fileNowMap = To-Map -Items $fileNow `
        -KeyScript { param($x) $x.Path } `
        -ValueScript { param($x) "{0}|{1}" -f $x.Length, $x.LastWriteUtc }

    $procChanges = Compare-Maps -OldMap $processMap -NewMap $processNowMap -Kind "PROCESS"
    if (@($procChanges).Count -gt 0) {
        Write-Section -Path $ProcChangeLog -Title ("sample-{0}" -f $sample) -Lines $procChanges
        Write-Log ("Process changes detected at sample {0}" -f $sample)
        $processMap = $processNowMap
    }

    $netChanges = Compare-Maps -OldMap $netMap -NewMap $netNowMap -Kind "NET"
    if (@($netChanges).Count -gt 0) {
        Write-Section -Path $NetChangeLog -Title ("sample-{0}" -f $sample) -Lines $netChanges
        Write-Log ("Network changes detected at sample {0}" -f $sample)
        $netMap = $netNowMap
    }

    $registryChanges = Compare-Maps -OldMap $registryMap -NewMap $registryNowMap -Kind "REG"
    if (@($registryChanges).Count -gt 0) {
        Write-Section -Path $RegChangeLog -Title ("sample-{0}" -f $sample) -Lines $registryChanges
        Write-Log ("Registry changes detected at sample {0}" -f $sample)
        $registryMap = $registryNowMap
    }

    $fileChanges = Compare-Maps -OldMap $fileMap -NewMap $fileNowMap -Kind "FILE"
    if (@($fileChanges).Count -gt 0) {
        Write-Section -Path $FileChangeLog -Title ("sample-{0}" -f $sample) -Lines $fileChanges
        Write-Log ("File changes detected at sample {0}" -f $sample)
        $fileMap = $fileNowMap
    }
}

$processFinal = Get-TargetProcessSnapshot
$netFinal = Get-NetSnapshot -TargetPids (Get-TargetPids -ProcessSnapshot $processFinal)
$registryFinal = Get-RegistrySnapshot -Roots $registryRoots
$fileFinal = Get-FileSnapshot -Roots $dataRoots

Export-CsvSafe -Path (Join-Path $OutDir "processes-final.csv") -Items $processFinal
Export-CsvSafe -Path (Join-Path $OutDir "net-final.csv") -Items $netFinal
Export-CsvSafe -Path (Join-Path $OutDir "registry-final.csv") -Items $registryFinal
Export-CsvSafe -Path (Join-Path $OutDir "files-final.csv") -Items $fileFinal

Write-Log "Capture finished"

try {
    Stop-Transcript | Out-Null
} catch {
}
