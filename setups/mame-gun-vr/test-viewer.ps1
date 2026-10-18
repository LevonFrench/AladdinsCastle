# SPDX-License-Identifier: GPL-3.0-or-later
param([Parameter(Mandatory=$true)][string]$OpenXRInclude,[Parameter(Mandatory=$true)][string]$OutputDirectory,[string]$MsysRoot=(Join-Path $env:SystemDrive 'msys64'),[string]$RunGatePath='')
$ErrorActionPreference='Stop'
$headers=(Resolve-Path -LiteralPath $OpenXRInclude).Path
New-Item -ItemType Directory -Force -Path $OutputDirectory | Out-Null
$output=(Resolve-Path -LiteralPath $OutputDirectory).Path
$env:PATH=(Join-Path $MsysRoot 'mingw64\bin')+';'+$env:PATH
$shared=Join-Path (Split-Path (Split-Path $PSScriptRoot -Parent) -Parent) 'libacvr\publish-presenter'
function Invoke-HiddenTestProcess([string]$File,[string[]]$Arguments,[string]$Name,[int]$Seconds) {
    if($RunGatePath -and (Test-Path -LiteralPath $RunGatePath)) { throw 'Headset gate blocks this build/run.' }
    $stdout=Join-Path $output ($Name+'.log'); $stderr=Join-Path $output ($Name+'-errors.log')
    $options=@{FilePath=$File; WorkingDirectory=$output; WindowStyle='Hidden'; PassThru=$true; RedirectStandardOutput=$stdout; RedirectStandardError=$stderr}
    if($Arguments.Count) { $options.ArgumentList=$Arguments }
    $process=Start-Process @options; $process.PriorityClass='BelowNormal'; $handle=$process.Handle
    if(-not $process.WaitForExit($Seconds*1000)) { & taskkill.exe /PID $process.Id /T /F | Out-Null; throw ($Name+' exceeded its time limit.') }
    $process.WaitForExit(); Get-Content -LiteralPath $stdout; Get-Content -LiteralPath $stderr
    if($process.ExitCode -ne 0) { throw ($Name+' exit '+$process.ExitCode) }
}

$exe=Join-Path $output 'theatre-test.exe'
$arguments=@('-std=c++17','-O2','-static',('-I"'+$headers+'"'),('"'+(Join-Path $shared 'theatre_test.cpp')+'"'),'-lopengl32','-lgdi32','-luser32','-o',('"'+$exe+'"'))
Invoke-HiddenTestProcess (Join-Path $MsysRoot 'mingw64\bin\g++.exe') $arguments 'theatre-build' 120
Invoke-HiddenTestProcess $exe @() 'theatre-test' 30
