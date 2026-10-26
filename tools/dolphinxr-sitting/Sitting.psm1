# SPDX-License-Identifier: GPL-3.0-only
# Windows PowerShell 5.1. Preparing settings never launches Dolphin or VR.
Set-StrictMode -Version 2.0
$script:Product = Get-Content -LiteralPath (Join-Path $PSScriptRoot '../../data/product.json') -Raw | ConvertFrom-Json
if ($script:Product.schema -ne 1 -or $script:Product.dsu_server_name -notmatch '^[A-Za-z0-9_-]+$' -or
    $script:Product.display_name -notmatch '^[A-Za-z0-9 _-]+$') { throw 'Invalid product identity.' }
$script:SittingMarker = $script:Product.display_name + ' sitting settings v2'
$script:GameProfileDirectory = Join-Path $PSScriptRoot '../../data/profiles/dolphinxr/GameSettingsVR'

function Write-AcText([string]$Path, [string]$Text) {
    $null = Assert-AcPrivatePath $Path
    [IO.File]::WriteAllText($Path, $Text, (New-Object Text.UTF8Encoding($false)))
}

function Set-AcIniValues([string]$Path, [System.Collections.IDictionary]$Sections) {
    $lines = New-Object 'Collections.Generic.List[string]'
    if (Test-Path -LiteralPath $Path) {
        foreach ($line in [IO.File]::ReadAllLines($Path)) { $lines.Add($line) }
    }
    foreach ($section in $Sections.Keys) {
        $start = -1; $end = $lines.Count
        for ($i = 0; $i -lt $lines.Count; $i++) {
            if ($lines[$i].Trim() -eq "[$section]") { $start = $i; break }
        }
        if ($start -lt 0) { $lines.Add("[$section]"); $start = $lines.Count - 1 }
        for ($i = $start + 1; $i -lt $lines.Count; $i++) {
            if ($lines[$i] -match '^\s*\[') { $end = $i; break }
        }
        if ($end -le $start) { $end = $lines.Count }
        foreach ($key in $Sections[$section].Keys) {
            # Remove every old occurrence of an owned key; retain all unrelated settings.
            for ($i = $end - 1; $i -gt $start; $i--) {
                if ($lines[$i] -match ('^\s*' + [regex]::Escape($key) + '\s*=')) {
                    $lines.RemoveAt($i); $end--
                }
            }
            $lines.Insert($end, "$key = $($Sections[$section][$key])"); $end++
        }
    }
    Write-AcText $Path (($lines -join "`r`n") + "`r`n")
}

function Assert-AcPrivatePath([string]$Path) {
    $full = [IO.Path]::GetFullPath($Path)
    if ($full -notmatch '[/\\]\.local[/\\].+') { throw 'Sitting output must be a child of a project .local directory.' }
    $cursor = $full
    while ($cursor) {
        if (Test-Path -LiteralPath $cursor) {
            if ((Get-Item -LiteralPath $cursor -Force).Attributes -band [IO.FileAttributes]::ReparsePoint) {
                throw "Linked sitting path refused: $cursor"
            }
        }
        $cursor = Split-Path -Parent $cursor
    }
    return $full
}

function Get-AcQuality([ValidateSet('Native','Sharp','SharpAA')][string]$Quality) {
    switch ($Quality) {
        Native { return @{ InternalResolution = '1'; MSAA = '1'; MaxAnisotropy = '0' } }
        Sharp { return @{ InternalResolution = '4'; MSAA = '1'; MaxAnisotropy = '3' } }
        SharpAA { return @{ InternalResolution = '4'; MSAA = '4'; MaxAnisotropy = '4' } }
    }
}

function Get-AcVRSwitches {
    param([ValidateSet('Auto','Off','72','90','120')][string]$ForcedVBI='Off',
          [ValidateSet('Both','Left','Right','None')][string]$Mirror='Both',
          [bool]$ControllerAnchor=$false, [bool]$CameraAnchor=$false,
          [ValidateRange(0.0,1.0)][double]$CameraSmoothing=0.85)
    # Native pinned enums; legacy AutoVBI must be false or Off can resolve to 90.
    return [ordered]@{
        ForcedVBIFrequency=@{Auto='-1';Off='0';'72'='72';'90'='90';'120'='120'}[$ForcedVBI]
        AutoVBIFromHMD='False'
        MirrorView=@{Both='0';Left='1';Right='2';None='3'}[$Mirror]
        EnableControllerAnchor=$ControllerAnchor.ToString()
        EnableCameraAnchor=$CameraAnchor.ToString()
        CameraAnchorSmoothing=$CameraSmoothing.ToString([Globalization.CultureInfo]::InvariantCulture)
    }
}

