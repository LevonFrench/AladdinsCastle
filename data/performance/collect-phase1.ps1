#Requires -Version 5.1
# Run only in the lead's quiet-machine/project-runtime slot. Never sends OS input.
[CmdletBinding()]
param(
    [Parameter(Mandatory)][string]$RunDirectory,
    [Parameter(Mandatory)][string]$LeaseFile,
    [string]$PrivateOutputRoot,
    [ValidateRange(10,240)][int]$Seconds = 120,
    [string]$PredecessorRun
)
$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot 'collector-common.ps1')
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
$run = Get-Content -LiteralPath (Join-Path $runRoot 'run.json') -Raw | ConvertFrom-Json
$nativeModes = @{'native-off'=0;'native-auto'=-1;'native-72'=72;'native-90'=90;'native-120'=120}
$native = $nativeModes.ContainsKey([string]$run.experiment)
$slotGranted = if ($native) { $lease.runtime_slot -eq $true } else { $lease.desktop -eq $true }
if ($lease.quiet_machine -ne $true -or -not $slotGranted -or
    $lease.granted_by -ne 'lead' -or
    [DateTimeOffset]::Parse($lease.expires_utc) -le [DateTimeOffset]::UtcNow.AddSeconds($Seconds + 20)) {
    throw 'Missing or expired lead QUIET MACHINE + DESKTOP grant'
}
if ($lease.run_directory -ne $runRoot) { throw 'Lease does not name this exact run directory' }
if (Test-Path -LiteralPath (Join-Path $runRoot 'execution.json')) { throw 'Run already attempted; prepare a fresh retry profile' }
$planRoot = if ($env:AC_PERFORMANCE_PLAN_DIR) { [IO.Path]::GetFullPath($env:AC_PERFORMANCE_PLAN_DIR) } else { $PSScriptRoot }
$planParts = $planRoot -split '[/\\]'
$privatePlan = $false
for ($i=0; $i -lt $planParts.Count-1; $i++) { if ($planParts[$i] -eq '.local' -and $planParts[$i+1] -ne 'worktrees') { $privatePlan=$true } }
if ($env:AC_PERFORMANCE_PLAN_DIR -and -not $privatePlan) { throw 'Selected run plans must be private under .local' }
$planName = if ($native) { 'native-vbi-phase1.json' } else { 'chronicles-phase1.json' }
$selectedPlan = Get-Content -LiteralPath (Join-Path $planRoot $planName) -Raw | ConvertFrom-Json
$admittedGame = $run.game_id -in @($selectedPlan.games.PSObject.Properties.Name)
if ($native -and $admittedGame) { $admittedGame = $selectedPlan.games.($run.game_id).format -eq 'iso' }
if (-not $admittedGame -or (-not $native -and $run.experiment -notin @('baseline','code','code-vbi90'))) {
    throw 'Outside selected Phase 1 plan'
}
if ($run.experiment -in @('code','code-vbi90')) { throw 'Legacy candidate-code runtime parked under R3; use native VBI without cheats' }
if ($native) {
    if ($lease.xr_session_authorized -ne $true) { throw 'Native VBI requires the lead-approved XR session conditions' }
    if ($run.route -ne 'native-vbi' -or $run.forced_vbi_value -ne $nativeModes[$run.experiment] -or -not $run.input_contract) {
        throw 'Native VBI profile requires verified scripted input before runtime'
    }
    if ($Seconds -gt 180) { throw 'Native VBI attempts are bounded to 180 seconds' }
    foreach ($name in @('script','entrypoint')) {
        $path = $run.input_contract.$name
        if ((Get-FileHash -LiteralPath $path -Algorithm SHA256).Hash.ToLowerInvariant() -ne $run.input_contract.($name + '_sha256')) {
            throw 'Scripted input source or timeline changed'
        }
    }
    $readyPath = PrivatePath $lease.dsu_ready_file
    $ready = Get-Content -LiteralPath $readyPath | Select-Object -First 1 | ConvertFrom-Json
    $producer = $null
    $producerHash = $null
    if (($lease.dsu_server_pid -is [int] -or $lease.dsu_server_pid -is [long]) -and $lease.dsu_server_pid -gt 0) {
        $producer = Get-Process -Id $lease.dsu_server_pid -ErrorAction SilentlyContinue
        if ($producer -and $producer.Path) {
            $producerHash = (Get-FileHash -LiteralPath $producer.Path -Algorithm SHA256).Hash.ToLowerInvariant()
        }
    }
    Assert-AcPhase1DsuBinding $ready $run.input_contract $lease $producer $producerHash
}
if ($native -and $run.experiment -ne 'native-off') {
    if (-not $PredecessorRun) { throw 'Code runs require the previous same-stretch normal-speed receipt' }
    $previous = Get-Content -LiteralPath (Join-Path (PrivatePath $PredecessorRun) 'observations.json') -Raw | ConvertFrom-Json
    $previousExecution = Get-Content -LiteralPath (Join-Path (PrivatePath $PredecessorRun) 'execution.json') -Raw | ConvertFrom-Json
    $expected = 'native-off'
    if ($previous.game_id -ne $run.game_id -or $previous.revision -ne $run.revision -or
        $previous.experiment -ne $expected -or $previous.counter_confirmed -ne $true -or
        $previous.normal_speed_verified -ne $true -or $previous.shader_warmup_discarded -ne $true -or
        -not $previous.stretch -or -not $previous.counter_evidence -or
        $previous.busiest_core_full -ne $false -or $previousExecution.owned_process_absent -ne $true -or
        $previousExecution.game_id -ne $run.game_id -or $previousExecution.revision -ne $run.revision -or
        $previousExecution.executable_sha256 -ne $run.executable_sha256 -or
        $previousExecution.input_script_sha256 -ne $run.input_contract.script_sha256 -or
        $previousExecution.dsu_profile_sha256 -ne $run.input_contract.profile_sha256) { throw 'Predecessor did not establish same-input counters, scene and normal speed with CPU headroom' }
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
$headerHash = Get-AcPhase1ByteHash $header
if ($headerHash -ne $run.header_sha256 -or (Get-Item -LiteralPath $run.disc).Length -ne $run.disc_bytes) {
    throw 'Disc differs from preparation snapshot'
}
# Fixed launch parameters come from validated experiment, not editable run.arguments.
$argsList = @(Get-AcPhase1Arguments $run $userRoot)
$receipt = [ordered]@{ status='starting'; start_utc=[DateTimeOffset]::UtcNow.ToString('o');
    game_id=$run.game_id; revision=$run.revision; experiment=$run.experiment;
    release=$run.release; executable_sha256=$pin.executable_sha256; lease=$lease;
    input_script_sha256=$(if ($native) { $run.input_contract.script_sha256 } else { $null });
    dsu_profile_sha256=$(if ($native) { $run.input_contract.profile_sha256 } else { $null });
    dsu_producer_pid=$(if ($native) { $ready.producer_pid } else { $null });
    dsu_session_token=$(if ($native) { $ready.session_token } else { $null });
    dsu_producer_executable_sha256=$(if ($native) { $producerHash } else { $null });
    required_stretch=$(if ($previous) { $previous.stretch } else { $null });
    other_processes=@(Get-Process | Select-Object ProcessName,Id,CPU); telemetry_errors=@();
    stop_reason='bounded-duration'; owned_process_absent=$false }
$receiptPath = Join-Path $runRoot 'execution.json'
$receipt | ConvertTo-Json -Depth 8 | Set-Content -LiteralPath $receiptPath -Encoding utf8
$process = $null
$cpuRows = [Collections.Generic.List[object]]::new()
$gpuRows = [Collections.Generic.List[object]]::new()
try {
    $start = New-Object Diagnostics.ProcessStartInfo
    $start.FileName = $run.executable
    $start.WorkingDirectory = $runRoot
    $start.UseShellExecute = $false
    $start.Arguments = Join-AcPhase1Arguments $argsList
    $process = [Diagnostics.Process]::Start($start)
    $receipt.pid = $process.Id
    $receipt.status = 'running'
    $clock = [Diagnostics.Stopwatch]::StartNew()
    $fullSamples = 0
    while ($clock.Elapsed.TotalSeconds -lt $Seconds) {
        $process.Refresh()
        if ($process.HasExited) { $receipt.stop_reason='application-exited'; break }
        if (Test-Path -LiteralPath (Join-Path $runRoot 'stop-requested.txt')) {
            $receipt.stop_reason='lead-stop-requested'; break
        }
        $elapsed = $clock.Elapsed.TotalSeconds
        try {
            $cores = @(Get-CimInstance Win32_PerfFormattedData_PerfOS_Processor | Where-Object Name -ne '_Total')
            if (-not $cores.Count) { throw 'No logical-core counters available' }
            foreach ($core in $cores) {
                $cpuRows.Add([pscustomobject]@{elapsed_s=$elapsed; core=$core.Name; percent=$core.PercentProcessorTime})
            }
            $busy = ($cores | Measure-Object PercentProcessorTime -Maximum).Maximum
            $fullSamples = if ($busy -ge 95) { $fullSamples + 1 } else { 0 }
            if ($run.experiment -notin @('baseline','native-off') -and $fullSamples -ge 3) {
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
