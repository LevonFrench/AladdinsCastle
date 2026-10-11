# Run only after the lead grants BOTH QUIET MACHINE and DESKTOP.
[CmdletBinding()]
param(
    [Parameter(Mandatory)][string]$RunDirectory,
    [Parameter(Mandatory)][string]$LeaseFile,
    [string]$PrivateOutputRoot,
    [ValidateRange(10,240)][int]$Seconds = 120,
    [string]$PredecessorRun
)
$ErrorActionPreference = 'Stop'
$defaultRoot = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../../.local'))
$selectedRoot = if ($PrivateOutputRoot) { [IO.Path]::GetFullPath($PrivateOutputRoot) } else { $defaultRoot }
if ($selectedRoot -ne $defaultRoot -and
    ((Split-Path -Leaf $selectedRoot) -ne 'framerate' -or '.local' -notin ($selectedRoot -split '[/\\]'))) {
    throw 'External private root must be a framerate directory under .local'
}
$privateRoot = $selectedRoot.TrimEnd([IO.Path]::DirectorySeparatorChar) + [IO.Path]::DirectorySeparatorChar
function PrivatePath([string]$Value) {
    $resolved = (Resolve-Path -LiteralPath $Value).Path
    if (-not $resolved.StartsWith($privateRoot, [StringComparison]::OrdinalIgnoreCase)) {
        throw 'All run/lease/receipt paths must be inside the designated private root'
    }
    return $resolved
}
$runRoot = PrivatePath $RunDirectory
$lease = Get-Content -LiteralPath (PrivatePath $LeaseFile) -Raw | ConvertFrom-Json
if ($lease.quiet_machine -ne $true -or $lease.desktop -ne $true -or
    $lease.granted_by -ne 'lead' -or
    [DateTimeOffset]::Parse($lease.expires_utc) -le [DateTimeOffset]::UtcNow.AddSeconds($Seconds + 20)) {
    throw 'Missing or expired lead QUIET MACHINE + DESKTOP grant'
}
$run = Get-Content -LiteralPath (Join-Path $runRoot 'run.json') -Raw | ConvertFrom-Json
if ($lease.run_directory -ne $runRoot) { throw 'Lease does not name this exact run directory' }
if (Test-Path -LiteralPath (Join-Path $runRoot 'execution.json')) { throw 'Run already attempted; prepare a fresh retry profile' }
if ($run.game_id -notin @('RBUP08','SBDP08') -or $run.experiment -notin @('baseline','code','code-vbi90')) {
    throw 'Outside bounded Chronicles Phase 1 scope'
}
if ($run.experiment -ne 'baseline') {
    if (-not $PredecessorRun) { throw 'Code runs require the previous same-stretch normal-speed receipt' }
    $previous = Get-Content -LiteralPath (Join-Path (PrivatePath $PredecessorRun) 'observations.json') -Raw | ConvertFrom-Json
    $previousExecution = Get-Content -LiteralPath (Join-Path (PrivatePath $PredecessorRun) 'execution.json') -Raw | ConvertFrom-Json
    $expected = if ($run.experiment -eq 'code') { 'baseline' } else { 'code' }
    if ($previous.game_id -ne $run.game_id -or $previous.revision -ne $run.revision -or
        $previous.experiment -ne $expected -or $previous.counter_confirmed -ne $true -or
        $previous.normal_speed_verified -ne $true -or $previous.shader_warmup_discarded -ne $true -or
        -not $previous.stretch -or -not $previous.counter_evidence -or
        $previous.busiest_core_full -ne $false -or $previousExecution.owned_process_absent -ne $true -or
        $previousExecution.game_id -ne $run.game_id -or $previousExecution.revision -ne $run.revision -or
        $previousExecution.executable_sha256 -ne $run.executable_sha256) { throw 'Predecessor did not establish counters, same stretch and normal speed with CPU headroom' }
}
$buildProcessNames = @(
    'cmake','MSBuild','ninja','cl','link','blender','Dolphin',
    'gcc','g++','cc1','cc1plus','make','gmake','mingw32-make',
    'clang','clang++','clang-cl','clang-cpp','ld','ld.lld','lld','lld-link',
    '*-gcc','*-g++','gcc-[0-9]*','g++-[0-9]*','clang-[0-9]*','clang++-[0-9]*'
)
$builders = @(Get-Process -Name $buildProcessNames -ErrorAction SilentlyContinue)
if ($builders.Count) { throw 'A build/render/Dolphin process is active: quiet-machine lease cannot be used' }
$pin = Get-Content -LiteralPath (Join-Path $PSScriptRoot '../../setups/dolphin-vr/upstream-pin.json') -Raw | ConvertFrom-Json
if ((Get-FileHash -LiteralPath $run.executable -Algorithm SHA256).Hash.ToLowerInvariant() -ne $pin.executable_sha256) {
    throw 'Pinned executable changed'
}
if ($run.executable_sha256 -ne $pin.executable_sha256) { throw 'Run does not pin the current release' }
$userRoot = PrivatePath $run.user
if ($userRoot -ne (Join-Path $runRoot 'User')) { throw 'Profile belongs to a different run' }
foreach ($property in $run.settings_sha256.PSObject.Properties) {
    $file = PrivatePath (Join-Path $userRoot $property.Name)
    if ((Get-FileHash -LiteralPath $file -Algorithm SHA256).Hash.ToLowerInvariant() -ne $property.Value) {
        throw 'Prepared profile changed; prepare a fresh run'
    }
}
# Validate the exact disc header again; never enumerate the library.
$stream = [IO.File]::OpenRead($run.disc)
try {
    $header = [byte[]]::new(32)
    if ($stream.Read($header,0,32) -ne 32) { throw 'Short disc header' }
} finally { $stream.Dispose() }
$headerHash = [Convert]::ToHexString([Security.Cryptography.SHA256]::HashData($header)).ToLowerInvariant()
if ($headerHash -ne $run.header_sha256 -or (Get-Item -LiteralPath $run.disc).Length -ne $run.disc_bytes) {
    throw 'Disc differs from preparation snapshot'
}
# Fixed launch parameters come from validated experiment, not editable run.arguments.
$argsList = @('--user', $userRoot, '--batch', '--exec', $run.disc,
    '-C','GFX.VR.EnableOpenXR=False', '-C','GFX.Hardware.VSync=False',
    '-C','Dolphin.Core.EmulationSpeed=1.0', '-C','Dolphin.Core.OverclockEnable=False',
    '-C','Dolphin.Core.Overclock=1.0', '-C','SYSCONF.IPL.E60=True', '-C','Dolphin.Core.GFXBackend=D3D')
