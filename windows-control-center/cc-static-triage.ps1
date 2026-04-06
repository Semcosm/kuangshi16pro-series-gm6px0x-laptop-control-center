[CmdletBinding()]
param(
    [string]$PackageName = 'ControlCenter3',
    [string]$InstallLocation = '',
    [string]$OutputRoot = ''
)

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest

function Get-AppxInfo {
    param([string]$Name)
    try {
        Get-AppxPackage -Name $Name -ErrorAction Stop | Select-Object -First 1
    } catch {
        $null
    }
}

function Get-AsciiStrings {
    param([byte[]]$Bytes, [int]$MinLen = 4)
    $sb = New-Object System.Text.StringBuilder
    $res = New-Object System.Collections.Generic.List[string]
    foreach ($b in $Bytes) {
        if ($b -ge 32 -and $b -le 126) {
            [void]$sb.Append([char]$b)
        } else {
            if ($sb.Length -ge $MinLen) { $res.Add($sb.ToString()) }
            $sb.Clear() | Out-Null
        }
    }
    if ($sb.Length -ge $MinLen) { $res.Add($sb.ToString()) }
    $res
}

function Get-Utf16LeStrings {
    param([byte[]]$Bytes, [int]$MinLen = 4)
    $res = New-Object System.Collections.Generic.List[string]
    $chars = New-Object System.Collections.Generic.List[char]
    for ($i = 0; $i -lt $Bytes.Length - 1; $i += 2) {
        $code = [BitConverter]::ToUInt16($Bytes, $i)
        if ($code -ge 32 -and $code -le 126) {
            $chars.Add([char]$code)
        } else {
            if ($chars.Count -ge $MinLen) {
                $res.Add((-join $chars))
            }
            $chars.Clear()
        }
    }
    if ($chars.Count -ge $MinLen) { $res.Add((-join $chars)) }
    $res
}

if (-not $InstallLocation) {
    $pkg = Get-AppxInfo -Name $PackageName
    if (-not $pkg) { throw "Package '$PackageName' not found." }
    $InstallLocation = $pkg.InstallLocation
}
if (-not (Test-Path $InstallLocation)) { throw "InstallLocation not found: $InstallLocation" }

$ts = Get-Date -Format 'yyyyMMdd-HHmmss'
if (-not $OutputRoot) {
    $OutputRoot = Join-Path (Get-Location) "cc-static-triage-$ts"
}
New-Item -ItemType Directory -Path $OutputRoot -Force | Out-Null

$targets = Get-ChildItem -LiteralPath $InstallLocation -Recurse -File |
    Where-Object {
        $_.Extension -in '.exe','.dll','.json','.xml' -and
        ($_.Name -match 'GamingCenter|ControlCenter|Systray|Cross|fan|thermal|oem' -or $_.Extension -in '.json','.xml')
    }

$hashes = foreach ($f in $targets) {
    try {
        $h = Get-FileHash -Algorithm SHA256 -LiteralPath $f.FullName
        [pscustomobject]@{
            FullName = $f.FullName
            Length   = $f.Length
            LastWriteTimeUtc = $f.LastWriteTimeUtc
            SHA256   = $h.Hash
        }
    } catch {}
}
$hashes | Export-Csv -NoTypeInformation -Encoding UTF8 -LiteralPath (Join-Path $OutputRoot 'hashes.csv')

$keywords = @(
    '\\.\\', 'DeviceIoControl', 'CreateFile', 'IOCTL', 'ACPI', 'WMI', 'ROOT\\WMI',
    'EC', 'EmbeddedController', 'Thermal', 'Fan', 'PWM', 'PowerMode', 'Performance',
    'GamingCenter', 'Systray', 'OpenHardwareMonitor', 'NvAPI', 'ADLX', 'GetSystemFirmwareTable'
)

$hits = New-Object System.Collections.Generic.List[object]
foreach ($f in $targets) {
    try {
        $bytes = [IO.File]::ReadAllBytes($f.FullName)
        $strings = @()
        if ($f.Extension -in '.exe','.dll') {
            $strings += Get-AsciiStrings -Bytes $bytes -MinLen 4
            $strings += Get-Utf16LeStrings -Bytes $bytes -MinLen 4
        } else {
            $strings += [IO.File]::ReadAllLines($f.FullName)
        }
        $strings = $strings | Select-Object -Unique
        $base = [IO.Path]::GetFileName($f.FullName)
        $strings | Set-Content -LiteralPath (Join-Path $OutputRoot ($base + '.strings.txt')) -Encoding UTF8
        foreach ($kw in $keywords) {
            foreach ($s in ($strings | Select-String -Pattern $kw -SimpleMatch:$false)) {
                $hits.Add([pscustomobject]@{
                    File    = $f.FullName
                    Keyword = $kw
                    Hit     = $s.Line
                })
            }
        }
    } catch {}
}
$hits | Export-Csv -NoTypeInformation -Encoding UTF8 -LiteralPath (Join-Path $OutputRoot 'keyword-hits.csv')

$manifest = Join-Path $InstallLocation 'AppxManifest.xml'
if (Test-Path $manifest) {
    Copy-Item -LiteralPath $manifest -Destination (Join-Path $OutputRoot 'AppxManifest.xml') -Force
}
$appsettings = Join-Path $InstallLocation 'appsettings.json'
if (Test-Path $appsettings) {
    Copy-Item -LiteralPath $appsettings -Destination (Join-Path $OutputRoot 'appsettings.json') -Force
}

Write-Host "完成。输出目录：$OutputRoot" -ForegroundColor Green
Write-Host '先看 keyword-hits.csv，再看 GamingCenter3_Cross.exe.strings.txt 和 SystrayComponent.exe.strings.txt。' -ForegroundColor Green
