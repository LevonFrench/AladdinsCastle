# Synthetic refusal checks. Never reaches executable access or process launch.
$ErrorActionPreference = 'Stop'
$privateRoot = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../../.local'))
$fixture = Join-Path $privateRoot ('synthetic-framerate-gates-' + [Guid]::NewGuid().ToString('N'))
New-Item -ItemType Directory -Path $fixture | Out-Null
$leasePath = Join-Path $fixture 'lease.json'
$runPath = Join-Path $fixture 'run.json'
$sentinel = Join-Path $fixture 'execution.json'
@{game_id='RBUP08';experiment='baseline'} | ConvertTo-Json | Set-Content -LiteralPath $runPath
'{"synthetic":true}' | Set-Content -LiteralPath $sentinel

function CheckRefusal([hashtable]$Lease, [string]$Expected) {
    $Lease | ConvertTo-Json | Set-Content -LiteralPath $leasePath
    $failure = $null
    try {
        & (Join-Path $PSScriptRoot 'collect-phase1.ps1') -RunDirectory $fixture -LeaseFile $leasePath -Seconds 10
    } catch { $failure = $_.Exception.Message }
    if (-not $failure -or $failure -notlike $Expected) {
        throw "Expected '$Expected'; got '$failure'"
    }
}

$lease = @{quiet_machine=$true;desktop=$true;granted_by='lead';run_directory=$fixture;
    expires_utc=[DateTimeOffset]::UtcNow.AddMinutes(5).ToString('o')}
# A neutral lead marker reaches the already-attempted sentinel, before any executable access.
CheckRefusal $lease '*Run already attempted*'
foreach ($field in @('quiet_machine','desktop')) {
    $changed = $lease.Clone()
    $changed[$field] = $false
    CheckRefusal $changed '*Missing or expired*'
}
$changed = $lease.Clone()
$changed.granted_by = 'unassigned'
CheckRefusal $changed '*Missing or expired*'
$changed = $lease.Clone()
$changed.expires_utc = [DateTimeOffset]::UtcNow.AddMinutes(-1).ToString('o')
CheckRefusal $changed '*Missing or expired*'
$changed = $lease.Clone()
$changed.run_directory = Join-Path $fixture 'another-run'
CheckRefusal $changed '*Lease does not name this exact run directory*'

# Remove only this newly created exact synthetic sentinel, then check the predecessor gate.
Remove-Item -LiteralPath $sentinel
@{game_id='RBUP08';experiment='code'} | ConvertTo-Json | Set-Content -LiteralPath $runPath
CheckRefusal $lease '*Legacy candidate-code runtime parked under R3*'

@{game_id='RBUP08';experiment='native-off';route='native-vbi';forced_vbi_value=0;input_contract=$null} |
    ConvertTo-Json | Set-Content -LiteralPath $runPath
$nativeLease = $lease.Clone()
$nativeLease.desktop = $false
$nativeLease.runtime_slot = $true
CheckRefusal $nativeLease '*lead-approved XR session conditions*'
$nativeLease.xr_session_authorized = $true
CheckRefusal $nativeLease '*requires verified scripted input*'
@{game_id='RZJE69';experiment='deadspace-code-candidate'} | ConvertTo-Json | Set-Content -LiteralPath $runPath
CheckRefusal $lease '*Outside bounded Chronicles Phase 1 scope*'

# Mock only process discovery; exercise the real collector's active-build refusal.
# The baseline fixture has no executable field, so a missed build cannot launch.
@{game_id='RBUP08';experiment='baseline'} | ConvertTo-Json | Set-Content -LiteralPath $runPath
function Get-Process {
    [CmdletBinding()]
    param([string[]]$Name, [int]$Id)
    if ($PSBoundParameters.ContainsKey('Id')) { return $mockDsuProducer }
    if (-not $Name) { throw 'Synthetic check reached unexpected process inventory' }
    foreach ($pattern in $Name) {
        if ($mockProcessName -like $pattern) {
            return [pscustomobject]@{ProcessName=$mockProcessName}
        }
    }
}
$mockProcessNames = @(
    'cmake','MSBuild','ninja','cl','link','blender','Dolphin',
    'gcc','g++','cc1','cc1plus','make','gmake','mingw32-make',
    'clang','clang++','clang-cl','clang-cpp','ld','ld.lld','lld','lld-link',
    'x86_64-w64-mingw32-gcc','x86_64-w64-mingw32-g++',
    'gcc-14','g++-14','clang-19','clang++-19'
)
foreach ($mockProcessName in $mockProcessNames) {
    CheckRefusal $lease '*A build/render/Dolphin process is active*'
}
$mockProcessName = 'idle-helper'
$syntheticExe = Join-Path $fixture 'never-executed.fixture'
'synthetic data, not an executable' | Set-Content -LiteralPath $syntheticExe
@{game_id='RBUP08';experiment='baseline';executable=$syntheticExe} | ConvertTo-Json | Set-Content -LiteralPath $runPath
CheckRefusal $lease '*Pinned executable changed*'
Write-Output "7 lease/legacy-code, $($mockProcessNames.Count) active-build and 1 idle-control checks passed; no application launched."