$code = if ($run.experiment -eq 'baseline') { 'False' } else { 'True' }
$vbi = if ($run.experiment -eq 'code-vbi90') { 'True' } else { 'False' }
$ratio = if ($vbi -eq 'True') { '1.5' } else { '1.0' }
$argsList += @('-C',"Dolphin.Core.EnableCheats=$code",'-C',"Dolphin.Core.VIOverclockEnable=$vbi",'-C',"Dolphin.Core.VIOverclock=$ratio")
$receipt = [ordered]@{ status='starting'; start_utc=[DateTimeOffset]::UtcNow.ToString('o');
    game_id=$run.game_id; revision=$run.revision; experiment=$run.experiment;
    release=$run.release; executable_sha256=$pin.executable_sha256; lease=$lease;
    required_stretch=$(if ($previous) { $previous.stretch } else { $null });
    other_processes=@(Get-Process | Select-Object ProcessName,Id,CPU); telemetry_errors=@();
    stop_reason='bounded-duration'; owned_process_absent=$false }
$receiptPath = Join-Path $runRoot 'execution.json'
$receipt | ConvertTo-Json -Depth 8 | Set-Content -LiteralPath $receiptPath -Encoding utf8
$process = $null
$cpuRows = [Collections.Generic.List[object]]::new()
$gpuRows = [Collections.Generic.List[object]]::new()
try {
    $start = [Diagnostics.ProcessStartInfo]::new($run.executable)
    $start.WorkingDirectory = $runRoot
    $start.UseShellExecute = $false
    foreach ($arg in $argsList) { $start.ArgumentList.Add($arg) }
    $process = [Diagnostics.Process]::Start($start)
    $receipt.pid = $process.Id
    $receipt.status = 'running'
    $clock = [Diagnostics.Stopwatch]::StartNew()
    $fullSamples = 0
    while ($clock.Elapsed.TotalSeconds -lt $Seconds) {
        $process.Refresh()
        if ($process.HasExited) { $receipt.stop_reason='application-exited'; break }
        $elapsed = $clock.Elapsed.TotalSeconds
        try {
            $cores = @(Get-CimInstance Win32_PerfFormattedData_PerfOS_Processor | Where-Object Name -ne '_Total')
            if (-not $cores.Count) { throw 'No logical-core counters available' }
            foreach ($core in $cores) {
                $cpuRows.Add([pscustomobject]@{elapsed_s=$elapsed; core=$core.Name; percent=$core.PercentProcessorTime})
            }
            $busy = ($cores | Measure-Object PercentProcessorTime -Maximum).Maximum
            $fullSamples = if ($busy -ge 95) { $fullSamples + 1 } else { 0 }
            if ($run.experiment -ne 'baseline' -and $fullSamples -ge 3) {
                $receipt.stop_reason='busiest-logical-core-at-least-95-percent-for-three-samples'; break
            }
        } catch {
            $receipt.telemetry_errors += "CPU unavailable: $($_.Exception.Message)"
            $receipt.stop_reason='CPU-counter-unavailable'; break
        }
        try {
            $engines = @(Get-CimInstance Win32_PerfFormattedData_GPUPerformanceCounters_GPUEngine | Where-Object Name -Like "pid_$($process.Id)_*")
            foreach ($engine in $engines) {
                $gpuRows.Add([pscustomobject]@{elapsed_s=$elapsed; engine=$engine.Name; percent=$engine.UtilizationPercentage})
            }
        } catch { if (-not $receipt.telemetry_errors.Count) { $receipt.telemetry_errors += 'GPU engine counters unavailable' } }
        Start-Sleep -Milliseconds 1000
    }
} catch {
    $receipt.status = 'failed'
    $receipt.failure = $_.Exception.Message
    throw
} finally {
    if ($process -and -not $process.HasExited) {
        $null = $process.CloseMainWindow()
        if (-not $process.WaitForExit(10000)) { $process.Kill(); $process.WaitForExit() }
    }
    $receipt.owned_process_absent = -not $process -or $process.HasExited
    $receipt.end_utc = [DateTimeOffset]::UtcNow.ToString('o')
    if ($receipt.status -ne 'failed') { $receipt.status = 'collected-unreviewed' }
    $receipt | ConvertTo-Json -Depth 8 | Set-Content -LiteralPath $receiptPath -Encoding utf8
    $cpuRows | Export-Csv -LiteralPath (Join-Path $runRoot 'logical-core.csv') -NoTypeInformation
    $gpuRows | Export-Csv -LiteralPath (Join-Path $runRoot 'gpu-engines.csv') -NoTypeInformation
}
Write-Output "Stopped: $($receipt.stop_reason). Review same-stretch counters and game speed before another experiment."
