# Run from a VS 2022 x64 developer shell. No machine runtime DLLs are used.
[CmdletBinding()]
param(
    [Parameter(Mandatory)][string]$BuildDir,
    [Parameter(Mandatory)][string]$QtDir,
    [Parameter(Mandatory)][string]$OutputDir,
    [string]$RepoRoot = (Join-Path $PSScriptRoot '../..'),
    [string]$VsRedistDir = $env:VCToolsRedistDir
)
$ErrorActionPreference = 'Stop'
$BuildDir = (Resolve-Path -LiteralPath $BuildDir).Path
$QtDir = (Resolve-Path -LiteralPath $QtDir).Path
$RepoRoot = (Resolve-Path -LiteralPath $RepoRoot).Path
$OutputDir = [IO.Path]::GetFullPath($OutputDir)
if (Test-Path -LiteralPath $OutputDir) {
    throw "Output must be a new folder: $OutputDir"
}
if (-not $VsRedistDir) {
    throw 'Use a VS developer shell, or pass the official VS VC/Redist/MSVC version directory.'
}
$VsRedistDir = (Resolve-Path -LiteralPath $VsRedistDir).Path
$crtDirectories = @(Get-ChildItem -LiteralPath (Join-Path $VsRedistDir 'x64') -Directory -Filter 'Microsoft.VC*.CRT')
if ($crtDirectories.Count -ne 1) { throw 'Expected one official VS x64 CRT redistribution directory.' }
$crtFiles = @(Get-ChildItem -LiteralPath $crtDirectories[0].FullName -File -Filter '*.dll')
foreach ($required in @('msvcp140.dll','vcruntime140.dll','vcruntime140_1.dll')) {
    if ($required -notin $crtFiles.Name) { throw "Official VS redist is missing $required" }
}
New-Item -ItemType Directory -Path $OutputDir | Out-Null
foreach ($binary in @('aladdinscastle-hub.exe','hubtool.exe','arttool.exe','steamtool.exe','openvr_api.dll')) {
    Copy-Item -LiteralPath (Join-Path $BuildDir "bin/$binary") -Destination $OutputDir
}
Copy-Item -LiteralPath (Join-Path $BuildDir 'bin/resources') -Destination (Join-Path $OutputDir 'resources') -Recurse
$deploy = Join-Path $QtDir 'bin/windeployqt.exe'
& $deploy --release --no-translations --no-compiler-runtime --qmldir (Join-Path $RepoRoot 'hub/qml') --dir $OutputDir (Join-Path $OutputDir 'aladdinscastle-hub.exe')
if ($LASTEXITCODE -ne 0) { throw "windeployqt failed ($LASTEXITCODE)" }
& $deploy --release --no-translations --no-compiler-runtime --dir $OutputDir (Join-Path $OutputDir 'hubtool.exe')
if ($LASTEXITCODE -ne 0) { throw "windeployqt hubtool failed ($LASTEXITCODE)" }
$crtFiles | Copy-Item -Destination $OutputDir
foreach ($directory in @('games','data','schemas','recipes')) {
    $source = Join-Path $RepoRoot $directory
    if (Test-Path -LiteralPath $source -PathType Container) {
        # Stage only project metadata, never local user/library directories or art.
        $target = Join-Path $OutputDir $directory
        New-Item -ItemType Directory -Path $target | Out-Null
        foreach ($file in Get-ChildItem -LiteralPath $source -Recurse -File) {
            if ($file.Extension -notin @('.toml','.json','.md','.csv','.tsv')) { continue }
            $relative = [IO.Path]::GetRelativePath($source, $file.FullName)
            $destination = Join-Path $target $relative
            New-Item -ItemType Directory -Force -Path (Split-Path -Parent $destination) | Out-Null
            Copy-Item -LiteralPath $file.FullName -Destination $destination
        }
    }
}
New-Item -ItemType Directory -Path (Join-Path $OutputDir 'user') | Out-Null
Copy-Item -LiteralPath (Join-Path $RepoRoot 'LICENSE') -Destination (Join-Path $OutputDir 'LICENSE-GPL-3.0.txt')
Copy-Item -LiteralPath (Join-Path $BuildDir 'licenses') -Destination $OutputDir -Recurse
Copy-Item -LiteralPath (Join-Path $PSScriptRoot 'licenses/LGPL-3.0-only.txt') -Destination (Join-Path $OutputDir 'licenses/Qt-LGPL-3.0.txt')
Copy-Item -LiteralPath (Join-Path $PSScriptRoot 'THIRD-PARTY-NOTICES.md') -Destination $OutputDir
# Preserve notices included with the installed Qt distribution, when present.
foreach ($directory in @('LICENSES','licenses')) {
    $source = Join-Path $QtDir $directory
    if (Test-Path -LiteralPath $source -PathType Container) {
        Copy-Item -LiteralPath $source -Destination (Join-Path $OutputDir 'licenses/Qt-upstream') -Recurse -Force
        break
    }
}
$sbom = Join-Path $QtDir 'sbom'
if (Test-Path -LiteralPath $sbom -PathType Container) {
    Copy-Item -LiteralPath $sbom -Destination (Join-Path $OutputDir 'licenses/Qt-sbom') -Recurse
}
@"
Qt 6.8.3; shared MSVC 2022 x64 libraries.
Application source: https://github.com/LevonFrench/AladdinsCastle
Qt source and module notices: see THIRD-PARTY-NOTICES.md.
"@ | Set-Content -LiteralPath (Join-Path $OutputDir 'BUILD-INFO.txt') -Encoding utf8
$zip = "$OutputDir.zip"
if (Test-Path -LiteralPath $zip) { throw "ZIP already exists: $zip" }
Compress-Archive -LiteralPath $OutputDir -DestinationPath $zip
Write-Output $zip
