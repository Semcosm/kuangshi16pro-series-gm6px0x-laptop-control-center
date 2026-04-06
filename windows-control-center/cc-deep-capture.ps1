[CmdletBinding()]
param(
    [string]$PackageName = 'ControlCenter3',
    [int]$DurationSec = 25,
    [int]$SampleMs = 250,
    [string]$OutputRoot = '',
    [string]$ProcmonPath = '',
    [string[]]$WatchPorts = @('13688','20122','20123','49350'),
    [switch]$TryExportCsv
)

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest

function Test-Admin {
    $id = [Security.Principal.WindowsIdentity]::GetCurrent()
    $p = New-Object Security.Principal.WindowsPrincipal($id)
    return $p.IsInRole([Security.Principal.WindowsBuiltInRole]::Administrator)
}

function Resolve-ProcmonPath {
    param([string]$Given)
    $candidates = @()
    if ($Given) { $candidates += $Given }
    $scriptDir = Split-Path -Parent $PSCommandPath
    $candidates += @(
        (Join-Path $scriptDir 'Procmon64.exe'),
        (Join-Path $scriptDir 'Procmon.exe'),
        'C:\Tools\Procmon\Procmon64.exe',
        'C:\Tools\Procmon\Procmon.exe',
        'C:\Sysinternals\Procmon64.exe',
        'C:\Sysinternals\Procmon.exe'
    )
    foreach ($c in $candidates) {
        if ($c -and (Test-Path $c)) { return (Resolve-Path $c).Path }
    }
    return $null
}

function Get-AppxInfo {
    param([string]$Name)
    try {
        $pkg = Get-AppxPackage -Name $Name -ErrorAction Stop | Select-Object -First 1
        return $pkg
    } catch {
        return $null
    }
}

function Write-Text {
    param([string]$Path, [string]$Text)
    $dir = Split-Path -Parent $Path
    if ($dir -and -not (Test-Path $dir)) { New-Item -ItemType Directory -Path $dir -Force | Out-Null }
    [IO.File]::WriteAllText($Path, $Text, [Text.UTF8Encoding]::new($false))
}

function Append-Line {
    param([string]$Path, [string]$Line)
    Add-Content -LiteralPath $Path -Value $Line -Encoding UTF8
}

function Save-Csv {
    param($InputObject, [string]$Path)
    $InputObject | Export-Csv -NoTypeInformation -Encoding UTF8 -LiteralPath $Path
}

function Get-TargetProcesses {
    Get-CimInstance Win32_Process |
        Where-Object {
            $_.Name -match 'ControlCenter|GamingCenter|SystrayComponent' -or
            $_.ExecutablePath -match 'ControlCenter3|GamingCenter3|SystrayComponent'
        } |
        Select-Object ProcessId, ParentProcessId, Name, ExecutablePath, CommandLine, CreationDate
}

function Get-NetSnapshot {
    param([int[]]$TargetPids, [string[]]$WatchPorts)
    $targets = @($TargetPids | Where-Object { $_ -gt 0 } | Select-Object -Unique)
    $cons = @()
    try {
        $all = Get-NetTCPConnection -ErrorAction Stop
    } catch {
        return @()
    }
    foreach ($c in $all) {
        $include = $false
        if ($targets -contains $c.OwningProcess) { $include = $true }
        if ($c.LocalAddress -in @('127.0.0.1','::1') -or $c.RemoteAddress -in @('127.0.0.1','::1')) { $include = $true }
        if ($WatchPorts -contains ([string]$c.LocalPort) -or $WatchPorts -contains ([string]$c.RemotePort)) { $include = $true }
        if (-not $include) { continue }
        $pname = ''
        try { $pname = (Get-Process -Id $c.OwningProcess -ErrorAction Stop).ProcessName } catch {}
        $cons += [pscustomobject]@{
            Time        = (Get-Date).ToString('o')
            State       = $c.State
            Local       = "$($c.LocalAddress):$($c.LocalPort)"
            Remote      = "$($c.RemoteAddress):$($c.RemotePort)"
            OwningPid   = $c.OwningProcess
            ProcessName = $pname
        }
    }
    return $cons | Sort-Object OwningPid, Local, Remote, State
}

