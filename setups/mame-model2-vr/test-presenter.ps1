# SPDX-License-Identifier: GPL-3.0-or-later
param(
    [Parameter(Mandatory=$true)][string]$OpenXRInclude,
    [Parameter(Mandatory=$true)][string]$OutputDirectory,
    [string]$MsysRoot = (Join-Path $env:SystemDrive 'msys64')
)
$ErrorActionPreference='Stop'
$headers=(Resolve-Path -LiteralPath $OpenXRInclude).Path
New-Item -ItemType Directory -Path $OutputDirectory -Force | Out-Null
$output=(Resolve-Path -LiteralPath $OutputDirectory).Path
$source=Join-Path $PSScriptRoot 'presenter'
$exe=Join-Path $output 'presenter-test.exe'
$env:PATH=(Join-Path $MsysRoot 'mingw64\bin')+';'+$env:PATH
function Invoke-HiddenTestProcess([string]$File,[string[]]$Arguments,[string]$Name,[int]$Seconds) {
    $stdout=Join-Path $output ($Name+'.log'); $stderr=Join-Path $output ($Name+'-errors.log')
    $options=@{FilePath=$File; WorkingDirectory=$output; WindowStyle='Hidden'; PassThru=$true; RedirectStandardOutput=$stdout; RedirectStandardError=$stderr}
    if($Arguments.Count) { $options.ArgumentList=$Arguments }
    $process=Start-Process @options; $process.PriorityClass='BelowNormal'; $handle=$process.Handle
    if(-not $process.WaitForExit($Seconds*1000)) { & taskkill.exe /PID $process.Id /T /F | Out-Null; throw ($Name+' exceeded its time limit.') }
    $process.WaitForExit(); Get-Content -LiteralPath $stdout; Get-Content -LiteralPath $stderr
    if($process.ExitCode -ne 0) { throw ($Name+' exit '+$process.ExitCode) }
}
$arguments=@('-std=c++17','-O2','-static',('-I"'+$source+'"'),('-I"'+$headers+'"'),('"'+(Join-Path $source 'presenter_test.cpp')+'"'),'-lopengl32','-lgdi32','-luser32','-o',('"'+$exe+'"'))
Invoke-HiddenTestProcess (Join-Path $MsysRoot 'mingw64\bin\g++.exe') $arguments 'test-build' 120
Invoke-HiddenTestProcess $exe @() 'presenter-test' 120
