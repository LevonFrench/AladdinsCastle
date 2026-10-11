# SPDX-License-Identifier: GPL-3.0-only
# Windows PowerShell 5.1. No installs, downloads, GUI or GPU.
param(
    [string]$Blender = '',
    [string]$Python = 'python',
    [switch]$CheckOnly,
    [switch]$OwnerApprovedCpuBuild
)
$ErrorActionPreference = 'Stop'
$taskRoot = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../..'))
if ($CheckOnly) {
    & $Python -B (Join-Path $taskRoot 'tools/wii_controls.py')
    if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
    & $Python -B (Join-Path $taskRoot 'tools/check_gun_assets.py') --model con-remote-shell --model con-stick-grip
    exit $LASTEXITCODE
}
if (-not $OwnerApprovedCpuBuild) { throw 'Explicit owner approval is required before Blender use.' }
if ([string]::IsNullOrWhiteSpace($Blender)) {
    $blenderCommand = Get-Command 'blender' -CommandType Application -ErrorAction SilentlyContinue
    if (-not $blenderCommand) { throw 'Pass -Blender with the path to an existing Blender 5.2 executable, or add it to PATH.' }
    $Blender = $blenderCommand.Source
}
if (-not (Test-Path -LiteralPath $Blender -PathType Leaf)) { throw 'The specified Blender executable does not exist.' }
$Blender = (Get-Item -LiteralPath $Blender).FullName
& $Python (Join-Path $PSScriptRoot 'build.py') --blender $Blender --owner-approved-cpu-batch --model con-remote-shell --model con-stick-grip
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
& $Blender --background --factory-startup --python-exit-code 1 --threads 4 --python (Join-Path $PSScriptRoot 'src/shell-stick-sheet.py') -- --preview-dir (Join-Path $PSScriptRoot 'preview')
exit $LASTEXITCODE