function Get-ModuleSnapshot {
    param([int[]]$TargetPids)
    $mods = @()
    foreach ($targetPid in ($TargetPids | Select-Object -Unique)) {
        try {
            $p = Get-Process -Id $targetPid -ErrorAction Stop
            foreach ($m in $p.Modules) {
                $mods += [pscustomobject]@{
                    Pid         = $targetPid
                    ProcessName = $p.ProcessName
                    ModuleName  = $m.ModuleName
                    FileName    = $m.FileName
                    FileVersion = $m.FileVersionInfo.FileVersion
                    ProductName = $m.FileVersionInfo.ProductName
                }
            }
        } catch {}
    }
    return $mods | Sort-Object Pid, ModuleName
}

function Get-InterestingDrivers {
    $patterns = 'oem|mechrevo|control|gaming|center|wmi|acpi|fan|thermal|ec'
    try {
        Get-CimInstance Win32_SystemDriver |
            Where-Object {
                $_.Name -match $patterns -or
                $_.DisplayName -match $patterns -or
                $_.PathName -match $patterns
            } |
            Select-Object Name, DisplayName, State, StartMode, PathName, Description |
            Sort-Object Name
    } catch {
        @()
    }
}

function New-DeltaMap {
    param($Rows, [string[]]$Keys)
    $map = @{}
    foreach ($r in $Rows) {
        $k = ($Keys | ForEach-Object { [string]($r.$_) }) -join '|'
        $map[$k] = $r
    }
    $map
}

if (-not (Test-Admin)) {
    throw '请右键 PowerShell -> 以管理员身份运行，再执行本脚本。否则 Procmon/模块/驱动信息会不完整。'
}

$procmon = Resolve-ProcmonPath -Given $ProcmonPath
if (-not $procmon) {
    throw '没找到 Procmon64.exe / Procmon.exe。把 Procmon64.exe 放到脚本同目录，或用 -ProcmonPath 指定。'
}

$timestamp = Get-Date -Format 'yyyyMMdd-HHmmss'
if (-not $OutputRoot) {
    $OutputRoot = Join-Path (Get-Location) "cc-deep-capture-$timestamp"
}
New-Item -ItemType Directory -Path $OutputRoot -Force | Out-Null

$logPath          = Join-Path $OutputRoot 'capture.log'
$procmonPml       = Join-Path $OutputRoot 'procmon.pml'
$procmonCsv       = Join-Path $OutputRoot 'procmon.csv'
$sysPath          = Join-Path $OutputRoot 'system.txt'
$pkgPath          = Join-Path $OutputRoot 'package.txt'
$procBasePath     = Join-Path $OutputRoot 'processes-baseline.csv'
$procFinalPath    = Join-Path $OutputRoot 'processes-final.csv'
$netBasePath      = Join-Path $OutputRoot 'net-baseline.csv'
$netFinalPath     = Join-Path $OutputRoot 'net-final.csv'
$modsBasePath     = Join-Path $OutputRoot 'modules-baseline.csv'
$modsFinalPath    = Join-Path $OutputRoot 'modules-final.csv'
$driversPath      = Join-Path $OutputRoot 'interesting-drivers.csv'
$sampleNetPath    = Join-Path $OutputRoot 'net-samples.csv'
$sampleProcPath   = Join-Path $OutputRoot 'process-samples.csv'
$sampleEventsPath = Join-Path $OutputRoot 'sample-events.log'
$opsPath          = Join-Path $OutputRoot 'HOW-TO.txt'

Append-Line $logPath "[$(Get-Date -Format o)] Output directory: $OutputRoot"
Append-Line $logPath "[$(Get-Date -Format o)] ProcmonPath: $procmon"
Append-Line $logPath "[$(Get-Date -Format o)] DurationSec=$DurationSec SampleMs=$SampleMs"

