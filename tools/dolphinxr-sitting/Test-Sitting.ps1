#Requires -Version 5.1
# SPDX-License-Identifier: GPL-3.0-only
$ErrorActionPreference = 'Stop'
Import-Module (Join-Path $PSScriptRoot 'Sitting.psm1') -Force
$product = Get-Content -LiteralPath (Join-Path $PSScriptRoot '../../data/product.json') -Raw | ConvertFrom-Json
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
    Assert ($gfx -match 'InternalResolution = 4' -and $gfx -match 'MSAA = 1' -and $gfx -match 'MaxAnisotropy = 3' -and $gfx -match 'ResolutionScale = 1\.0') 'Sharp quality or runtime eye resolution wrong.'
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
    $gameProfileLibrary = Join-Path $root 'native-game-profiles'
    New-Item -ItemType Directory $gameProfileLibrary | Out-Null
    $shaderBytes = [Text.Encoding]::UTF8.GetBytes("[Graphics.VR]`r`nVirtualScreen = False`r`n[ShaderOverride_Enable]`r`nSynthetic HUD`r`n[ShaderOverride]`r`nname = Synthetic HUD`r`nhandling = screen`r`n")
    [IO.File]::WriteAllBytes("$gameProfileLibrary/SYN001.ini", $shaderBytes)
    $originalLibrary = & $module { $script:GameProfileDirectory }
    & $module { param($path) $script:GameProfileDirectory = $path } $gameProfileLibrary
    try {
        foreach ($gameMode in @('Default','Eager','Desktop')) {
            $seeded = Initialize-AcSitting "$root/profile-$gameMode" $profile -Mode $gameMode -GameID SYN001
            Assert ((Get-FileHash "$seeded/GameSettingsVR/SYN001.ini").Hash -eq (Get-FileHash "$gameProfileLibrary/SYN001.ini").Hash) 'Shader profile bytes/bare enabled names must survive in every mode.'
            [IO.File]::WriteAllText("$seeded/GameSettingsVR/SYN001.ini", 'user tuning')
            $null = Initialize-AcSitting $seeded $profile -Mode $gameMode -GameID SYN001
            Assert ([IO.File]::ReadAllText("$seeded/GameSettingsVR/SYN001.ini") -ceq 'user tuning') 'Preparation must preserve an existing tuned game profile.'
        }
        $unmatched = Initialize-AcSitting "$root/profile-other" $profile -GameID SYN002
        Assert (-not (Test-Path "$unmatched/GameSettingsVR")) 'A profile must never be borrowed for another region or title.'
        MustFail { Initialize-AcSitting "$root/profile-invalid" $profile -GameID '../SYN001' } 'Invalid game identity must be rejected.'
        Assert (-not (Test-Path "$root/profile-invalid")) 'Invalid game identity must fail before writes.'
    } finally { & $module { param($path) $script:GameProfileDirectory = $path } $originalLibrary }
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
    & (Join-Path $PSHOME 'powershell.exe') -NoProfile -File "$PSScriptRoot/Start-Sitting.ps1" `
        -Executable $exe -ExecutableSHA256 $hash -Disc $disc -GameID RBUP08 -StateDirectory "$root/cli-check" -CheckOnly
    Assert ($LASTEXITCODE -eq 0 -and -not (Test-Path "$root/cli-check")) 'PowerShell -File must support omitted default profile without writes.'
    $generated = Join-Path $root 'generated/dolphinxr'
    New-Item -ItemType Directory $generated -Force | Out-Null
    Copy-Item $profile "$generated/split-right.ini"
    @{version='shell-stick/1';files=@{'dolphinxr/split-right.ini'=(Get-FileHash $profile).Hash}} |
        ConvertTo-Json | Set-Content "$root/generated/generated-files.json"
    & "$PSScriptRoot/Start-Sitting.ps1" -Executable $exe -ExecutableSHA256 $hash -Disc $disc -GameID RBUP08 `
        -StateDirectory "$root/runner" -NativeProfile "$generated/split-right.ini" -Mode Desktop -PrepareOnly
    Assert (Test-Path "$root/runner/User-v2-Desktop/Logs") 'Actual runner prepare path failed.'
    foreach ($setting in @(@('Auto','-1'),@('Off','0'),@('72','72'),@('90','90'),@('120','120'))) {
        $switches = Get-AcVRSwitches -ForcedVBI $setting[0]
        Assert ($switches.ForcedVBIFrequency -eq $setting[1] -and $switches.AutoVBIFromHMD -eq 'False') 'Native VBI enum and Off fallback guard.'
    }
    $dsuFolder = Join-Path $root 'generated/flat-dolphin'
    New-Item -ItemType Directory $dsuFolder -Force | Out-Null
    $dsu = Join-Path $dsuFolder 'split-right.ini'
    Set-Content -LiteralPath $dsu -Value "[Wiimote1]`nSource = 1`nDevice = DSUClient/0/$($product.dsu_server_name)`nButtons/A = Cross`nButtons/B = R2"
    @{version='shell-stick/1';files=@{'flat-dolphin/split-right.ini'=(Get-FileHash $dsu).Hash}} |
        ConvertTo-Json | Set-Content "$root/generated/generated-files.json"
    $scripted = Initialize-AcSitting -UserDirectory "$root/scripted" -NativeProfile $profile -Mode Desktop `
        -DsuProfile $dsu -DsuPort 28760 -ForcedVBI 90 -Mirror Right -CameraAnchor $true -CameraSmoothing 0.7
    $gfx = Get-Content -LiteralPath "$scripted/Config/GFX.ini" -Raw
    Assert ($gfx -match 'MirrorView = 2' -and $gfx -match 'ForcedVBIFrequency = 90') 'Selected mirror and VBI must be written.'
    Assert ($gfx -match 'EnableCameraAnchor = True' -and $gfx -match 'CameraAnchorSmoothing = 0.7') 'Camera anchor switch and invariant smoothing.'
    Assert ((Get-Content -LiteralPath "$scripted/Config/DSUClient.ini" -Raw) -match '127.0.0.1:28760') 'DSU stays loopback at selected port.'
    # Dolphin ConfigChanged reconstructs every parsed entry with a trailing semicolon.
    # A differing raw string restarts its DSU backend on every configuration write.
    $entryLine = @(Get-Content -LiteralPath "$scripted/Config/DSUClient.ini" | Where-Object { $_ -match '^Entries\s*=' })[0]
    $entries = ($entryLine -split '=', 2)[1].Trim()
    $canonical = (@($entries -split ';' | Where-Object { $_ } | ForEach-Object { $_ + ';' }) -join '')
    Assert ($entries -ceq $canonical) 'DSU server entries must be stable under Dolphin ConfigChanged serialization.'
    Assert ($entries -ceq "$($product.dsu_server_name):127.0.0.1:28760;") 'DSU canonical entry preserves product name and selected port.'

    Assert ((Get-Content -LiteralPath "$scripted/Config/WiimoteNew.ini" -Raw) -match 'Source = 1') 'Scripted input must not use native XR override.'
    $gameFolder = Join-Path $root 'generated/games/synthetic-wii'
    New-Item -ItemType Directory $gameFolder -Force | Out-Null
    Copy-Item $profile "$gameFolder/dolphinxr-right.ini"
    Copy-Item $dsu "$gameFolder/scripted-dsu-right.ini"
    $gameFiles = @{
        'games/synthetic-wii/dolphinxr-right.ini'=(Get-FileHash $profile).Hash
        'games/synthetic-wii/scripted-dsu-right.ini'=(Get-FileHash $dsu).Hash
        'flat-dolphin/split-right.ini'=(Get-FileHash $dsu).Hash
    }
    @{version='shell-stick/1';files=$gameFiles} | ConvertTo-Json | Set-Content "$root/generated/generated-files.json"
    $gameUser = Initialize-AcSitting "$root/game-scripted" "$gameFolder/dolphinxr-right.ini" -Mode Desktop `
        -ControlSet synthetic-wii -DsuProfile "$gameFolder/scripted-dsu-right.ini"
    Assert ((Get-FileHash "$gameUser/Config/WiimoteNew.ini").Hash -eq (Get-FileHash $dsu).Hash) 'Game profile must install exact generated bytes.'
    MustFail { Initialize-AcSitting "$root/wrong-game" "$gameFolder/dolphinxr-right.ini" -ControlSet different-wii } 'Must reject mismatched game control identity.'
    Assert (-not (Test-Path "$root/wrong-game")) 'Mismatched game must fail before writes.'
    Copy-Item $profile "$gameFolder/dolphinxr-left.ini"
    MustFail { Assert-AcGeneratedProfile "$gameFolder/dolphinxr-left.ini" Native synthetic-wii } 'Left-primary needs separate native admission.'
    MustFail { Assert-AcGeneratedProfile "$gameFolder/scripted-dsu-right.ini" Native synthetic-wii } 'DSU cannot replace a native route profile.'
    Add-Content "$gameFolder/scripted-dsu-right.ini" '# changed'
    MustFail { Initialize-AcSitting "$root/tampered-game" "$gameFolder/dolphinxr-right.ini" -ControlSet synthetic-wii -DsuProfile "$gameFolder/scripted-dsu-right.ini" } 'Tampered game DSU must fail before writes.'
    Assert (-not (Test-Path "$root/tampered-game")) 'Tampered game profile wrote settings.'
    & (Join-Path $PSHOME 'powershell.exe') -NoProfile -File "$PSScriptRoot/Start-Sitting.ps1" `
        -Executable $exe -ExecutableSHA256 $hash -Disc $disc -GameID RBUP08 -StateDirectory "$root/game-cli" `
        -ControlSet synthetic-wii -NativeProfile "$gameFolder/dolphinxr-right.ini" -PrepareOnly
    Assert ($LASTEXITCODE -eq 0 -and (Test-Path "$root/game-cli/User-v2-Default/Config/WiimoteNew.ini")) 'Actual PS5.1 per-game prepare path must succeed.'
    Add-Content -LiteralPath $dsu -Value '# tampered'
    MustFail { Initialize-AcSitting "$root/tampered-dsu" $profile -DsuProfile $dsu } 'DSU manifest mismatch must fail before writes.'
    Assert (-not (Test-Path "$root/tampered-dsu")) 'Rejected DSU profile must not write settings.'
    Assert-AcSittingInputs $exe $hash $disc RBUP08
    $script:checks++
    MustFail { Assert-AcSittingInputs $exe ('0'*64) $disc RBUP08 } 'Must reject executable mismatch.'
    MustFail { Assert-AcSittingInputs $exe $hash $disc SBDP08 } 'Must reject other title.'
    foreach ($shift in @(9,12)) {
        $sectorSize = 1 -shl $shift
        $wbfs = New-Object byte[] ($sectorSize*2)
        [Text.Encoding]::ASCII.GetBytes('WBFS').CopyTo($wbfs,0)
        $wbfs[7]=2; $wbfs[8]=$shift; $wbfs[9]=21; $wbfs[12]=1
        $header.CopyTo($wbfs,$sectorSize)
        [IO.File]::WriteAllBytes($disc,$wbfs)
        Assert-AcSittingInputs $exe $hash $disc RBUP08
        $script:checks++
        $wbfs[13]=1; [IO.File]::WriteAllBytes($disc,$wbfs)
        MustFail { Assert-AcSittingInputs $exe $hash $disc RBUP08 } 'Must refuse multiple WBFS slots.'
        $wbfs[13]=0; $wbfs[8]=32; [IO.File]::WriteAllBytes($disc,$wbfs)
        MustFail { Assert-AcSittingInputs $exe $hash $disc RBUP08 } 'Must reject invalid WBFS shift before allocation.'
        $wbfs[8]=$shift; $wbfs[12]=0; [IO.File]::WriteAllBytes($disc,$wbfs)
        MustFail { Assert-AcSittingInputs $exe $hash $disc RBUP08 } 'Must require the first WBFS slot.'
        $wbfs[12]=1; $wbfs[$sectorSize+24]=0; [IO.File]::WriteAllBytes($disc,$wbfs)
        MustFail { Assert-AcSittingInputs $exe $hash $disc RBUP08 } 'Must reject invalid copied Wii magic.'
        [IO.File]::WriteAllBytes($disc,$wbfs[0..($sectorSize+12)])
        MustFail { Assert-AcSittingInputs $exe $hash $disc RBUP08 } 'Must reject truncated copied Wii header.'
    }
    [IO.File]::WriteAllBytes($disc,[byte[]]@(1,2))
    MustFail { Assert-AcSittingInputs $exe $hash $disc RBUP08 } 'Must reject short header.'
    Assert (-not (Test-AcOpenXRLog "$root/missing.log")) 'Missing log cannot pass.'
    Set-Content "$root/log.txt" 'Ordinary video message'
    Assert (-not (Test-AcOpenXRLog "$root/log.txt")) 'Unrelated log cannot pass.'
    Add-Content "$root/log.txt" 'I[Video]: ordinary line mentioning OpenXR'
    Assert (-not (Test-AcOpenXRLog "$root/log.txt")) 'A mention is not an OpenXR-category diagnostic.'
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
