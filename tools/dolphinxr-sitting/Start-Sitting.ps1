#Requires -Version 5.1
# SPDX-License-Identifier: GPL-3.0-only
[CmdletBinding()]
param(
    [Parameter(Mandatory=$true)][string]$Executable,
    [Parameter(Mandatory=$true)][string]$ExecutableSHA256,
    [Parameter(Mandatory=$true)][string]$Disc,
    [ValidatePattern('^[A-Z0-9]{6}$')][string]$GameID = 'RBUP08',
    [Parameter(Mandatory=$true)][string]$StateDirectory,
    [string]$NativeProfile = '',
    [string]$ControlSet = '',
    [ValidateSet('Default','Eager','Desktop')][string]$Mode = 'Default',
    [ValidateSet('Native','Sharp','SharpAA')][string]$Quality = 'Sharp',
    [string]$DsuProfile='',
    [ValidateRange(1024,65535)][int]$DsuPort=26760,
    [ValidateSet('Auto','Off','72','90','120')][string]$ForcedVBI='Off',
    [ValidateSet('Both','Left','Right','None')][string]$Mirror='Both',
    [switch]$ControllerAnchor, [switch]$CameraAnchor,
    [ValidateRange(0.0,1.0)][double]$CameraSmoothing=0.85,
    [ValidateRange(0,60)][int]$LaunchDelaySeconds = 15,
    [ValidateRange(10,1800)][int]$MaximumSeconds = 900,
    [switch]$CheckOnly,
    [switch]$PrepareOnly
)
$ErrorActionPreference = 'Stop'
Import-Module (Join-Path $PSScriptRoot 'Sitting.psm1') -Force
Assert-AcSittingInputs $Executable $ExecutableSHA256 $Disc $GameID
$state = Assert-AcPrivatePath $StateDirectory
if ($CheckOnly) { Write-Output 'Executable hash and exact Wii header match. No settings changed or process started.'; return }
if ($ControlSet -and $ControlSet -cnotmatch '^[a-z0-9]+(?:-[a-z0-9]+)*$') { throw 'Invalid control-set identifier.' }
if (-not $NativeProfile) {
    $relativeProfile = if ($ControlSet) { "games/$ControlSet/dolphinxr-right.ini" } else { 'dolphinxr/split-right.ini' }
    $NativeProfile = Join-Path $PSScriptRoot "../../data/controls/wii/generated/$relativeProfile"
}
# The active native route is deliberately single-player/right hand. Native per-slot
# IR routing is not supplied by swapping button expressions in the pinned build.
$profileKey = Assert-AcGeneratedProfile $NativeProfile Native $ControlSet
if (-not $ControlSet -and $profileKey.StartsWith('games/')) { $ControlSet = $profileKey.Split('/')[1] }
$userPath = Initialize-AcSitting -UserDirectory (Join-Path $state "User-v2-$Mode") -NativeProfile $NativeProfile -Mode $Mode -Quality $Quality `
    -DsuProfile $DsuProfile -DsuPort $DsuPort -ControlSet $ControlSet -ForcedVBI $ForcedVBI -Mirror $Mirror `
    -ControllerAnchor ([bool]$ControllerAnchor) -CameraAnchor ([bool]$CameraAnchor) -CameraSmoothing $CameraSmoothing
if ($PrepareOnly) { Write-Output "Prepared $Mode / $Quality at $userPath. No process started."; return }
if (@(Get-Process -Name Dolphin -ErrorAction SilentlyContinue).Count) {
    throw 'Dolphin is already running. Close the existing session before starting this sitting.'
}
$run = Join-Path $state ('runs/' + (Get-Date -Format 'yyyyMMdd-HHmmss-fff') + "-$Mode-$Quality")
New-Item -ItemType Directory -Path $run -Force | Out-Null
Copy-Item -LiteralPath (Join-Path $userPath 'Config') -Destination (Join-Path $run 'Config-before') -Recurse
$liveLog = Join-Path $userPath 'Logs/dolphin.log'
if (Test-Path -LiteralPath $liveLog) {
    Move-Item -LiteralPath $liveLog -Destination (Join-Path $run 'previous-dolphin.log')
}
$vr = if ($Mode -eq 'Desktop') {'False'} else {'True'}
$eager = if ($Mode -eq 'Eager') {'True'} else {'False'}
$qualitySettings = Get-AcQuality $Quality
foreach ($path in @($Executable,$Disc,$userPath)) {
    if ($path.Contains('"')) { throw 'Quotation marks in a launch path are unsupported.' }
}
$launchArgs = @('--user', ('"' + $userPath + '"'), '-v', 'D3D',
    '-C', "Graphics.VR.EnableOpenXR=$vr", '-C', "Graphics.VR.EagerHeartbeat=$eager",
    '-C', 'Graphics.VR.ResolutionScale=1.0',
    '-C', "Graphics.Settings.InternalResolution=$($qualitySettings.InternalResolution)",
    '-C', "Graphics.Settings.MSAA=$($qualitySettings.MSAA)", '-C', 'Graphics.Settings.SSAA=False',
    '-C', "Graphics.Enhancements.MaxAnisotropy=$($qualitySettings.MaxAnisotropy)",
    '-C', 'Graphics.Stereoscopy.StereoMode=0', '--exec', ('"' + $Disc + '"'))