$sys = [ordered]@{
    ComputerName       = $env:COMPUTERNAME
    UserName           = "$env:USERDOMAIN\$env:USERNAME"
    IsAdmin            = $true
    OSCaption          = (Get-CimInstance Win32_OperatingSystem).Caption
    OSVersion          = (Get-CimInstance Win32_OperatingSystem).Version
    BuildNumber        = (Get-CimInstance Win32_OperatingSystem).BuildNumber
    SystemVendor       = (Get-CimInstance Win32_ComputerSystem).Manufacturer
    SystemProductName  = (Get-CimInstance Win32_ComputerSystemProduct).Name
    BaseBoardProduct   = (Get-CimInstance Win32_BaseBoard).Product
    BaseBoardVendor    = (Get-CimInstance Win32_BaseBoard).Manufacturer
    StartedAt          = (Get-Date).ToString('o')
}
$sys.GetEnumerator() | ForEach-Object { "{0}={1}" -f $_.Key, $_.Value } | Set-Content -Path $sysPath -Encoding UTF8

$pkg = Get-AppxInfo -Name $PackageName
if ($pkg) {
    $pkgText = @(
        "Name: $($pkg.Name)",
        "PackageFamilyName: $($pkg.PackageFamilyName)",
        "Version: $($pkg.Version)",
        "Publisher: $($pkg.Publisher)",
        "InstallLocation: $($pkg.InstallLocation)"
    ) -join [Environment]::NewLine
    Write-Text -Path $pkgPath -Text $pkgText
    if ($pkg.InstallLocation -and (Test-Path $pkg.InstallLocation)) {
        Get-ChildItem -LiteralPath $pkg.InstallLocation -Force |
            Select-Object FullName, Name, Length, LastWriteTimeUtc, Attributes |
            Export-Csv -NoTypeInformation -Encoding UTF8 -LiteralPath (Join-Path $OutputRoot 'install-top.csv')
    }
} else {
    Write-Text -Path $pkgPath -Text "Package '$PackageName' not found."
}

Get-InterestingDrivers | Export-Csv -NoTypeInformation -Encoding UTF8 -LiteralPath $driversPath

$procBase = Get-TargetProcesses
Save-Csv $procBase $procBasePath
$basePids = @($procBase.ProcessId | ForEach-Object { [int]$_ } | Select-Object -Unique)

$modsBase = Get-ModuleSnapshot -TargetPids $basePids
Save-Csv $modsBase $modsBasePath

$netBase = Get-NetSnapshot -TargetPids $basePids -WatchPorts $WatchPorts
Save-Csv $netBase $netBasePath

$howto = @"
1. 保持托盘里的笔记本控制中心在后台运行，但先不要打开主界面。
2. 现在脚本会启动 Procmon 后台抓取。
3. 当你看到控制台提示“NOW REPRODUCE”后，只做 1~2 个动作，别乱点：
   A. 打开 Control Center 主界面；
   B. 切一次性能模式（如 均衡 -> 狂暴）；
   C. 切一次风扇模式；
   D. 如果有“自定义风扇曲线”页面，打开一次并保存一次。
4. 20~30 秒内完成，然后什么都别动，等脚本自己结束。
5. 结果目录里最关键的是 procmon.pml、modules-final.csv、net-samples.csv、sample-events.log。
6. 下一轮可以分动作单独抓：
   - 只抓“打开界面”
   - 只抓“切性能模式”
   - 只抓“切风扇模式”
   - 只抓“保存自定义风扇曲线”
"@
Write-Text -Path $opsPath -Text $howto

# Best effort: clear any stray procmon instances first.
try { & $procmon /AcceptEula /Terminate /Quiet | Out-Null } catch {}
Start-Sleep -Milliseconds 800

$procmonArgs = @('/AcceptEula','/BackingFile', $procmonPml, '/Quiet', '/Minimized', '/NoFilter')
Append-Line $logPath "[$(Get-Date -Format o)] Starting Procmon: $($procmonArgs -join ' ')"
$pm = Start-Process -FilePath $procmon -ArgumentList $procmonArgs -PassThru
Start-Sleep -Seconds 2