function Assert-AcGeneratedProfile {
    param([string]$Path, [ValidateSet('Native','DSU')][string]$Role,
          [string]$ControlSet = '')
    $full = [IO.Path]::GetFullPath($Path)
    $folder = Split-Path $full
    $leaf = Split-Path $full -Leaf
    $parent = Split-Path $folder -Leaf
    if ((Split-Path (Split-Path $folder) -Leaf) -eq 'games') {
        if ($parent -cnotmatch '^[a-z0-9]+(?:-[a-z0-9]+)*$' -or
            ($ControlSet -and $parent -cne $ControlSet)) { throw 'Game control-set identity differs.' }
        $allowed = if ($Role -eq 'Native') { @('dolphinxr-right.ini') } else { @('scripted-dsu-right.ini','flat-dolphin-right.ini') }
        if ($leaf -cnotin $allowed) { throw 'Expected the generated right-primary game profile.' }
        $generated = Split-Path (Split-Path $folder)
        $key = "games/$parent/$leaf"
    } else {
        $expected = if ($Role -eq 'Native') { 'dolphinxr' } else { 'flat-dolphin' }
        if ($ControlSet -or $parent -cne $expected -or $leaf -cne 'split-right.ini') {
            throw 'Expected the generated split-right profile or a matching per-game profile.'
        }
        $generated = Split-Path $folder
        $key = "$parent/$leaf"
    }
    $manifest = Get-Content -LiteralPath (Join-Path $generated 'generated-files.json') -Raw | ConvertFrom-Json
    $entry = $manifest.files.PSObject.Properties[$key]
    if ($manifest.version -ne 'shell-stick/1' -or $null -eq $entry -or
        $entry.Value -ne (Get-FileHash -LiteralPath $full -Algorithm SHA256).Hash) {
        throw 'Profile does not match the shared generated manifest.'
    }
    return $key
}

