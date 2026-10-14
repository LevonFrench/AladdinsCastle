# One unattended run of the patched hotd2-vr Flycast that saves pictures from inside the program and exits.
# The window is hidden (ACVR_HIDDEN), sound is off, priority is below normal and there is a time limit.
# No screen capture, no mouse or keyboard use. Windows PowerShell 5.1.
param(
    [Parameter(Mandatory = $true)][string]$Program,   # flycast.exe in a folder of its own (settings and saves go beside it)
    [Parameter(Mandatory = $true)][string]$Game,      # your own disc image
    [Parameter(Mandatory = $true)][string]$OutDir,    # where the pictures and the log go
    [int]$Every = 10,                                 # one picture every N seconds of game time
    [int]$TimeoutSec = 90,
    [string]$Config = '',                             # extra "section:key=value" pairs, comma separated
    [string]$Press = '',                              # scripted presses "seconds:mask;..." (8 Start, 4 A, 128 right)
    [switch]$NoVr                                     # plain flat picture, no 3D rebuild
)
$ErrorActionPreference = 'Stop'
$run = Split-Path -Parent $Program
$data = Join-Path $run 'data'
New-Item -ItemType Directory -Force $OutDir | Out-Null
if (Test-Path $data) { Get-ChildItem $data -Filter 'shot_*.png' | ForEach-Object { [System.IO.File]::Delete($_.FullName) } }
$base = "config:pvr.rend=0,config:rend.NativeDepthInterpolation=yes,config:aica.Volume=0,log:LogToFile=yes,config:vr.Xr=no,config:vr.DebugShots=$Every"
if (-not $NoVr) { $base += ',config:vr.Reproject=yes' }
if ($Config) { $base += ",$Config" }
$env:ACVR_HIDDEN = '1'
if ($Press) { $env:ACVR_PRESS = $Press } else { $env:ACVR_PRESS = $null }
$p = Start-Process -FilePath $Program -ArgumentList @('-config', $base, "`"$Game`"") -WorkingDirectory $run -PassThru -WindowStyle Hidden `
    -RedirectStandardOutput (Join-Path $OutDir 'stdout.txt') -RedirectStandardError (Join-Path $OutDir 'stderr.txt')
try { $p.PriorityClass = 'BelowNormal' } catch {}
if (-not $p.WaitForExit($TimeoutSec * 1000)) { $p.Kill(); "time limit of $TimeoutSec s reached: stopped" } else { "exited by itself with code $($p.ExitCode)" }
$env:ACVR_HIDDEN = $null; $env:ACVR_PRESS = $null
Start-Sleep -Milliseconds 500
if (Test-Path $data) { Get-ChildItem $data -Filter 'shot_*.png' | ForEach-Object { Move-Item $_.FullName (Join-Path $OutDir $_.Name) -Force } }
Copy-Item (Join-Path $run 'flycast.log') (Join-Path $OutDir 'flycast.log') -ErrorAction SilentlyContinue
Get-ChildItem $OutDir -Filter '*.png' | ForEach-Object { "saved $($_.FullName)" }
