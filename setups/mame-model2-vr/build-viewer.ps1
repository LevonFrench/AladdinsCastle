# SPDX-License-Identifier: GPL-3.0-or-later
param(
    [Parameter(Mandatory=$true)][string]$MameSource,
    [Parameter(Mandatory=$true)][string]$OpenXRInclude,
    [Parameter(Mandatory=$true)][string]$OutputDirectory,
    [string]$MsysRoot = (Join-Path $env:SystemDrive 'msys64')
)
$ErrorActionPreference='Stop'
$mame = (Resolve-Path -LiteralPath $MameSource).Path
$headers = (Resolve-Path -LiteralPath $OpenXRInclude).Path
New-Item -ItemType Directory -Path $OutputDirectory -Force | Out-Null
$output = (Resolve-Path -LiteralPath $OutputDirectory).Path
$source = Join-Path $PSScriptRoot 'presenter'
$env:PATH = (Join-Path $MsysRoot 'mingw64\bin') + ';' + $env:PATH
$arguments = @('-std=c++17','-O2','-static',('-I"' + $source + '"'),('-I"' + $headers + '"'),('-I"' + (Join-Path $mame 'src\mame\sega') + '"'),('"' + (Join-Path $source 'Model2Viewer.cpp') + '"'),('"' + (Join-Path $source 'ACVRXR.cpp') + '"'),'-lopengl32','-lgdi32','-luser32','-o',('"' + (Join-Path $output 'model2-viewer.exe') + '"'))
$process = Start-Process -FilePath (Join-Path $MsysRoot 'mingw64\bin\g++.exe') -ArgumentList $arguments -WindowStyle Hidden -PassThru -RedirectStandardOutput (Join-Path $output 'viewer-build.log') -RedirectStandardError (Join-Path $output 'viewer-build-errors.log')
$process.PriorityClass='BelowNormal'; $handle=$process.Handle
if (-not $process.WaitForExit(300000)) { & taskkill.exe /PID $process.Id /T /F | Out-Null; throw 'Viewer build exceeded five minutes.' }
$process.WaitForExit()
if ($process.ExitCode -ne 0) { Get-Content -LiteralPath (Join-Path $output 'viewer-build-errors.log'); throw ('Viewer build exit ' + $process.ExitCode) }
