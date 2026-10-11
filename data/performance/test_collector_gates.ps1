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
    param([string[]]$Name)
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
$modes = @{'native-off'=0;'native-auto'=-1;'native-72'=72;'native-90'=90;'native-120'=120}
foreach ($mode in $modes.Keys) {
    $argsList = @(Get-AcPhase1Arguments ([pscustomobject]@{experiment=$mode;disc='synthetic-disc.fixture'}) 'synthetic-user')
    foreach ($value in @("Graphics.VR.ForcedVBIFrequency=$($modes[$mode])",'Graphics.VR.EnableOpenXR=True',
        'Graphics.VR.AutoVBIFromHMD=False','Dolphin.Core.EnableCheats=False','Dolphin.Core.VIOverclockEnable=False')) {
        if ($value -notin $argsList) { throw "Missing native setting: $value" }
    }
    if (@($argsList | Where-Object { $_ -like 'GFX.*' }).Count) { throw 'Unknown graphics CLI namespace emitted' }
}
$quoted = Join-AcPhase1Arguments @('C:\test path\','x=y')
if ($quoted -ne '"C:\test path\\" "x=y"') { throw 'Windows native argument quoting failed' }
if ((Get-AcPhase1ByteHash ([byte[]]@(97,98,99))) -ne 'ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad') { throw 'Portable byte hash failed' }
Write-Output '2 native admission and 1 disabled-code refusal, 5 native argument modes, quoting and byte-hash checks passed.'