function Initialize-AcSitting {
    param([Parameter(Mandatory=$true)][string]$UserDirectory,
          [Parameter(Mandatory=$true)][string]$NativeProfile,
          [ValidateSet('Default','Eager','Desktop')][string]$Mode = 'Default',
          [ValidateSet('Native','Sharp','SharpAA')][string]$Quality = 'Sharp',
          [string]$DsuProfile='', [ValidateRange(1024,65535)][int]$DsuPort=26760,
          [string]$ControlSet='',
          [ValidateSet('Auto','Off','72','90','120')][string]$ForcedVBI='Off',
          [ValidateSet('Both','Left','Right','None')][string]$Mirror='Both',
          [bool]$ControllerAnchor=$false, [bool]$CameraAnchor=$false,
          [ValidateRange(0.0,1.0)][double]$CameraSmoothing=0.85,
          [string]$GameID='')
    if (@(Get-Process -Name Dolphin -ErrorAction SilentlyContinue).Count) {
        throw 'Dolphin is running; settings cannot be prepared or changed during a session.'
    }
    $userPath = Assert-AcPrivatePath $UserDirectory
    if ($GameID -and $GameID -cnotmatch '^[A-Z0-9]{6}$') { throw 'Expected a six-character Wii game ID.' }
    $gameProfileBytes = $null
    $gameProfileTarget = $null
    if ($GameID) {
        $gameProfileSource = Join-Path $script:GameProfileDirectory "$GameID.ini"
        $gameProfileTarget = Assert-AcPrivatePath (Join-Path $userPath "GameSettingsVR/$GameID.ini")
        if (Test-Path -LiteralPath $gameProfileTarget -PathType Container) { throw 'Game profile target is a directory.' }
        if (-not (Test-Path -LiteralPath $gameProfileTarget) -and
            (Test-Path -LiteralPath $gameProfileSource -PathType Leaf)) {
            # Native shader enable lists contain bare names. Never parse/reserialize them.
            $gameProfileBytes = [IO.File]::ReadAllBytes($gameProfileSource)
        }
    }
    if ($ControlSet) { $null = Assert-AcGeneratedProfile $NativeProfile Native $ControlSet }
    $profileBytes = [IO.File]::ReadAllBytes($NativeProfile)
    $profile = [Text.Encoding]::UTF8.GetString($profileBytes)
    $sections = [regex]::Matches($profile, '(?m)^\[([^\r\n]+)\]\r?\n([\s\S]*?)(?=^\[|\z)')
    $remote = @($sections | Where-Object { $_.Groups[1].Value -eq 'Wiimote1' })
    if ($remote.Count -ne 1) { throw 'Native profile must contain one Wiimote1 section.' }
    $remoteText = $remote[0].Groups[2].Value
    foreach ($entry in @(@('Source','3'), @('Device','OpenXR/0/OpenXR Controller'), @('Buttons/A','.+'), @('Buttons/B','.+'))) {
        $keyPattern = '(?m)^' + [regex]::Escape($entry[0]) + '[ \t]*=[ \t]*(.+?)[ \t]*\r?$'
        $found = [regex]::Matches($remoteText, $keyPattern)
        if ($found.Count -ne 1 -or $found[0].Groups[1].Value -notmatch ('^' + $entry[1] + '$')) {
            throw 'Native Wiimote1 lacks unique required active controller bindings.'
        }
    }
    foreach ($other in $sections) {
        if ($other.Groups[1].Value -ne 'Wiimote1' -and $other.Groups[2].Value -match '(?m)^Source\s*=\s*[1-9]') {
            throw 'Native multi-remote aiming is unsupported by this sitting profile.'
        }
    }
    $dsuBytes = $null
    if ($DsuProfile) {
        $dsuBytes = [IO.File]::ReadAllBytes($DsuProfile)
        $dsuText = [Text.Encoding]::UTF8.GetString($dsuBytes)
        if ($dsuText -notmatch ('(?m)^Device = DSUClient/0/' + [regex]::Escape($script:Product.dsu_server_name) + '\r?$') -or
            $dsuText -notmatch '(?m)^Source = 1\r?$') { throw 'Expected generated slot-zero DSU profile.' }
        $null = Assert-AcGeneratedProfile $DsuProfile DSU $ControlSet
    }
    $marker = Join-Path $userPath 'ac-sitting-v2.txt'
    $null = Assert-AcPrivatePath $marker
    if ((Test-Path -LiteralPath $userPath) -and
        (-not (Test-Path -LiteralPath $marker) -or
         [IO.File]::ReadAllText($marker) -ne $script:SittingMarker)) {
        throw 'Existing unowned User directory refused; choose a fresh sitting-v2 directory.'
    }
    $config = Join-Path $userPath 'Config'
    foreach ($target in @($config, (Join-Path $userPath 'Logs'), (Join-Path $userPath 'ConfigBackups'), $marker)) {
        $null = Assert-AcPrivatePath $target
    }
    New-Item -ItemType Directory -Path $config -Force | Out-Null
    # Pinned DolphinXR creates many user folders, but omits Logs before opening its file listener.
    New-Item -ItemType Directory -Path (Join-Path $userPath 'Logs') -Force | Out-Null
    if ($null -ne $gameProfileBytes) {
        New-Item -ItemType Directory -Path (Split-Path $gameProfileTarget) -Force | Out-Null
        # Create only: an existing user's tuned profile is never replaced.
        $stream = [IO.File]::Open($gameProfileTarget, [IO.FileMode]::CreateNew, [IO.FileAccess]::Write)
        try { $stream.Write($gameProfileBytes, 0, $gameProfileBytes.Length) } finally { $stream.Dispose() }
    }
    Write-AcText $marker $script:SittingMarker
    $backup = Join-Path $userPath ('ConfigBackups/' + [Guid]::NewGuid().ToString('N'))
    $existing = @(Get-ChildItem -LiteralPath $config -Filter '*.ini' -File)
    if ($existing.Count) {
        New-Item -ItemType Directory -Path $backup -Force | Out-Null
        foreach ($file in $existing) {
            $null = Assert-AcPrivatePath $file.FullName
            Copy-Item -LiteralPath $file.FullName -Destination $backup
        }
    }
    # Native/DSU buttons must remain active when the headset or another window has focus.
    # Desktop keyboard/mouse input stays focus-gated.
    $backgroundInput = if ($Mode -ne 'Desktop' -or $null -ne $dsuBytes) { 'True' } else { 'False' }
    Set-AcIniValues (Join-Path $config 'Dolphin.ini') ([ordered]@{
        Core = [ordered]@{GFXBackend='D3D';EnableCheats='False'}
        Input = [ordered]@{BackgroundInput=$backgroundInput}
        Interface = [ordered]@{ConfirmStop='False'}
        Display = [ordered]@{RenderToMain='False';RenderWindowWidth='1280';RenderWindowHeight='720';RenderWindowAutoSize='False'}
        General = [ordered]@{ISOPaths='0';RecursiveISOPaths='False';HotkeysRequireFocus='False'}
        Android = [ordered]@{QuestLeftHanded='False'}
        Analytics = [ordered]@{Enabled='False';PermissionAsked='True'}
        AutoUpdate = [ordered]@{UpdateTrack=''}
    })
    $q = Get-AcQuality $Quality
    $vr = if ($Mode -eq 'Desktop') {'False'} else {'True'}
    $eager = if ($Mode -eq 'Eager') {'True'} else {'False'}
    Set-AcIniValues (Join-Path $config 'GFX.ini') ([ordered]@{
        VR = [ordered]@{EnableOpenXR=$vr;EagerHeartbeat=$eager;ResolutionScale='1.0';FlatScreen='False';VirtualScreen='True';DisableCPUCull='True';ScreenDistance='1.5';ScreenSize='1.5';HudThickness='0.0';EnableCameraAnchor='False';EnableControllerAnchor='False'}
        Settings = [ordered]@{InternalResolution=$q.InternalResolution;MSAA=$q.MSAA;SSAA='False';ShowFPS='True'}
        Enhancements = [ordered]@{MaxAnisotropy=$q.MaxAnisotropy}
    })
    Set-AcIniValues (Join-Path $config 'Logger.ini') ([ordered]@{
        Options = [ordered]@{WriteToFile='True';WriteToConsole='False';Verbosity='4'}
        Logs = [ordered]@{OpenXR='True';Video='True';CORE='True';Wiimote='True';CI='True'}
    })
    Set-AcIniValues (Join-Path $config 'Hotkeys.ini') ([ordered]@{
        Hotkeys = [ordered]@{Device='DInput/0/Keyboard Mouse';'VR/Reset VR Position'='@(Shift+R) | (pulse(!`OpenXR/0/OpenXR Controller:Left Button Thumbstick` & !`OpenXR/0/OpenXR Controller:Right Button Thumbstick`, 0.1) & pulse(pulse(`OpenXR/0/OpenXR Controller:Left Button Thumbstick`, 0.15) & pulse(`OpenXR/0/OpenXR Controller:Right Button Thumbstick`, 0.15) & `OpenXR/0/OpenXR Controller:Left Button Thumbstick` & `OpenXR/0/OpenXR Controller:Right Button Thumbstick`, 1.0))';'General/Exit'='hold(`OpenXR/0/OpenXR Controller:Left Button Thumbstick` & `OpenXR/0/OpenXR Controller:Right Button Thumbstick`, 3.0)'}
    })
    $switches = Get-AcVRSwitches $ForcedVBI $Mirror $ControllerAnchor $CameraAnchor $CameraSmoothing
    Set-AcIniValues (Join-Path $config 'GFX.ini') ([ordered]@{VR=$switches})
    if ($null -ne $dsuBytes) {
        Write-AcText (Join-Path $config 'WiimoteNew.ini') $dsuText
        Set-AcIniValues (Join-Path $config 'DSUClient.ini') ([ordered]@{
            Server=[ordered]@{Enabled='True';Entries="$($script:Product.dsu_server_name):127.0.0.1:$DsuPort;"}
        })
    } elseif ($Mode -eq 'Desktop') {
        Write-AcText (Join-Path $config 'WiimoteNew.ini') @'
[Wiimote1]
Source = 1
Device = DInput/0/Keyboard Mouse
Extension = Nunchuk
Buttons/A = X
Buttons/B = Z
Buttons/+ = E
Buttons/- = Q
Buttons/1 = `1`
Buttons/2 = `2`
D-Pad/Up = UP
D-Pad/Down = DOWN
D-Pad/Left = LEFT
D-Pad/Right = RIGHT
IR/Up = `Cursor Y-`
IR/Down = `Cursor Y+`
IR/Left = `Cursor X-`
IR/Right = `Cursor X+`
IR/Auto-Hide = False
Nunchuk/Buttons/Z = LSHIFT
Nunchuk/Buttons/C = LCONTROL
[Wiimote2]
Source = 0
[Wiimote3]
Source = 0
[Wiimote4]
Source = 0
[BalanceBoard]
Source = 0
'@
    } else {
        # Exact generated bytes also consumed by the Hub recipe.
        $null = Assert-AcPrivatePath (Join-Path $config 'WiimoteNew.ini')
        [IO.File]::WriteAllBytes((Join-Path $config 'WiimoteNew.ini'), $profileBytes)
    }
    return $userPath
}

