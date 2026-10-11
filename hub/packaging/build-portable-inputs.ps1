# Run in the existing VS x64 developer shell. No dependency acquisition or application launch.
[CmdletBinding()]
param([Parameter(Mandatory)][string]$BuildDir,[string]$RepoRoot='',[switch]$AllTargets)
$ErrorActionPreference = 'Stop'
if (-not $RepoRoot) { $RepoRoot = Join-Path $PSScriptRoot '../..' }
. (Join-Path $PSScriptRoot 'PortablePolicy.ps1')
$RepoRoot = Assert-PlainPortablePath $RepoRoot
$BuildDir = Assert-PlainPortablePath $BuildDir
$revision = Get-PortableSourceRevision $RepoRoot
$cachePath = Assert-PlainPortablePath (Join-Path $BuildDir 'CMakeCache.txt')
$cache = Get-Content -LiteralPath $cachePath
$cacheHomes = @($cache | Where-Object { $_ -like 'CMAKE_HOME_DIRECTORY:*=*' })
if ($cacheHomes.Count -ne 1 -or [IO.Path]::GetFullPath(($cacheHomes[0] -split '=',2)[1]).TrimEnd('\','/') -ine
    [IO.Path]::GetFullPath((Join-Path $RepoRoot 'hub')).TrimEnd('\','/')) { throw 'Build cache belongs to another source tree.' }
foreach ($dependency in @('tomlplusplus','nlohmann_json','json_schema_validator','openvr','libarchive','xz','zlib','chdr')) {
    $entry = @($cache | Where-Object { $_ -like "${dependency}_SOURCE_DIR:*=*" })
    if ($entry.Count -ne 1 -or -not (Test-Path -LiteralPath (($entry[0] -split '=',2)[1]) -PathType Container)) { throw 'All pinned dependency sources must already be local.' }
    Assert-PlainPortablePath (($entry[0] -split '=',2)[1]) | Out-Null
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
