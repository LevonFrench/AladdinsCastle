# One unattended test run of a patched hotd2-vr Flycast build, for checking a game without a headset.
# Hidden window, no sound, below-normal priority, a time limit, and never OpenXR: vr.Xr=no and XR_RUNTIME_JSON
# pointing at a file that does not exist. Pictures come from inside the program (vr.DebugShots, ACVR_SNAP).
# Scripted inputs and probes are the patch's ACVR_* variables (see ../README.md). Windows PowerShell 5.1.
#
#   Run-Test.ps1 -Exe <flycast.exe> -RunDir <scratch folder> -Out <picture folder> -Game <disc or rom set> [options]
#
# The program is copied into RunDir and run from there, so its settings, saves and logs stay out of a play folder.
# With -StopFile, the run refuses to start, or stops, while that file exists (for example a "headset in use" flag).
param(
    [Parameter(Mandatory = $true)][string]$Exe,
    [Parameter(Mandatory = $true)][string]$RunDir,
    [Parameter(Mandatory = $true)][string]$Out,
    [Parameter(Mandatory = $true)][string]$Game,
    [string]$StopFile = '',
    [int]$Every = 0,          # vr.DebugShots: a picture every N s of game time (0: off)
    [int]$TimeoutSec = 120,
    [string]$Config = '',     # extra "key=value" (section config) or "section:key=value", comma separated
    [string]$Press = '',      # ACVR_PRESS "s:mask;..." on port 1 (8 Start, 4 A, 2 B)
    [string]$Gun = '',        # ACVR_GUN "s:x:y:buttons;..." (x, y 0..10000; buttons 1 trigger, 2 reload, 4 start, 256 2D)
    [string]$Snap = '',       # ACVR_SNAP "s;s;..." exact-time pictures
    [string]$Rip = '',        # ACVR_RIP "s:count,every,ram;..." (rips go to RunDir\rip; they hold game data: keep private)
    [string]$Watch = '',      # ACVR_WATCH "offset:t,..." (hex RAM offset, t f/h/w): logged when they change
    [string]$Poke = '',       # ACVR_POKE "offset:value,...": 16-bit writes every frame
    [double]$PreviewWide = 0, # ACVR_PREVIEW_WIDE: the flat picture shows k times the stock view
    [double]$Quit = 0,        # ACVR_QUIT: end the run at this game time (s)
    [string]$Ray = '',        # ACVR_RAY "s:px:py:dist:ox:oy:oz:trigger;...": the headset's own gun decision for a ray (needs -Reproject)
    [string]$Gun2 = '',       # ACVR_GUN2: player 2's gun, same format as -Gun
    [string]$Rtc = '',        # ACVR_RTC: fixes the Dreamcast clock (seconds since 1950) so two runs repeat exactly
    [string]$EyeSize = '',    # ACVR_EYE_SIZE, e.g. 2592x2688: build the matrices as if drawing into a headset eye buffer
    [string]$Seed = '',       # a folder whose files replace the saves in RunDir\data first (same start every run)
    [switch]$Reproject,       # rebuild in 3D (vr.Reproject=yes); vr.Yaw / vr.Pitch turn the view
    [switch]$GunPort          # Dreamcast: a light gun in port A
)
$ErrorActionPreference = 'Stop'
if ($StopFile -and (Test-Path $StopFile)) { 'refused: stop file present'; exit 3 }
$exeCopy = Join-Path $RunDir 'flycast.exe'
if (Get-Process flycast -ErrorAction SilentlyContinue | Where-Object { $_.Path -eq $exeCopy }) { 'another run is still going'; exit 4 }
New-Item -ItemType Directory -Force $RunDir, $Out | Out-Null
if (-not (Test-Path $exeCopy) -or (Get-Item $Exe).LastWriteTime -gt (Get-Item $exeCopy).LastWriteTime) { Copy-Item $Exe $exeCopy -Force }
if (-not (Test-Path (Join-Path $RunDir 'emu.cfg'))) { Set-Content -Path (Join-Path $RunDir 'emu.cfg') -Value "[config]`r`nvr.Xr = no`r`n" -Encoding ASCII }
if (-not (Test-Path -LiteralPath $Game)) { throw "No game at $Game" }
Remove-Item (Join-Path $RunDir 'flycast.log') -ErrorAction SilentlyContinue
$data = Join-Path $RunDir 'data'
New-Item -ItemType Directory -Force $data | Out-Null
Get-ChildItem $data -Filter '*.png' | ForEach-Object { [System.IO.File]::Delete($_.FullName) }
if ($Seed) {
    Get-ChildItem $data -File | ForEach-Object { [System.IO.File]::Delete($_.FullName) }
    Copy-Item (Join-Path $Seed '*') $data -Force
}
$base = "config:vr.Xr=no,config:pvr.rend=0,config:rend.NativeDepthInterpolation=yes,config:aica.Volume=0,log:LogToFile=yes,config:vr.DebugShots=$Every,config:rend.vsync=no,config:rend.ThreadedRendering=no"
$base += if ($Reproject) { ',config:vr.Reproject=yes' } else { ',config:vr.Reproject=no' }
if ($GunPort) { $base += ',input:device1=7' }
if ($Config) { $base += ',' + (($Config -split ',' | ForEach-Object { if ($_ -match '^[^=]*:') { $_ } else { "config:$_" } }) -join ',') }
$vars = @{ ACVR_HIDDEN = '1'; ACVR_PRESS = $Press; ACVR_GUN = $Gun; ACVR_GUN2 = $Gun2; ACVR_RAY = $Ray; ACVR_RTC = $Rtc; ACVR_EYE_SIZE = $EyeSize; ACVR_SNAP = $Snap; ACVR_RIP = $Rip; ACVR_WATCH = $Watch; ACVR_POKE = $Poke
    ACVR_PREVIEW_WIDE = $(if ($PreviewWide -ge 1) { [string]$PreviewWide } else { '' }); ACVR_QUIT = $(if ($Quit -gt 0) { [string]$Quit } else { '' }) }