function Assert-AcSittingInputs([string]$Executable, [string]$ExecutableSHA256,
                               [string]$Disc, [string]$GameID) {
    if ($GameID -cnotmatch '^[A-Z0-9]{6}$') { throw 'Expected a six-character Wii game ID.' }
    if ($ExecutableSHA256 -notmatch '^[a-fA-F0-9]{64}$' -or
        (Get-FileHash -LiteralPath $Executable -Algorithm SHA256).Hash -ne $ExecutableSHA256) {
        throw 'Pinned DolphinXR executable hash differs.'
    }
    $stream = [IO.File]::OpenRead($Disc)
    try {
        $header = New-Object byte[] 32
        if ($stream.Read($header, 0, 32) -ne 32) { throw 'Disc header is truncated.' }
        if ([Text.Encoding]::ASCII.GetString($header,0,4) -ceq 'WBFS') {
            # Copied disc header follows the host sector, not a fixed 512-byte offset.
            # Match the Hub's bounded single-disc metadata reader. This does not
            # validate the payload/block map or inspect companion files.
            $hostShift = [int]$header[8]; $wbfsShift = [int]$header[9]
            if ($hostShift -lt 9 -or $hostShift -gt 12 -or $wbfsShift -lt 15 -or
                $wbfsShift -gt 31 -or $wbfsShift -lt $hostShift) { throw 'Invalid WBFS sector sizes.' }
            $hostSize = 1 -shl $hostShift
            $sectors = [uint64]$header[4]*16777216 + [uint64]$header[5]*65536 + [uint64]$header[6]*256 + [uint64]$header[7]
            if ($sectors -eq 0 -or $sectors*$hostSize -lt $hostSize+32 -or $stream.Length -lt $hostSize+32) {
                throw 'Truncated WBFS disc information.'
            }
            $container = New-Object byte[] $hostSize
            $stream.Position = 0
            if ($stream.Read($container,0,$hostSize) -ne $hostSize -or $container[12] -eq 0) {
                throw 'WBFS has no first disc slot.'
            }
            for ($i=13; $i -lt $hostSize; $i++) {
                if ($container[$i] -ne 0) { throw 'Multiple WBFS disc slots are unsupported.' }
            }
            $stream.Position = $hostSize
            if ($stream.Read($header,0,32) -ne 32) { throw 'Copied Wii header is truncated.' }
        }
    } finally { $stream.Dispose() }
    if ([Text.Encoding]::ASCII.GetString($header,0,6) -cne $GameID -or
        [BitConverter]::ToString($header[24..27]) -ne '5D-1C-9E-A3') {
        throw 'Disc header does not match the selected Wii game.'
    }
}