. (Join-Path $PSScriptRoot 'collector-common.ps1')
# A matching synthetic interpreter can only reach the active-build sentinel, never launch.
$digest = (Get-FileHash -LiteralPath $syntheticExe -Algorithm SHA256).Hash.ToLowerInvariant()
$inputContract = @{ready_protocol=1;session_token=('a'*32);script_sha256=$digest;
    producer_executable=$syntheticExe;producer_executable_sha256=$digest;port=26760;
    script=$syntheticExe;entrypoint=$syntheticExe;entrypoint_sha256=$digest}
$readyPath = Join-Path $fixture 'ready.json'
$ready = @{ready=$true;port=26760;producer_pid=2222;session_token=('a'*32);script_sha256=$digest}
$nativeLease.dsu_ready_file = $readyPath
$nativeLease.dsu_server_pid = 2222
$nativeLease.dsu_session_token = 'a'*32
$mockDsuProducer = [pscustomobject]@{Id=2222;Path=$syntheticExe}
@{game_id='RBUP08';experiment='native-off';route='native-vbi';forced_vbi_value=0;input_contract=$inputContract} |
    ConvertTo-Json -Depth 5 | Set-Content -LiteralPath $runPath
# The previous gate accepted this old ready receipt with any live lease PID.
@{ready=$true;port=26760} | ConvertTo-Json -Compress | Set-Content -LiteralPath $readyPath
CheckRefusal $nativeLease '*does not bind this run*'
$ready | ConvertTo-Json -Compress | Set-Content -LiteralPath $readyPath
$changed = $nativeLease.Clone()
$changed.dsu_server_pid = 3333
$mockDsuProducer = [pscustomobject]@{Id=3333;Path=$syntheticExe}
CheckRefusal $changed '*does not bind this run*'
$mockDsuProducer = [pscustomobject]@{Id=2222;Path=$syntheticExe}
$mockProcessName = 'cmake'
CheckRefusal $nativeLease '*A build/render/Dolphin process is active*'

$bindingRefusals = 0
function CheckBindingRefusal($ReadyValue, $InputValue, $LeaseValue, $ProducerValue, $HashValue) {
    $failure = $null
    try { Assert-AcPhase1DsuBinding $ReadyValue $InputValue $LeaseValue $ProducerValue $HashValue }
    catch { $failure = $_.Exception.Message }
    if (-not $failure) { throw 'Mismatched DSU binding was accepted' }
    $script:bindingRefusals++
}
foreach ($field in @('ready','port','producer_pid','session_token','script_sha256')) {
    $bad = $ready.Clone()
    $bad.Remove($field)
    CheckBindingRefusal $bad $inputContract $nativeLease $mockDsuProducer $digest
}
foreach ($pair in @(@('ready','true'),@('port',26761),@('producer_pid',3333),
        @('session_token',('b'*32)),@('script_sha256',('b'*64)))) {
    $bad = $ready.Clone()
    $bad[$pair[0]] = $pair[1]
    CheckBindingRefusal $bad $inputContract $nativeLease $mockDsuProducer $digest
}
foreach ($field in @('dsu_server_pid','dsu_session_token')) {
    $bad = $nativeLease.Clone()
    $bad.Remove($field)
    CheckBindingRefusal $ready $inputContract $bad $mockDsuProducer $digest
}
foreach ($field in @('ready_protocol','session_token','producer_executable','producer_executable_sha256')) {
    $bad = $inputContract.Clone()
    $bad.Remove($field)
    CheckBindingRefusal $ready $bad $nativeLease $mockDsuProducer $digest
}
CheckBindingRefusal $ready $inputContract $nativeLease $null $digest
CheckBindingRefusal $ready $inputContract $nativeLease ([pscustomobject]@{Id=3333;Path=$syntheticExe}) $digest
CheckBindingRefusal $ready $inputContract $nativeLease ([pscustomobject]@{Id=2222;Path=(Join-Path $fixture 'other.exe')}) $digest
CheckBindingRefusal $ready $inputContract $nativeLease $mockDsuProducer ('b'*64)
Assert-AcPhase1DsuBinding $ready $inputContract $nativeLease $mockDsuProducer $digest
Write-Output "DSU binding: 2 real-collector stale/unrelated-PID refusals, $bindingRefusals mismatch refusals and matching controls passed; no process launched."

$modes = @{'native-off'=0;'native-auto'=-1;'native-72'=72;'native-90'=90;'native-120'=120}
foreach ($mode in $modes.Keys) {
    $argsList = @(Get-AcPhase1Arguments ([pscustomobject]@{experiment=$mode;disc='synthetic-disc.fixture'}) 'synthetic-user')
    foreach ($value in @("Graphics.VR.ForcedVBIFrequency=$($modes[$mode])",'Graphics.VR.EnableOpenXR=True',
        'Graphics.VR.AutoVBIFromHMD=False','Dolphin.Core.EnableCheats=False','Dolphin.Core.VIOverclockEnable=False')) {
        if ($value -notin $argsList) { throw "Missing native setting: $value" }
    }
    if (@($argsList | Where-Object { $_ -like 'GFX.*' }).Count) { throw 'Unknown graphics CLI namespace emitted' }
}
$quoted = Join-AcPhase1Arguments @('test path\','x=y')
if ($quoted -ne '"test path\\" "x=y"') { throw 'Windows native argument quoting failed' }
if ((Get-AcPhase1ByteHash ([byte[]]@(97,98,99))) -ne 'ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad') { throw 'Portable byte hash failed' }
Write-Output '2 native admission and 1 disabled-code refusal, 5 native argument modes, quoting and byte-hash checks passed.'