$env:XR_RUNTIME_JSON = Join-Path $RunDir 'no-openxr-runtime.json'   # never created: no OpenXR runtime can load
foreach ($k in $vars.Keys) { if ($vars[$k]) { Set-Item "Env:$k" $vars[$k] } else { Remove-Item "Env:$k" -ErrorAction SilentlyContinue } }
"game $Game`r`nconfig $base`r`n" + (($vars.Keys | Sort-Object | ForEach-Object { "$_ $($vars[$_])" }) -join "`r`n") | Set-Content (Join-Path $Out 'run.txt')
$p = Start-Process -FilePath $exeCopy -ArgumentList @('-config', $base, "`"$Game`"") -WorkingDirectory $RunDir -PassThru -WindowStyle Hidden -RedirectStandardOutput (Join-Path $Out 'stdout.txt') -RedirectStandardError (Join-Path $Out 'stderr.txt')
try { $p.PriorityClass = 'BelowNormal' } catch {}
$stopped = $false
while (-not $p.WaitForExit(2000)) {
    if ($StopFile -and (Test-Path $StopFile)) { $p.Kill(); 'STOPPED: stop file present'; $stopped = $true; break }
    if (((Get-Date) - $p.StartTime).TotalSeconds -gt $TimeoutSec) { $p.Kill(); "time limit of $TimeoutSec s reached: stopped"; break }
}
if ($p.HasExited -and -not $stopped) { "exited, code $($p.ExitCode)" }
foreach ($k in $vars.Keys) { Remove-Item "Env:$k" -ErrorAction SilentlyContinue }
Start-Sleep -Milliseconds 700
Get-ChildItem $data -Filter '*.png' | ForEach-Object { Move-Item $_.FullName (Join-Path $Out $_.Name) -Force }
Copy-Item (Join-Path $RunDir 'flycast.log') (Join-Path $Out 'flycast.log') -ErrorAction SilentlyContinue
"$((Get-ChildItem $Out -Filter '*.png').Count) pictures in $Out"
if ($stopped) { exit 3 }