$vrSwitches = Get-AcVRSwitches $ForcedVBI $Mirror ([bool]$ControllerAnchor) ([bool]$CameraAnchor) $CameraSmoothing
foreach ($key in $vrSwitches.Keys) { $launchArgs += @('-C', "Graphics.VR.$key=$($vrSwitches[$key])") }
@{ mode=$Mode; quality=$Quality; quality_settings=(Get-AcQuality $Quality); executable=$Executable;
   executable_sha256=$ExecutableSHA256; game_id=$GameID; user_dir=$userPath; args=$launchArgs;
   profile_sha256=(Get-FileHash -LiteralPath $NativeProfile -Algorithm SHA256).Hash;
   control_set=$ControlSet; generated_profile=$profileKey;
   dsu_profile=$DsuProfile; dsu_port=$DsuPort; vr_switches=$vrSwitches;
   powershell=$PSVersionTable.PSVersion.ToString(); acceptance='Runtime and owner observation required' } |
    ConvertTo-Json -Depth 5 | Set-Content -LiteralPath (Join-Path $run 'launch.json') -Encoding UTF8
Write-Host "Mode $Mode; quality $Quality; bounded to $MaximumSeconds seconds. Logs: $run"
if ($Mode -ne 'Desktop') {
    Write-Host 'Put on the headset now and face the play direction. Click both sticks together to recenter; keyboard Shift+R remains available.'
    Write-Host "Launch in $LaunchDelaySeconds seconds. This delay is preparation time, not a headset-wear detector."
    Start-Sleep -Seconds $LaunchDelaySeconds
} else {
    if ($DsuProfile) { Write-Host 'VR is OFF. Input uses the isolated loopback DSU profile; no desktop input is required.' }
    else { Write-Host 'VR is OFF. Mouse aims; X = A, Z = B, E = Plus; Shift/Control = Nunchuk Z/C.' }
}
$owned = $null
try {
    $owned = Start-Process -FilePath $Executable -ArgumentList $launchArgs -WorkingDirectory (Split-Path $Executable) -PassThru `
        -RedirectStandardOutput (Join-Path $run 'stdout.log') -RedirectStandardError (Join-Path $run 'stderr.log')
    # Keep the native process handle before exit so PowerShell 5.1 retains ExitCode.
    $null = $owned.Handle
    @{pid=$owned.Id; executable=$Executable; started=(Get-Date).ToString('o')} |
        ConvertTo-Json | Set-Content -LiteralPath (Join-Path $run 'process.json') -Encoding UTF8
    $deadline = (Get-Date).AddSeconds($MaximumSeconds)
    while (-not $owned.WaitForExit(250)) {
        if ((Get-Date) -ge $deadline) {
            $null = $owned.CloseMainWindow()
            if (-not $owned.WaitForExit(5000)) { $owned.Kill(); $owned.WaitForExit() }
            break
        }
    }
    $owned.Refresh()
    "Exit code: $($owned.ExitCode)" | Set-Content -LiteralPath (Join-Path $run 'exit.txt')
} finally {
    if ($null -ne $owned -and -not $owned.HasExited) { $owned.Kill(); $owned.WaitForExit() }
    Copy-Item -LiteralPath (Join-Path $userPath 'Config') -Destination (Join-Path $run 'Config-after') -Recurse
    if (Test-Path -LiteralPath $liveLog) { Copy-Item -LiteralPath $liveLog -Destination (Join-Path $run 'dolphin.log') }
    $logExists = Test-Path -LiteralPath (Join-Path $run 'dolphin.log')
    @{log_exists=$logExists; openxr_lines=(Test-AcOpenXRLog (Join-Path $run 'dolphin.log'));
      stopped=(Get-Date).ToString('o')} | ConvertTo-Json |
        Set-Content -LiteralPath (Join-Path $run 'log-check.json') -Encoding UTF8
    Write-Host "Finished. Evidence: $run"
}
if (-not (Test-AcOpenXRLog (Join-Path $run 'dolphin.log'))) {
    Write-Warning 'This run did not prove an OpenXR diagnostic in dolphin.log. See log-check.json; do not treat empty stdout as logging evidence.'
}