function Assert-AcInitialState([string]$Path, [string]$GameID) {
    if ($Path.Contains('"') -or -not (Test-Path -LiteralPath $Path -PathType Leaf)) { throw 'Initial save state must be an existing file without quotation marks in its path.' }
    $full = (Resolve-Path -LiteralPath $Path).ProviderPath
    $stream = [IO.File]::OpenRead($full)
    try {
        # Native StateHeaderLegacy plus StateHeaderVersion; Dolphin validates the rest.
        $header = New-Object byte[] 32
        if ($stream.Read($header,0,32) -ne 32 -or [Text.Encoding]::ASCII.GetString($header,0,6) -cne $GameID) { throw 'Initial save state is truncated or belongs to a different game.' }
    } finally { $stream.Dispose() }
    return $full
}

function Test-AcOpenXRLog([string]$LogPath) {
    if (-not (Test-Path -LiteralPath $LogPath -PathType Leaf)) { return $false }
    # Require the actual native logger category, not a path or arbitrary mention.
    return [bool](Select-String -LiteralPath $LogPath -Pattern '[NEWID]\[OpenXR\]:' -Quiet)
}

Export-ModuleMember -Function Initialize-AcSitting, Assert-AcSittingInputs, Assert-AcPrivatePath, Test-AcOpenXRLog, Get-AcQuality, Get-AcVRSwitches, Assert-AcGeneratedProfile, Assert-AcInitialState
