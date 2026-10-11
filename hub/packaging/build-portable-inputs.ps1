# Run in the existing VS x64 developer shell. No dependency acquisition or application launch.
[CmdletBinding()]
param([Parameter(Mandatory)][string]$BuildDir,[string]$RepoRoot='',[switch]$AllTargets)
$ErrorActionPreference = 'Stop'
if (-not $RepoRoot) { $RepoRoot = Join-Path $PSScriptRoot '../..' }
. (Join-Path $PSScriptRoot 'PortablePolicy.ps1')
$RepoRoot = Assert-PlainPortablePath $RepoRoot
$BuildDir = Assert-PlainPortablePath $BuildDir
# Configure writes generated identity, notices and resources before linking.
# Refuse every existing writable descendant before any compiler/configure call.
@(Get-PlainPortableFiles $BuildDir) | Out-Null
$revision = Get-PortableSourceRevision $RepoRoot
$cachePath = Assert-PlainPortablePath (Join-Path $BuildDir 'CMakeCache.txt')
$cache = Get-Content -LiteralPath $cachePath
$cacheHomes = @($cache | Where-Object { $_ -like 'CMAKE_HOME_DIRECTORY:*=*' })
if ($cacheHomes.Count -ne 1 -or [IO.Path]::GetFullPath(($cacheHomes[0] -split '=',2)[1]).TrimEnd('\','/') -ine
    [IO.Path]::GetFullPath((Join-Path $RepoRoot 'hub')).TrimEnd('\','/')) { throw 'Build cache belongs to another source tree.' }
$baseEntries = @($cache | Where-Object { $_ -like 'FETCHCONTENT_BASE_DIR:*=*' })
if ($baseEntries.Count -ne 1) { throw 'Configured FetchContent base is required.' }
$fetchBase = ($baseEntries[0] -split '=',2)[1]
# Use FetchContent names, not subproject PROJECT_SOURCE_DIR cache entries.
# CMake 3.31 FetchContent.cmake selects an explicit SOURCE_DIR override, or
# FETCHCONTENT_BASE_DIR/<lowercase declared name>-src when none is set.
foreach ($dependency in @('tomlplusplus','nlohmann_json','json_schema_validator','openvr','ac_archive','ac_xz','ac_zlib','libchdr','sevenzip')) {
    $key = 'FETCHCONTENT_SOURCE_DIR_'+$dependency.ToUpperInvariant()
    $overrides = @($cache | Where-Object { $_ -like "${key}:*=*" })
    if ($overrides.Count -gt 1) { throw 'Duplicate dependency source override.' }
    $dependencyRoot = if ($overrides.Count) { ($overrides[0] -split '=',2)[1] } else { '' }
    if (-not $dependencyRoot) { $dependencyRoot = Join-Path $fetchBase ($dependency+'-src') }
    if (-not [IO.Path]::IsPathRooted($dependencyRoot)) { throw 'Dependency source directories must be absolute.' }
    $dependencyRoot = Assert-PlainPortablePath $dependencyRoot
    if (-not (Test-Path -LiteralPath $dependencyRoot -PathType Container)) { throw 'All declared dependency sources must already be local.' }
}
$productFile = Join-Path $RepoRoot 'data/product.json'
$product = Read-PortableJson $productFile
if ($product.schema -ne 1 -or $product.program_name -notmatch '^[a-z0-9][a-z0-9.-]*$') { throw 'Invalid product identity.' }
$productHash = (Get-FileHash -LiteralPath $productFile -Algorithm SHA256).Hash.ToLowerInvariant()
$oldNodeReuse = $env:MSBUILDDISABLENODEREUSE
try {
    $env:MSBUILDDISABLENODEREUSE = '1'
    & cmake -S (Join-Path $RepoRoot 'hub') -B $BuildDir -DFETCHCONTENT_FULLY_DISCONNECTED=ON -DFETCHCONTENT_UPDATES_DISCONNECTED=ON
    if ($LASTEXITCODE -ne 0) { throw 'Existing-cache configure failed; no build receipt issued.' }
    @(Get-PlainPortableFiles $BuildDir) | Out-Null
    $targets = @('aladdinscastle-hub','hubtool','arttool','steamtool')
    $arguments = @('--build',$BuildDir,'--parallel','2')
    if (-not $AllTargets) { $arguments += @('--target') + $targets }
    & cmake @arguments
    if ($LASTEXITCODE -ne 0) { throw 'Four-EXE build failed; no build receipt issued.' }
    if ((Get-PortableSourceRevision $RepoRoot) -cne $revision -or
        (Get-FileHash -LiteralPath $productFile -Algorithm SHA256).Hash.ToLowerInvariant() -cne $productHash) { throw 'Source changed during build; no build receipt issued.' }
    $binaries = [ordered]@{}
    foreach ($name in @(($product.program_name+'.exe'),'hubtool.exe','arttool.exe','steamtool.exe')) {
        $binary = Assert-PlainPortablePath (Join-Path $BuildDir "bin/$name")
        $binaries[$name] = (Get-FileHash -LiteralPath $binary -Algorithm SHA256).Hash.ToLowerInvariant()
    }
    $receipt = [ordered]@{schema='hub-portable-build/1';source_commit=$revision;product_sha256=$productHash;
        build_succeeded=$true;targets=$targets;all_targets=[bool]$AllTargets;binaries=$binaries}
    $destination = Join-Path $BuildDir 'generated/portable-build.json'
    Assert-PlainPortablePath $destination | Out-Null
    $temporary = $destination+'.'+[Guid]::NewGuid().ToString('N')+'.tmp'
    [IO.File]::WriteAllText($temporary,($receipt | ConvertTo-Json -Depth 6),[Text.UTF8Encoding]::new($false))
    Move-Item -LiteralPath $temporary -Destination $destination -Force
    Write-Output $destination
} finally { $env:MSBUILDDISABLENODEREUSE = $oldNodeReuse }
