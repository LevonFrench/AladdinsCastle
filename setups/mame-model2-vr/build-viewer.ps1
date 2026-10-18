# SPDX-License-Identifier: GPL-3.0-or-later
param(
    [Parameter(Mandatory=$true)][string]$MameSource,
    [Parameter(Mandatory=$true)][string]$OpenXRInclude,
    [Parameter(Mandatory=$true)][string]$OutputDirectory,
    [string]$MsysRoot = (Join-Path $env:SystemDrive 'msys64'),
    [switch]$SharedPresenter,
    [string]$RunGatePath = ''
)
$ErrorActionPreference='Stop'
$mame=(Resolve-Path -LiteralPath $MameSource).Path
$headers=(Resolve-Path -LiteralPath $OpenXRInclude).Path
New-Item -ItemType Directory -Path $OutputDirectory -Force | Out-Null
$output=(Resolve-Path -LiteralPath $OutputDirectory).Path
$source=Join-Path $PSScriptRoot 'presenter'
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
$library=@()
$name='model2-viewer.exe'
if($SharedPresenter) {
    $object=Join-Path $output 'acvr-publish-presenter.o'
    $archive=Join-Path $output 'libacvr-publish-presenter.a'
    $coreArguments=@('-std=c++17','-O2',('-I"'+$headers+'"'),'-c',('"'+(Join-Path $shared 'Presenter.cpp')+'"'),'-o',('"'+$object+'"'))
    Invoke-HiddenBuild $gpp $coreArguments 'shared-core-build'
    Invoke-HiddenBuild (Join-Path $MsysRoot 'mingw64\bin\ar.exe') @('rcs',('"'+$archive+'"'),('"'+$object+'"')) 'shared-archive'
    $library=@(('"'+$archive+'"'))
    $name='model2-viewer-shared.exe'
} else {
    # Preserve the old viewer and its helpers byte-for-byte until owner play.
    $source=Join-Path $source 'legacy'
}
$arguments=@('-std=c++17','-O2','-static',('-I"'+$source+'"'),('-I"'+$headers+'"'),('-I"'+(Join-Path $mame 'src\mame\sega')+'"'),('"'+(Join-Path $source 'Model2Viewer.cpp')+'"'),('"'+(Join-Path $source 'ACVRXR.cpp')+'"'))+$library+@('-lopengl32','-lgdi32','-luser32','-o',('"'+(Join-Path $output $name)+'"'))
Invoke-HiddenBuild $gpp $arguments 'viewer-build'
Write-Output (Join-Path $output $name)
