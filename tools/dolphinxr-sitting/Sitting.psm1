# SPDX-License-Identifier: GPL-3.0-only
# Windows PowerShell 5.1. Preparing settings never launches Dolphin or VR.
Set-StrictMode -Version 2.0
$script:Product = Get-Content -LiteralPath (Join-Path $PSScriptRoot '../../data/product.json') -Raw | ConvertFrom-Json
if ($script:Product.schema -ne 1 -or $script:Product.dsu_server_name -notmatch '^[A-Za-z0-9_-]+$' -or
    $script:Product.display_name -notmatch '^[A-Za-z0-9 _-]+$') { throw 'Invalid product identity.' }
$script:SittingMarker = $script:Product.display_name + ' sitting settings v2'

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

function Initialize-AcSitting {
    param([Parameter(Mandatory=$true)][string]$UserDirectory,
          [Parameter(Mandatory=$true)][string]$NativeProfile,
          [ValidateSet('Default','Eager','Desktop')][string]$Mode = 'Default',
          [ValidateSet('Native','Sharp','SharpAA')][string]$Quality = 'Sharp',
          [string]$DsuProfile='', [ValidateRange(1024,65535)][int]$DsuPort=26760,
          [ValidateSet('Auto','Off','72','90','120')][string]$ForcedVBI='Off',
          [ValidateSet('Both','Left','Right','None')][string]$Mirror='Both',
          [bool]$ControllerAnchor=$false, [bool]$CameraAnchor=$false,
          [ValidateRange(0.0,1.0)][double]$CameraSmoothing=0.85)
    if (@(Get-Process -Name Dolphin -ErrorAction SilentlyContinue).Count) {
        throw 'Dolphin is running; settings cannot be prepared or changed during a session.'
    }
    $userPath = Assert-AcPrivatePath $UserDirectory
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
            $dsuText -notmatch '(?m)^Source = 1\r?$') { throw 'Expected generated split-right DSU profile.' }
        $dsuManifest = Get-Content -LiteralPath (Join-Path (Split-Path (Split-Path $DsuProfile)) 'generated-files.json') -Raw | ConvertFrom-Json
        if ((Split-Path $DsuProfile -Leaf) -ne 'split-right.ini' -or $dsuManifest.version -ne 'shell-stick/1' -or
            $dsuManifest.files.'flat-dolphin/split-right.ini' -ne (Get-FileHash -LiteralPath $DsuProfile -Algorithm SHA256).Hash) {
            throw 'DSU profile does not match shared generated manifest.'
        }
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
    Set-AcIniValues (Join-Path $config 'Dolphin.ini') ([ordered]@{
        Core = [ordered]@{GFXBackend='D3D';EnableCheats='False'}
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
        Hotkeys = [ordered]@{Device='DInput/0/Keyboard Mouse';'VR/Reset VR Position'='@(Shift+R) | (`OpenXR/0/OpenXR Controller:Left Button Thumbstick` & `OpenXR/0/OpenXR Controller:Right Button Thumbstick`)'}
    })
    $switches = Get-AcVRSwitches $ForcedVBI $Mirror $ControllerAnchor $CameraAnchor $CameraSmoothing
    Set-AcIniValues (Join-Path $config 'GFX.ini') ([ordered]@{VR=$switches})
    if ($null -ne $dsuBytes) {
        Write-AcText (Join-Path $config 'WiimoteNew.ini') $dsuText
        Set-AcIniValues (Join-Path $config 'DSUClient.ini') ([ordered]@{
            Server=[ordered]@{Enabled='True';Entries="$($script:Product.dsu_server_name):127.0.0.1:$DsuPort"}
        })
        Set-AcIniValues (Join-Path $config 'Dolphin.ini') ([ordered]@{Input=[ordered]@{BackgroundInput='True'}})
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
    if ($ExecutableSHA256 -notmatch '^[a-fA-F0-9]{64}$' -or
        (Get-FileHash -LiteralPath $Executable -Algorithm SHA256).Hash -ne $ExecutableSHA256) {
        throw 'Pinned DolphinXR executable hash differs.'
    }
    $stream = [IO.File]::OpenRead($Disc)
    try {
        $header = New-Object byte[] 32
        if ($stream.Read($header, 0, 32) -ne 32) { throw 'Disc header is truncated.' }
    } finally { $stream.Dispose() }
    if ([Text.Encoding]::ASCII.GetString($header,0,6) -cne $GameID -or
        [BitConverter]::ToString($header[24..27]) -ne '5D-1C-9E-A3') {
        throw 'Disc header does not match the selected Wii game.'
    }
}

function Test-AcOpenXRLog([string]$LogPath) {
    if (-not (Test-Path -LiteralPath $LogPath -PathType Leaf)) { return $false }
    # Require the actual native logger category, not a path or arbitrary mention.
    return [bool](Select-String -LiteralPath $LogPath -Pattern '[NEWID]\[OpenXR\]:' -Quiet)
}

Export-ModuleMember -Function Initialize-AcSitting, Assert-AcSittingInputs, Assert-AcPrivatePath, Test-AcOpenXRLog, Get-AcQuality, Get-AcVRSwitches
