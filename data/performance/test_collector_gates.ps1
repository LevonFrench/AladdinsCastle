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
CheckRefusal $lease '*previous same-stretch normal-speed receipt*'
Write-Output '7 synthetic lease/refusal checks passed; no executable accessed or application launched.'
