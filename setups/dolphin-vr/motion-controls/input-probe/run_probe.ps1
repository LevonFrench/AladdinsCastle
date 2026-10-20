# One hidden, below-normal, time-limited DolphinNoGUI run with the input probe and no XR runtime.
# Example:
#   ./run_probe.ps1 -Exe <DolphinNoGUI.exe with probe> -User <test user dir> -Disc <owned disc>
#                   -Script bindings-remote-nunchuk.txt -Probe <out.log> -Seconds 90 -Marker <HEADSET-ON path>
#                   [-Extra '-C','Section.Key=Value']
param(
  [Parameter(Mandatory)] [string] $Exe,
  [Parameter(Mandatory)] [string] $User,
  [Parameter(Mandatory)] [string] $Disc,
  [string] $Script = '',
  [Parameter(Mandatory)] [string] $Probe,
  [int] $Seconds = 90,
  [string[]] $Extra = @(),
  # The owner's headset marker file (private review folder); nothing runs while it exists.
  [Parameter(Mandatory)] [string] $Marker,
  # Optional: stop as soon as the probe log shows emulated time at or past this many seconds.
  [double] $UntilProbeTime = 0
)
if (Test-Path $Marker) { 'paused: headset on'; exit 3 }
if (Get-Process -Name Dolphin, DolphinNoGUI -ErrorAction SilentlyContinue) { 'another Dolphin is running'; exit 4 }
$env:XR_RUNTIME_JSON = Join-Path $env:TEMP 'ac-no-such-openxr-runtime.json'
$env:DOLPHINXR_INPUT_SCRIPT = $Script
$env:DOLPHINXR_INPUT_PROBE = $Probe
Remove-Item Env:DOLPHINXR_GUN -ErrorAction SilentlyContinue
$argv = @('-p', 'headless', '-u', "`"$User`"", '-C', 'Graphics.VR.EnableOpenXR=False') + $Extra + @('-e', "`"$Disc`"")
$p = Start-Process -FilePath $Exe -ArgumentList $argv -WindowStyle Hidden -PassThru `
  -RedirectStandardOutput "$Probe.stdout.txt" -RedirectStandardError "$Probe.stderr.txt"
try { $p.PriorityClass = [System.Diagnostics.ProcessPriorityClass]::BelowNormal } catch {}
$deadline = (Get-Date).AddSeconds($Seconds)
$reached = $false
while (-not $p.HasExited -and (Get-Date) -lt $deadline) {
  Start-Sleep -Milliseconds 2000
  if ($UntilProbeTime -gt 0 -and (Test-Path $Probe)) {
    $last = Get-Content $Probe -Tail 1 -ErrorAction SilentlyContinue
    if ($last -match '^t=([\d.]+)' -and [double]$Matches[1] -ge $UntilProbeTime) { $reached = $true; Start-Sleep -Seconds 3; break }
  }
}
if (-not $p.HasExited) {
  Stop-Process -Id $p.Id -Force
  $null = $p.WaitForExit(20000)
  if ($reached) { "stopped at probe time $UntilProbeTime" } else { "stopped after $Seconds s" }
} else { "exited $($p.ExitCode)" }
