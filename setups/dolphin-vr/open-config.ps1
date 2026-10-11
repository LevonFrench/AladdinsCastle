# SPDX-License-Identifier: GPL-3.0-only
# Opens the downloaded application's desktop settings only. No game or XR session.
param([switch]$OpenConfig)
$ErrorActionPreference = 'Stop'
$checkout = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../..'))
$privateRoot = Join-Path $checkout '.local/dolphinxr'
$releaseRoot = Join-Path $privateRoot 'releases/v0.4'
$userRoot = Join-Path $privateRoot 'users/umbrella-inspection'
$exe = Join-Path $releaseRoot 'app/Dolphin.exe'
$pin = Get-Content -LiteralPath (Join-Path $PSScriptRoot 'upstream-pin.json') -Raw | ConvertFrom-Json
foreach ($directory in @((Join-Path $checkout '.local'), $privateRoot, $releaseRoot, (Join-Path $releaseRoot 'app'))) {
    $item = Get-Item -LiteralPath $directory
    if ($item.Attributes -band [IO.FileAttributes]::ReparsePoint) { throw 'Redirected private directory.' }
}
foreach ($directory in @((Join-Path $privateRoot 'users'), $userRoot, (Join-Path $userRoot 'Config'))) {
    if ((Test-Path -LiteralPath $directory) -and
        ((Get-Item -LiteralPath $directory).Attributes -band [IO.FileAttributes]::ReparsePoint)) {
        throw 'Redirected user directory.'
    }
}
$archive = Join-Path $releaseRoot $pin.archive
if ((Get-Item -LiteralPath $archive).Length -ne $pin.bytes -or
    (Get-FileHash -LiteralPath $archive -Algorithm SHA256).Hash.ToLowerInvariant() -ne $pin.sha256 -or
    (Get-FileHash -LiteralPath $exe -Algorithm SHA256).Hash.ToLowerInvariant() -ne $pin.executable_sha256) {
    throw 'Pinned release identity changed.'
}
if (!(Test-Path -LiteralPath $userRoot)) {
    New-Item -ItemType Directory -Path (Join-Path $userRoot 'Config') | Out-Null
    Copy-Item -LiteralPath (Join-Path $PSScriptRoot 'templates/Dolphin.ini') -Destination (Join-Path $userRoot 'Config/Dolphin.ini')
    Copy-Item -LiteralPath (Join-Path $PSScriptRoot 'templates/GFX.ini') -Destination (Join-Path $userRoot 'Config/GFX.ini')
    Copy-Item -LiteralPath (Join-Path $PSScriptRoot 'templates/WiimoteNew.ini') -Destination (Join-Path $userRoot 'Config/WiimoteNew.ini')
}
if (!$OpenConfig) {
    [pscustomobject]@{Executable=$exe;UserDirectory=$userRoot;Mode='Prepared; not launched';VR='Disabled for desktop inspection'}
    return
}
# CLI custom user path returns before profile/registry fallback in UICommon.cpp.
# Explicit VR override also covers an already edited isolated user configuration.
$argsList = @('--user', ('"' + $userRoot + '"'), '-C', 'GFX.VR.EnableOpenXR=False',
    '-C', 'Dolphin.Analytics.Enabled=False', '-C', 'Dolphin.Analytics.PermissionAsked=True',
    '-C', 'Dolphin.AutoUpdate.UpdateTrack=')
$process = Start-Process -FilePath $exe -ArgumentList $argsList -WorkingDirectory (Split-Path $exe) -WindowStyle Normal -PassThru
[pscustomobject]@{PID=$process.Id;Executable=$exe;UserDirectory=$userRoot;Mode='Desktop configuration; no game argument; OpenXR disabled'}
