#Requires -Version 5.1
# SPDX-License-Identifier: GPL-3.0-only
$ErrorActionPreference = 'Stop'
Import-Module (Join-Path $PSScriptRoot 'Sitting.psm1') -Force
$root = Join-Path ([IO.Path]::GetTempPath()) ('.local/ac-sitting-test-' + [Guid]::NewGuid().ToString('N'))
New-Item -ItemType Directory -Path $root -Force | Out-Null
$script:checks = 0
function Assert($Condition, $Message) {
    if (-not $Condition) { throw $Message }
    $script:checks++
}
function MustFail([scriptblock]$Action, [string]$Message) {
    $failed = $false
    try { & $Action | Out-Null } catch { $failed = $true }
    Assert $failed $Message
}
try {
    $profile = Join-Path $root 'native.ini'
    $bytes = [Text.Encoding]::UTF8.GetBytes("[Wiimote1]`nSource = 3`nDevice = OpenXR/0/OpenXR Controller`nButtons/A = Button A`nButtons/B = Trigger`n")
    [IO.File]::WriteAllBytes($profile,$bytes)
    $user = Join-Path $root 'User'
    $null = Initialize-AcSitting $user $profile Default Sharp
    Assert (Test-Path "$user/Logs" -PathType Container) 'Logs must exist before any executable starts.'
    Assert ((Get-FileHash "$user/Config/WiimoteNew.ini").Hash -eq (Get-FileHash $profile).Hash) 'Native bytes differ.'
    $gfx = [IO.File]::ReadAllText("$user/Config/GFX.ini")
    Assert ($gfx -match 'InternalResolution = 4' -and $gfx -match 'MSAA = 1' -and $gfx -match 'MaxAnisotropy = 3') 'Sharp quality wrong.'
    $hotkeys = [IO.File]::ReadAllText("$user/Config/Hotkeys.ini")
    Assert ($hotkeys.Contains('@(Shift+R)') -and $hotkeys.Contains('Left Button Thumbstick') -and $hotkeys.Contains('Right Button Thumbstick')) 'Recenter bindings missing.'
    Add-Content "$user/Config/GFX.ini" "`n[Unrelated]`nOwnerValue = Keep"
    $null = Initialize-AcSitting $user $profile Eager SharpAA
    $gfx = [IO.File]::ReadAllText("$user/Config/GFX.ini")
    Assert ($gfx -match 'OwnerValue = Keep' -and $gfx -match 'EagerHeartbeat = True' -and $gfx -match 'MSAA = 4' -and $gfx -match 'MaxAnisotropy = 4') 'Update lost custom keys or quality.'
    Assert (([regex]::Matches($gfx,'InternalResolution =')).Count -eq 1) 'Duplicate owned INI key.'
    Assert (@(Get-ChildItem "$user/ConfigBackups" -Recurse -Filter GFX.ini).Count -eq 1) 'Previous configuration not backed up.'
    $null = Initialize-AcSitting $user $profile Desktop Native
    Assert ((Get-Content "$user/Config/WiimoteNew.ini" -Raw) -match 'Source = 1') 'Desktop keyboard profile missing.'
    Assert ((Get-Content "$user/Config/GFX.ini" -Raw) -match 'EnableOpenXR = False') 'Desktop enabled XR.'
    New-Item -ItemType Directory "$root/unowned" | Out-Null
    MustFail { Initialize-AcSitting "$root/unowned" $profile } 'Must refuse existing unowned user folder.'
    MustFail { Assert-AcPrivatePath (Join-Path ([IO.Path]::GetTempPath()) 'outside') } 'Must refuse non-private output.'
    Set-Content $profile "[Wiimote1]`nSource = 3"
    MustFail { Initialize-AcSitting "$root/bad-profile" $profile } 'Must refuse Source-only native input.'
    Assert (-not (Test-Path "$root/bad-profile")) 'Validation must precede writes.'
    Set-Content $profile "[Wiimote1]`nSource = 0`n[Wiimote2]`nSource = 3`nDevice = OpenXR/0/OpenXR Controller`nButtons/A = A`nButtons/B = B"
    MustFail { Initialize-AcSitting "$root/wrong-slot" $profile } 'Wiimote2 bindings must not admit a disabled Wiimote1.'
    [IO.File]::WriteAllBytes($profile,$bytes)
    $module = Get-Module Sitting
    & $module { function script:Get-Process { param($Name,$ErrorAction) [pscustomobject]@{Id=999999} } }
    try {
        MustFail { Initialize-AcSitting "$root/running-session" $profile } 'Must refuse configuration writes while Dolphin runs.'
        Assert (-not (Test-Path "$root/running-session")) 'Running-process refusal must precede every write.'
    } finally { & $module { Remove-Item Function:Get-Process } }
    $exe = Join-Path $root 'synthetic.exe'; Set-Content $exe 'synthetic, never executed'
    $disc = Join-Path $root 'synthetic.iso'; $header = New-Object byte[] 32
    [Text.Encoding]::ASCII.GetBytes('RBUP08').CopyTo($header,0)
    ([byte[]]@(0x5d,0x1c,0x9e,0xa3)).CopyTo($header,24)
    [IO.File]::WriteAllBytes($disc,$header)
    $hash = (Get-FileHash $exe).Hash
    $generated = Join-Path $root 'generated/dolphinxr'
    New-Item -ItemType Directory $generated -Force | Out-Null
    Copy-Item $profile "$generated/split-right.ini"
    @{version='shell-stick/1';files=@{'dolphinxr/split-right.ini'=(Get-FileHash $profile).Hash}} |
        ConvertTo-Json | Set-Content "$root/generated/generated-files.json"
    & "$PSScriptRoot/Start-Sitting.ps1" -Executable $exe -ExecutableSHA256 $hash -Disc $disc -GameID RBUP08 `
        -StateDirectory "$root/runner" -NativeProfile "$generated/split-right.ini" -Mode Desktop -PrepareOnly
    Assert (Test-Path "$root/runner/User-v2-Desktop/Logs") 'Actual runner prepare path failed.'
    Assert-AcSittingInputs $exe $hash $disc RBUP08
    $script:checks++
    MustFail { Assert-AcSittingInputs $exe ('0'*64) $disc RBUP08 } 'Must reject executable mismatch.'
    MustFail { Assert-AcSittingInputs $exe $hash $disc SBDP08 } 'Must reject other title.'
    [IO.File]::WriteAllBytes($disc,[byte[]]@(1,2))
    MustFail { Assert-AcSittingInputs $exe $hash $disc RBUP08 } 'Must reject short header.'
    Assert (-not (Test-AcOpenXRLog "$root/missing.log")) 'Missing log cannot pass.'
    Set-Content "$root/log.txt" 'Ordinary video message'
    Assert (-not (Test-AcOpenXRLog "$root/log.txt")) 'Unrelated log cannot pass.'
    Add-Content "$root/log.txt" 'I[OpenXR]: OpenXR disabled; no session initialized'
    Assert (Test-AcOpenXRLog "$root/log.txt") 'OpenXR diagnostic must be detected.'
    Write-Output "$script:checks checks passed in PowerShell $($PSVersionTable.PSVersion). No app/game/VR launched."
} finally {
    $resolved = [IO.Path]::GetFullPath($root)
    $temp = [IO.Path]::GetFullPath([IO.Path]::GetTempPath())
    if ($resolved.StartsWith($temp,[StringComparison]::OrdinalIgnoreCase) -and $resolved -match '[/\\]\.local[/\\]ac-sitting-test-[a-f0-9]+$') {
        Remove-Item -LiteralPath $resolved -Recurse -Force
    }
}
