# SPDX-License-Identifier: GPL-3.0-or-later
param([Parameter(Mandatory=$true)][string]$OpenXRInclude,
 [Parameter(Mandatory=$true)][string]$OutputDirectory,
 [string]$MsysRoot=(Join-Path $env:SystemDrive 'msys64'),[string]$RunGatePath='')
$ErrorActionPreference='Stop'
$headers=(Resolve-Path -LiteralPath $OpenXRInclude).Path
New-Item -ItemType Directory -Force -Path $OutputDirectory | Out-Null
$output=(Resolve-Path -LiteralPath $OutputDirectory).Path
$shared=Join-Path (Split-Path (Split-Path $PSScriptRoot -Parent) -Parent) 'libacvr\publish-presenter'
$env:PATH=(Join-Path $MsysRoot 'mingw64\bin')+';'+$env:PATH
function Invoke-HiddenBuild([string]$File,[string[]]$Arguments,[string]$Name) {
    if($RunGatePath -and (Test-Path -LiteralPath $RunGatePath)) { throw 'Headset gate blocks this build.' }
    $process=Start-Process -FilePath $File -ArgumentList $Arguments -WorkingDirectory $output -WindowStyle Hidden -PassThru -RedirectStandardOutput (Join-Path $output ($Name+'.log')) -RedirectStandardError (Join-Path $output ($Name+'-errors.log'))
    $process.PriorityClass='BelowNormal'; $handle=$process.Handle
    if(-not $process.WaitForExit(300000)) { & taskkill.exe /PID $process.Id /T /F | Out-Null; throw ($Name+' exceeded five minutes.') }
    $process.WaitForExit()
    if($process.ExitCode -ne 0) { Get-Content -LiteralPath (Join-Path $output ($Name+'-errors.log')); throw ($Name+' exit '+$process.ExitCode) }
}

$gpp=Join-Path $MsysRoot 'mingw64\bin\g++.exe'
$object=Join-Path $output 'acvr-publish-presenter.o'
$archive=Join-Path $output 'libacvr-publish-presenter.a'
Invoke-HiddenBuild $gpp @('-std=c++17','-O2',('-I"'+$headers+'"'),'-c',('"'+(Join-Path $shared 'Presenter.cpp')+'"'),'-o',('"'+$object+'"')) 'shared-core-build'
Invoke-HiddenBuild (Join-Path $MsysRoot 'mingw64\bin\ar.exe') @('rcs',('"'+$archive+'"'),('"'+$object+'"')) 'shared-archive'
Invoke-HiddenBuild $gpp @('-std=c++17','-O2','-static',('-I"'+$shared+'"'),('-I"'+$headers+'"'),('"'+(Join-Path $PSScriptRoot 'GunViewer.cpp')+'"'),('"'+$archive+'"'),'-lopengl32','-lgdi32','-luser32','-o',('"'+(Join-Path $output 'mame-gun-viewer.exe')+'"')) 'viewer-build'
Write-Output (Join-Path $output 'mame-gun-viewer.exe')
