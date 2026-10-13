# One unattended run of the patched Supermodel that saves chosen frames as pictures and exits.
# The window is hidden, sound is off, priority is below normal and there is a time limit.
# No screen capture, no mouse or keyboard use: the program writes the pictures itself. Windows PowerShell 5.1.
param(
    [Parameter(Mandatory = $true)][string]$Program,   # the built Supermodel program
    [Parameter(Mandatory = $true)][string]$Source,    # its working folder (the Supermodel source root, which holds Config)
    [Parameter(Mandatory = $true)][string]$GameFile,  # your own game file
    [Parameter(Mandatory = $true)][string]$OutDir,    # where the pictures go
    [string]$Frame = '2500',                          # one frame number, or several separated by commas
    [int]$SaveStateFrame = -1,
    [string]$LoadState = '',
    [switch]$Stereo,
    [switch]$FakeHeadset,                             # head-tracked rendering with a made-up headset
    [double]$FakeYaw = 0,
    [double]$Sep = 0.065,
    [double]$Scale = 1.0,
    [double]$Conv = 0.0,
    [int]$Width = 496,
    [int]$Height = 384,
    [int]$TimeoutSec = 45,
    [switch]$Throttle,
    [string[]]$Extra = @(),
    [string]$InputScript = '',
    [switch]$DebugViews
)
$ErrorActionPreference = 'Stop'
if (-not (Test-Path $OutDir)) { New-Item -ItemType Directory -Force $OutDir | Out-Null }
Get-ChildItem $OutDir -Filter '*.bmp' | ForEach-Object { [System.IO.File]::Delete($_.FullName) }
$env:ACVR_HIDDEN = '1'
$env:ACVR_CAPTURE_FRAME = "$Frame"
$env:ACVR_CAPTURE_DIR = $OutDir
if ($SaveStateFrame -ge 0) { $env:ACVR_SAVE_STATE_FRAME = "$SaveStateFrame" } else { $env:ACVR_SAVE_STATE_FRAME = $null }
if ($InputScript) { $env:ACVR_INPUT_SCRIPT = $InputScript } else { $env:ACVR_INPUT_SCRIPT = $null }
if ($Stereo) { $env:ACVR_STEREO = '1' } else { $env:ACVR_STEREO = $null }
if ($FakeHeadset) { $env:ACVR_VR_FAKE = '1'; $env:ACVR_VR_FAKE_YAW = "$FakeYaw" } else { $env:ACVR_VR_FAKE = $null; $env:ACVR_VR_FAKE_YAW = $null }
if ($DebugViews) { $env:ACVR_DEBUG = '1' } else { $env:ACVR_DEBUG = $null }
$env:ACVR_EYE_SEP = "$Sep"; $env:ACVR_WORLD_SCALE = "$Scale"; $env:ACVR_CONVERGENCE = "$Conv"
$arguments = @($GameFile, '-window', "-res=$Width,$Height", '-no-sound', '-no-dsb', '-input-system=sdl', '-no-vsync')
if (-not $Throttle) { $arguments += '-no-throttle' }
if ($Extra) { $arguments += $Extra }
if ($LoadState) { $arguments += "-load-state=$LoadState" }
$p = Start-Process -FilePath $Program -ArgumentList $arguments -WorkingDirectory $Source -PassThru -WindowStyle Hidden -RedirectStandardOutput (Join-Path $OutDir 'stdout.txt') -RedirectStandardError (Join-Path $OutDir 'stderr.txt')
try { $p.PriorityClass = 'BelowNormal' } catch {}
if (-not $p.WaitForExit($TimeoutSec * 1000)) { $p.Kill(); "TIMEOUT after $TimeoutSec s: stopped" } else { 'finished' }
$pictures = Get-ChildItem $OutDir -Filter '*.bmp'
if ($pictures) { $pictures | ForEach-Object { "saved $($_.FullName)" } }
else {
    'no picture written'
    Get-Content (Join-Path $OutDir 'stdout.txt') -ErrorAction SilentlyContinue | Select-Object -Last 8
    Get-Content (Join-Path $OutDir 'stderr.txt') -ErrorAction SilentlyContinue | Select-Object -Last 8
}
