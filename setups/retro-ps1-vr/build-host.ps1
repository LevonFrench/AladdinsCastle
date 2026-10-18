# SPDX-License-Identifier: GPL-3.0-or-later
# Builds acvr-ps1.exe with MSYS2 MINGW64 g++, hidden and at below-normal priority.
param(
    [Parameter(Mandatory=$true)][string]$OpenXRInclude,
    [Parameter(Mandatory=$true)][string]$OutputDirectory,
    [string]$MsysRoot = (Join-Path $env:SystemDrive 'msys64')
)
$ErrorActionPreference='Stop'
$headers = (Resolve-Path -LiteralPath $OpenXRInclude).Path
New-Item -ItemType Directory -Path $OutputDirectory -Force | Out-Null
$output = (Resolve-Path -LiteralPath $OutputDirectory).Path
$hostSource = Join-Path $PSScriptRoot 'host'
$presenter = Join-Path $PSScriptRoot '..\mame-model2-vr\presenter'
$presenter = (Resolve-Path -LiteralPath $presenter).Path
$env:PATH = (Join-Path $MsysRoot 'mingw64\bin') + ';' + $env:PATH
$arguments = @('-std=c++17','-O2','-static','-Wall','-Wno-misleading-indentation',
    ('-I"' + $hostSource + '"'),('-I"' + $presenter + '"'),('-I"' + $headers + '"'),
    ('"' + (Join-Path $hostSource 'Ps1Host.cpp') + '"'),('"' + (Join-Path $hostSource 'TheatreXR.cpp') + '"'),
    '-lopengl32','-lgdi32','-luser32','-lwinmm','-o',('"' + (Join-Path $output 'acvr-ps1.exe') + '"'))
$process = Start-Process -FilePath (Join-Path $MsysRoot 'mingw64\bin\g++.exe') -ArgumentList $arguments -WindowStyle Hidden -PassThru -RedirectStandardOutput (Join-Path $output 'host-build.log') -RedirectStandardError (Join-Path $output 'host-build-errors.log')
$process.PriorityClass='BelowNormal'; $handle=$process.Handle
if (-not $process.WaitForExit(300000)) { & taskkill.exe /PID $process.Id /T /F | Out-Null; throw 'Host build exceeded five minutes.' }
$process.WaitForExit()
if ($process.ExitCode -ne 0) { Get-Content -LiteralPath (Join-Path $output 'host-build-errors.log'); throw ('Host build exit ' + $process.ExitCode) }
Write-Output ('Built ' + (Join-Path $output 'acvr-ps1.exe'))