Write-Host ''
Write-Host '================ NOW REPRODUCE ================' -ForegroundColor Yellow
Write-Host '现在去操作 Control Center：打开主界面 -> 切性能/风扇模式 -> 如有自定义风扇，保存一次。' -ForegroundColor Yellow
Write-Host "脚本会持续抓取 $DurationSec 秒。完成后别再点。" -ForegroundColor Yellow
Write-Host '===============================================' -ForegroundColor Yellow
Write-Host ''

$sampleProc = New-Object System.Collections.Generic.List[object]
$sampleNet  = New-Object System.Collections.Generic.List[object]

$prevProcMap = New-DeltaMap -Rows $procBase -Keys @('ProcessId','Name','ExecutablePath','CommandLine')
$sw = [Diagnostics.Stopwatch]::StartNew()
$sampleNo = 0
while ($sw.Elapsed.TotalSeconds -lt $DurationSec) {
    $sampleNo++
    $now = Get-Date
    $procs = Get-TargetProcesses
    $pids  = @($procs.ProcessId | ForEach-Object { [int]$_ } | Select-Object -Unique)
    foreach ($p in $procs) {
        $sampleProc.Add([pscustomobject]@{
            Time            = $now.ToString('o')
            Sample          = $sampleNo
            ProcessId       = $p.ProcessId
            ParentProcessId = $p.ParentProcessId
            Name            = $p.Name
            ExecutablePath  = $p.ExecutablePath
            CommandLine     = $p.CommandLine
        })
    }

    $procMap = New-DeltaMap -Rows $procs -Keys @('ProcessId','Name','ExecutablePath','CommandLine')
    foreach ($k in $procMap.Keys) {
        if (-not $prevProcMap.ContainsKey($k)) {
            Append-Line $sampleEventsPath "[$($now.ToString('o'))] ADD PROCESS $k"
        }
    }
    foreach ($k in $prevProcMap.Keys) {
        if (-not $procMap.ContainsKey($k)) {
            Append-Line $sampleEventsPath "[$($now.ToString('o'))] DEL PROCESS $k"
        }
    }
    $prevProcMap = $procMap

    $nets = Get-NetSnapshot -TargetPids $pids -WatchPorts $WatchPorts
    foreach ($n in $nets) {
        $n2 = $n | Select-Object *, @{n='Sample';e={$sampleNo}}
        $sampleNet.Add($n2)
    }

    Start-Sleep -Milliseconds $SampleMs
}
$sw.Stop()

Append-Line $logPath "[$(Get-Date -Format o)] Stopping Procmon"
try {
    & $procmon /AcceptEula /Terminate /Quiet | Out-Null
} catch {
    Append-Line $logPath "[$(Get-Date -Format o)] Procmon terminate threw: $($_.Exception.Message)"
}
Start-Sleep -Seconds 3

$procFinal = Get-TargetProcesses
Save-Csv $procFinal $procFinalPath
$finalPids = @($procFinal.ProcessId | ForEach-Object { [int]$_ } | Select-Object -Unique)
$modsFinal = Get-ModuleSnapshot -TargetPids $finalPids
Save-Csv $modsFinal $modsFinalPath
$netFinal  = Get-NetSnapshot -TargetPids $finalPids -WatchPorts $WatchPorts
Save-Csv $netFinal $netFinalPath

$sampleProc | Export-Csv -NoTypeInformation -Encoding UTF8 -LiteralPath $sampleProcPath
$sampleNet  | Export-Csv -NoTypeInformation -Encoding UTF8 -LiteralPath $sampleNetPath

if ($TryExportCsv) {
    try {
        Append-Line $logPath "[$(Get-Date -Format o)] Trying Procmon CSV export"
        & $procmon /AcceptEula /OpenLog $procmonPml /SaveAs $procmonCsv /Quiet | Out-Null
    } catch {
        Append-Line $logPath "[$(Get-Date -Format o)] Procmon CSV export failed: $($_.Exception.Message)"
    }
}

Append-Line $logPath "[$(Get-Date -Format o)] Finished"
Write-Host "完成。输出目录：$OutputRoot" -ForegroundColor Green
Write-Host '先把这些文件发给我：procmon.pml、sample-events.log、net-samples.csv、modules-final.csv、interesting-drivers.csv' -ForegroundColor Green
