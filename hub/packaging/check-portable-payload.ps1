# SPDX-License-Identifier: GPL-3.0-only
# Checks an unpacked portable folder the way portable-windows.ps1 does just before it makes the ZIP:
# every runtime file is there, and no private path, game, BIOS, key or personal data is inside.
# Reads the folder only; nothing is started. Example: ./check-portable-payload.ps1 -PackageDir <unpacked folder>
[CmdletBinding()]
param([Parameter(Mandatory)][string]$PackageDir,[string]$RepoRoot='')
$ErrorActionPreference = 'Stop'
if (-not $RepoRoot) { $RepoRoot = Join-Path $PSScriptRoot '../..' }
. (Join-Path $PSScriptRoot 'PortablePolicy.ps1')
$product = Read-PortableJson (Join-Path $RepoRoot 'data/product.json')
if ($product.schema -ne 1 -or $product.program_name -notmatch '^[a-z0-9][a-z0-9.-]*$') { throw 'Invalid product identity' }
Assert-PortablePayload -Root $PackageDir -HubProgram "$($product.program_name).exe" -ShortId $product.short_id -PrivateMarkers @($RepoRoot,$env:USERPROFILE)
Write-Output 'Package preflight passed.'
