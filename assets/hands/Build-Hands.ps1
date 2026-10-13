# SPDX-License-Identifier: GPL-3.0-only
param([string]$Blender='', [string]$Python='python', [switch]$OwnerApprovedCpuBuild, [switch]$CheckOnly)
$ErrorActionPreference='Stop'
$taskRoot=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../..'))
if ($CheckOnly) {
    & $Python -B (Join-Path $taskRoot 'tools/check_hand_assets.py')
    exit $LASTEXITCODE
}
if (-not $OwnerApprovedCpuBuild) { throw 'Owner approval is required for Blender.' }
if ([string]::IsNullOrWhiteSpace($Blender)) {
    $found=Get-Command blender -CommandType Application -ErrorAction SilentlyContinue
    if (-not $found) { throw 'Provide an existing Blender 5.2 executable with -Blender.' }
    $Blender=$found.Source
}
if (-not (Test-Path -LiteralPath $Blender -PathType Leaf)) { throw 'Blender executable missing.' }
foreach ($hand in @('right','left')) {
    & $Blender --background --factory-startup --python-exit-code 1 --threads 4 --python (Join-Path $PSScriptRoot 'src/grip_hand.py') -- --hand $hand --out (Join-Path $PSScriptRoot ('grip-hand-'+$hand+'.glb')) --preview-dir (Join-Path $PSScriptRoot 'preview')
    if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
}
& $Python -B (Join-Path $taskRoot 'tools/check_hand_assets.py')
exit $LASTEXITCODE
