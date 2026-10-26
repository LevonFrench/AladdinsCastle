# SPDX-License-Identifier: GPL-3.0-only
# Fresh-install proof for the portable Hub ZIP, run the way a new user meets it: install into an empty folder,
# start with no games, read the first-run Set-up page, run Set-up against a folder of FAKE files, then uninstall.
# Windows PowerShell 5.1. Nothing here downloads anything, starts SteamVR or touches a real game.
#
# What it starts is the packaged Hub, offscreen (QT_QPA_PLATFORM=offscreen). The ZIP ships only the window
# plugin (qwindows), so the offscreen plugin is copied in AFTER the package has been checked, for this test only.
# The Hub runs with a throwaway profile (USERPROFILE, APPDATA, LOCALAPPDATA, TEMP all empty folders under
# -WorkDir) and a bare PATH, so a file it writes outside the install folder, or a DLL it finds on this PC instead
# of in the package, shows up as a failure.
#
# Example (CI):  ./hub/packaging/tests/Test-FreshInstall.ps1 -PackageZip AladdinsCastle.zip -TestPluginDir plugins `
#                  -WorkDir $env:RUNNER_TEMP/fresh -ArtifactDir fresh-install-evidence
[CmdletBinding()]
param(
    [Parameter(Mandatory)][string]$PackageZip,
    [Parameter(Mandatory)][string]$TestPluginDir,   # a folder holding qoffscreen.dll from the same Qt as the package
    [Parameter(Mandatory)][string]$WorkDir,         # must not exist yet
    [Parameter(Mandatory)][string]$ArtifactDir,     # pictures, receipts, logs: uploaded by CI
    [int]$HubTimeoutSeconds = 120
)
$ErrorActionPreference = 'Stop'
if ($PSVersionTable.PSVersion.Major -ne 5) { throw 'Run this check with Windows PowerShell 5.1.' }
$repo = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../../..'))
$PackageZip = [IO.Path]::GetFullPath($PackageZip)
$TestPluginDir = [IO.Path]::GetFullPath($TestPluginDir)
$WorkDir = [IO.Path]::GetFullPath($WorkDir)
$ArtifactDir = [IO.Path]::GetFullPath($ArtifactDir)
if (-not (Test-Path -LiteralPath $PackageZip -PathType Leaf)) { throw "Package ZIP not found: $PackageZip" }
if (-not (Test-Path -LiteralPath (Join-Path $TestPluginDir 'qoffscreen.dll') -PathType Leaf)) { throw 'qoffscreen.dll is missing from -TestPluginDir.' }
if (Test-Path -LiteralPath $WorkDir) { throw 'Use a new work folder.' }
New-Item -ItemType Directory -Force -Path $ArtifactDir | Out-Null
Add-Type -AssemblyName System.IO.Compression.FileSystem
Add-Type -AssemblyName System.Drawing

$product = Get-Content -LiteralPath (Join-Path $repo 'data/product.json') -Raw | ConvertFrom-Json
$hubExe = "$($product.program_name).exe"
$results = New-Object 'Collections.Generic.List[object]'
$failures = New-Object 'Collections.Generic.List[string]'
function Note([string]$Check,[bool]$Ok,[string]$Detail) {
    $results.Add([pscustomobject]@{ Check = $Check; Result = $(if ($Ok) { 'pass' } else { 'FAIL' }); Detail = $Detail })
    if (-not $Ok) { $failures.Add("$Check - $Detail") }
    Write-Host ("[{0}] {1}: {2}" -f $(if ($Ok) { 'pass' } else { 'FAIL' }), $Check, $Detail)
}
function Write-Summary {
    $lines = @('# Fresh-install check', '', '| Result | Check | Detail |', '|---|---|---|')
    foreach ($r in $results) { $lines += ('| {0} | {1} | {2} |' -f $r.Result, $r.Check, ($r.Detail -replace '\|', '/' -replace "[\r\n]+", ' ')) }
    $lines | Set-Content -LiteralPath (Join-Path $ArtifactDir 'summary.md') -Encoding utf8
    if ($env:GITHUB_STEP_SUMMARY) { $lines | Add-Content -LiteralPath $env:GITHUB_STEP_SUMMARY -Encoding utf8 }
}

# ---- Where things live ---------------------------------------------------------------------------------------
$install = Join-Path $WorkDir 'install'            # the empty folder the new user unpacks into
$profileRoot = Join-Path $WorkDir 'profile'        # the throwaway user profile the Hub runs under
$sandbox = @{
    USERPROFILE  = Join-Path $profileRoot 'user'
    APPDATA      = Join-Path $profileRoot 'user/AppData/Roaming'
    LOCALAPPDATA = Join-Path $profileRoot 'user/AppData/Local'
    TEMP         = Join-Path $profileRoot 'temp'
}
$synthetic = Join-Path $WorkDir 'synthetic'        # fake files standing in for a player's folders
New-Item -ItemType Directory -Force -Path $install | Out-Null
foreach ($key in $sandbox.Keys) { New-Item -ItemType Directory -Force -Path $sandbox[$key] | Out-Null }
New-Item -ItemType Directory -Force -Path $synthetic | Out-Null

# What a real user profile and registry hold before the Hub is ever installed, for the "nothing left behind" check.
function Get-RealProfileNames {
    $names = @()
    foreach ($base in @($env:APPDATA, $env:LOCALAPPDATA, $env:USERPROFILE, [IO.Path]::GetTempPath())) {
        if ($base -and (Test-Path -LiteralPath $base)) {
            $names += @(Get-ChildItem -LiteralPath $base -Force -ErrorAction SilentlyContinue | ForEach-Object { "$base|$($_.Name)" })
        }
    }
    return $names
}
function Get-RegistrySnapshot {
    $keys = @()
    foreach ($path in @('HKCU:\Software', 'HKCU:\Software\Classes')) {
        $keys += @(Get-ChildItem -LiteralPath $path -ErrorAction SilentlyContinue | ForEach-Object { $_.PSChildName } | ForEach-Object { "$path|$_" })
    }
    $run = 'HKCU:\Software\Microsoft\Windows\CurrentVersion\Run'
    if (Test-Path $run) { $keys += @((Get-ItemProperty $run).PSObject.Properties | Where-Object { $_.Name -notmatch '^PS' } | ForEach-Object { "$run|$($_.Name)" }) }
    return $keys
}
$profileBefore = Get-RealProfileNames
$registryBefore = Get-RegistrySnapshot

# ---- 1. Install: unpack the ZIP into the empty folder --------------------------------------------------------
[IO.Compression.ZipFile]::ExtractToDirectory($PackageZip, $install)
$tops = @(Get-ChildItem -LiteralPath $install -Force)
$package = $null
if ($tops.Count -eq 1 -and $tops[0].PSIsContainer) { $package = $tops[0].FullName }
Note 'install: the ZIP unpacks to one folder' ($null -ne $package) ("top-level entries: " + (($tops | ForEach-Object { $_.Name }) -join ', '))
if (-not $package) { Write-Summary; throw 'The package did not unpack to a single folder.' }
$fileCount = @(Get-ChildItem -LiteralPath $package -Recurse -File -Force).Count
Note 'install: files unpacked' ($fileCount -gt 100) "$fileCount files"
Note 'install: the Hub program is there' (Test-Path -LiteralPath (Join-Path $package $hubExe)) $hubExe
Note 'install: no offscreen plugin ships' (-not (Test-Path -LiteralPath (Join-Path $package 'platforms/qoffscreen.dll'))) 'only qwindows is packaged'
Note 'install: the window plugin ships' (Test-Path -LiteralPath (Join-Path $package 'platforms/qwindows.dll')) 'platforms/qwindows.dll'

# The same payload gate the build uses, on the unpacked folder, before anything has been started in it.
try {
    & (Join-Path $repo 'hub/packaging/check-portable-payload.ps1') -PackageDir $package -RepoRoot $repo | Out-Null
    Note 'install: payload check on the unpacked folder' $true 'every runtime file present; no game, BIOS, key, private path or personal data'
} catch { Note 'install: payload check on the unpacked folder' $false $_.Exception.Message }
Note 'install: user folder starts empty' ((@(Get-ChildItem -LiteralPath (Join-Path $package 'user') -Force -ErrorAction SilentlyContinue)).Count -eq 0) 'no settings, no saved reports'

# ---- 2. Make the Hub able to run offscreen (test only) --------------------------------------------------------
Copy-Item -LiteralPath (Join-Path $TestPluginDir 'qoffscreen.dll') -Destination (Join-Path $package 'platforms/qoffscreen.dll')

# ---- 3. Run the packaged Hub ----------------------------------------------------------------------------------
function Stop-HubTree([System.Diagnostics.Process]$Process) {
    if ($Process -and -not $Process.HasExited) { & taskkill.exe /T /F /PID $Process.Id | Out-Null; $Process.WaitForExit(15000) | Out-Null }
}
# Starts the packaged Hub with the sandbox profile and a bare PATH, waits for it to write its picture and quit.
function Invoke-Hub([string]$Name,[hashtable]$Extra) {
    $out = Join-Path $ArtifactDir "$Name.out.txt"; $err = Join-Path $ArtifactDir "$Name.err.txt"
    $picture = Join-Path $ArtifactDir "$Name.png"
    $env_ = @{ QT_QPA_PLATFORM = 'offscreen'; QT_QUICK_BACKEND = 'software'; QT_OPENGL = 'software'
               AC_UI_CAPTURE = $picture; AC_UI_CAPTURE_QUIT = '1'
               PATH = "$env:SystemRoot\System32;$env:SystemRoot" }
    foreach ($key in $sandbox.Keys) { $env_[$key] = $sandbox[$key] }
    $env_['TMP'] = $sandbox['TEMP']
    foreach ($key in $Extra.Keys) { $env_[$key] = $Extra[$key] }
    $saved = @{}
    # Names this PC may carry that would make the package pick up a Qt that is not its own.
    foreach ($key in @('QT_ROOT_DIR', 'QT_PLUGIN_PATH', 'QML2_IMPORT_PATH', 'QML_IMPORT_PATH', 'QT_QPA_PLATFORM_PLUGIN_PATH', 'Qt6_DIR', 'QT_QPA_FONTDIR')) { $env_[$key] = $null }
    foreach ($key in $env_.Keys) { $saved[$key] = [Environment]::GetEnvironmentVariable($key, 'Process'); [Environment]::SetEnvironmentVariable($key, $env_[$key], 'Process') }
    $process = $null
    try {
        $process = Start-Process -FilePath (Join-Path $package $hubExe) -ArgumentList @('--quit-after-ms', ([string]($HubTimeoutSeconds * 1000 - 10000))) `
            -WorkingDirectory $WorkDir -RedirectStandardOutput $out -RedirectStandardError $err -PassThru -WindowStyle Hidden
        $null = $process.Handle   # Windows PowerShell 5.1 loses the exit code of a -PassThru process unless its handle is read while it runs
        if (-not $process.WaitForExit($HubTimeoutSeconds * 1000)) { Stop-HubTree $process; return [pscustomobject]@{ Name = $Name; TimedOut = $true; ExitCode = -1; Picture = $picture; Out = $out; Err = $err } }
        return [pscustomobject]@{ Name = $Name; TimedOut = $false; ExitCode = $process.ExitCode; Picture = $picture; Out = $out; Err = $err }
    } finally {
        Stop-HubTree $process
        foreach ($key in $saved.Keys) { [Environment]::SetEnvironmentVariable($key, $saved[$key], 'Process') }
    }
}
function Get-Text([string]$Path) { if (Test-Path -LiteralPath $Path) { return [IO.File]::ReadAllText($Path) } else { return '' } }
# A picture the Hub took of its own window: the right size and not one flat colour.
function Test-Picture([string]$Path) {
    if (-not (Test-Path -LiteralPath $Path)) { return 'no picture was written' }
    $bitmap = $null
    try {
        $bitmap = New-Object System.Drawing.Bitmap $Path
        if ($bitmap.Width -lt 400 -or $bitmap.Height -lt 300) { return "picture is only $($bitmap.Width) x $($bitmap.Height)" }
        $colours = New-Object 'Collections.Generic.HashSet[int]'
        for ($y = 0; $y -lt $bitmap.Height; $y += 8) { for ($x = 0; $x -lt $bitmap.Width; $x += 8) { [void]$colours.Add($bitmap.GetPixel($x, $y).ToArgb()) } }
        if ($colours.Count -lt 8) { return "picture is nearly one colour ($($colours.Count) colours sampled)" }
        return ''
    } catch { return "picture cannot be read: $($_.Exception.Message)" } finally { if ($bitmap) { $bitmap.Dispose() } }
}
function Assert-HubRun($Run,[string]$What) {
    Note "$What`: the Hub starts and quits by itself" ((-not $Run.TimedOut) -and $Run.ExitCode -eq 0) ("exit code $($Run.ExitCode)" + $(if ($Run.TimedOut) { ', timed out' } else { '' }) + '; ' + (Get-Text $Run.Err).Trim())
    $problem = Test-Picture $Run.Picture
    Note "$What`: it took a picture of its own window" ($problem -eq '') $(if ($problem) { $problem } else { 'picture saved' })
    $receiptPath = $Run.Picture + '.json'
    $receipt = $null
    if (Test-Path -LiteralPath $receiptPath) { $receipt = Get-Content -LiteralPath $receiptPath -Raw | ConvertFrom-Json }
    Note "$What`: receipt says the picture was saved and nothing was playing" ($null -ne $receipt -and $receipt.imageSaved -eq $true -and $receipt.launcherBusy -eq $false) $(if ($receipt) { "imageSaved=$($receipt.imageSaved) launcherBusy=$($receipt.launcherBusy)" } else { 'no receipt' })
}
function Write-Settings([string]$Toml) {
    $user = Join-Path $package 'user'
    New-Item -ItemType Directory -Force -Path $user | Out-Null
    [IO.File]::WriteAllText((Join-Path $user 'hub-settings.toml'), $Toml, (New-Object System.Text.UTF8Encoding($false)))
    foreach ($stale in @('cache/setup-report.json', 'cache/library.json', 'cache/library-index.json')) { Remove-Item -LiteralPath (Join-Path $user $stale) -Force -ErrorAction SilentlyContinue }
}
function Get-SetupSystem([string]$Id) {
    $path = Join-Path $package 'user/cache/setup-report.json'
    if (-not (Test-Path -LiteralPath $path)) { return $null }
    $report = Get-Content -LiteralPath $path -Raw | ConvertFrom-Json
    return @($report.systems | Where-Object { $_.id -eq $Id })[0]
}
function Count-Of($Value) { return @($Value | Where-Object { $null -ne $_ }).Count }
function Slash([string]$Path) { return $Path.Replace('\', '/') }

# 3a. First run: no games, no folders, no settings.
$run = Invoke-Hub 'first-run' @{}
Assert-HubRun $run 'first run'
Note 'first run: the Hub counted its catalog' ((Get-Text $run.Out) -match ':\s*[1-9][0-9]* games') ((Get-Text $run.Out).Trim())
$firstSettings = Join-Path $package 'user/hub-settings.toml'
$setupDone = (Test-Path -LiteralPath $firstSettings) -and ((Get-Text $firstSettings) -match 'setupDone\s*=\s*true')
Note 'first run: Set-up is not marked done' (-not $setupDone) 'a new user is still sent to Set-up'
Note 'first run: no game was started' ((Get-Text ($run.Picture + '.exit.json')) -match '"launcherBusy":\s*false') 'exit receipt'
# The page that is open on a first run is Set-up; its picture is kept as evidence (open it to read the page).

# 3b. Fake files. Nothing here is a game: plain text and zero-filled bytes with game-style names.
$arcade = Join-Path $synthetic 'arcade'
$wii = Join-Path $synthetic 'wii'
$emptyArcade = Join-Path $synthetic 'empty-arcade'
New-Item -ItemType Directory -Force -Path $arcade, $wii, $emptyArcade | Out-Null
[IO.File]::WriteAllBytes((Join-Path $wii 'notes.iso'), (New-Object byte[] 2048))
[IO.File]::WriteAllText((Join-Path $wii 'readme.txt'), 'A fake file for the Hub fresh-install check. Not a game.')
[IO.File]::WriteAllBytes((Join-Path $wii 'zero-bytes.iso'), (New-Object byte[] 0))
$fakeZip = Join-Path $arcade 'readme.zip'
$archive = [IO.Compression.ZipFile]::Open($fakeZip, 'Create')
try {
    $entry = $archive.CreateEntry('notes.txt')
    $writer = New-Object IO.StreamWriter($entry.Open())
    $writer.Write('A fake file for the Hub fresh-install check. Not a game.'); $writer.Dispose()
} finally { $archive.Dispose() }

# 3c. Empty state: the player ticked Sega Model 3 and chose a folder with nothing in it.
Write-Settings ("[ui]`nmodel3GameFolder = '" + (Slash $emptyArcade) + "'`nlibraryRoots = ['" + (Slash $emptyArcade) + "']`n")
$run = Invoke-Hub 'setup-empty' @{ AC_UI_VIEW = 'Setup/4'; AC_UI_LIBRARY_SCAN = '1' }
Assert-HubRun $run 'empty folder'
$m3 = Get-SetupSystem 'model3'
Note 'empty folder: Set-up wrote a report with Model 3 in it' ($null -ne $m3) $(if ($m3) { "note: $($m3.note)" } else { 'no report or no Model 3 entry' })
if ($m3) {
    Note 'empty folder: nothing is called ready' ((Count-Of $m3.ready) -eq 0) "ready=$(Count-Of $m3.ready)"
    Note 'empty folder: the games it knows are listed as not found' ((Count-Of $m3.missing) -gt 0) "not found=$(Count-Of $m3.missing)"
    Note 'empty folder: it says what is missing in words' ((@($m3.missing) | Where-Object { $_.expected }).Count -gt 0) 'each not-found row names what the Hub looks for'
}

# 3d. Error state: folders and programs that do not exist.
$gone = Join-Path $synthetic 'does-not-exist'
Write-Settings ("[ui]`nmodel3GameFolder = '" + (Slash (Join-Path $gone 'arcade')) + "'`nsystem22Executable = '" + (Slash (Join-Path $gone 'timecrisis.exe')) + "'`n" +
    "wiiGameFolder = '" + (Slash (Join-Path $gone 'wii')) + "'`nwiiExecutable = '" + (Slash (Join-Path $gone 'Dolphin.exe')) + "'`nlibraryRoots = ['" + (Slash $emptyArcade) + "']`n")
$run = Invoke-Hub 'setup-error' @{ AC_UI_VIEW = 'Setup/4'; AC_UI_LIBRARY_SCAN = '1' }
Assert-HubRun $run 'missing folders'
$tc = Get-SetupSystem 'system22'
Note 'missing folders: the Time Crisis program that is not there is "not right", with a reason' ($tc -and (Count-Of $tc.notRight) -eq 1 -and ([string]$tc.notRight[0].reason).Length -gt 10) $(if ($tc) { "notRight=$(Count-Of $tc.notRight)" } else { 'no entry' })
$w = Get-SetupSystem 'wii'
Note 'missing folders: Wii shows nothing ready and the games as not found' ($w -and (Count-Of $w.ready) -eq 0 -and (Count-Of $w.missing) -gt 0) $(if ($w) { "ready=$(Count-Of $w.ready) not found=$(Count-Of $w.missing); note: $($w.note)" } else { 'no entry' })
$m3 = Get-SetupSystem 'model3'
Note 'missing folders: Model 3 shows nothing ready' ($m3 -and (Count-Of $m3.ready) -eq 0) $(if ($m3) { "ready=$(Count-Of $m3.ready)" } else { 'no entry' })

# 3e. Unrecognised state: folders that hold files, none of them a game the Hub knows.
Write-Settings ("[ui]`nmodel3GameFolder = '" + (Slash $arcade) + "'`nwiiGameFolder = '" + (Slash $wii) + "'`nlibraryRoots = ['" + (Slash $synthetic) + "']`n")
$run = Invoke-Hub 'setup-unrecognised' @{ AC_UI_VIEW = 'Setup/4'; AC_UI_LIBRARY_SCAN = '1' }
Assert-HubRun $run 'unrecognised files'
$w = Get-SetupSystem 'wii'
if ($w) {
    $listed = @($w.unrecognised | Where-Object { $_.name -eq 'notes.iso' })
    $inOther = @($w.other | Where-Object { $_.detail -match 'not a Wii disc' })
    Note 'unrecognised files: nothing is called ready' ((Count-Of $w.ready) -eq 0) "ready=$(Count-Of $w.ready)"
    Note 'unrecognised files: the library looked through the folder' ($w.unrecognisedChecked -eq $true) "unrecognisedChecked=$($w.unrecognisedChecked)"
    Note 'unrecognised files: the fake disc is named, with a reason' ($listed.Count -eq 1 -and ([string]$listed[0].reason).Length -gt 5) $(if ($listed.Count) { "reason: $($listed[0].reason)" } else { "not in the unrecognised list; other=$($inOther.Count)" })
} else { Note 'unrecognised files: Set-up wrote a report with Wii in it' $false 'no report or no Wii entry' }
$m3 = Get-SetupSystem 'model3'
if ($m3) {
    $zips = @($m3.unrecognised | Where-Object { $_.name -eq 'readme.zip' })
    Note 'unrecognised files: the fake archive in the arcade folder is named' ($zips.Count -eq 1 -and $zips[0].code -eq 'unknownArchive') $(if ($zips.Count) { "code: $($zips[0].code)" } else { 'not listed' })
} else { Note 'unrecognised files: Set-up wrote a report with Model 3 in it' $false 'no Model 3 entry' }
# The fake folders were only read.
Note 'unrecognised files: the fake folders were not changed' ((@(Get-ChildItem -LiteralPath $wii -File).Count) -eq 3 -and (@(Get-ChildItem -LiteralPath $arcade -File).Count) -eq 1) 'same files as before Set-up'

# ---- 4. Evidence: keep the Hub's own files from the run ------------------------------------------------------
$keep = Join-Path $ArtifactDir 'hub-user-folder'
New-Item -ItemType Directory -Force -Path $keep | Out-Null
foreach ($name in @('hub-settings.toml', 'cache/setup-report.json')) {
    $source = Join-Path $package "user/$name"
    if (Test-Path -LiteralPath $source) { Copy-Item -LiteralPath $source -Destination (Join-Path $keep (Split-Path -Leaf $name)) -Force }
}
('{0} bytes, {1} files unpacked' -f (Get-Item -LiteralPath $PackageZip).Length, $fileCount) | Set-Content -LiteralPath (Join-Path $ArtifactDir 'package-size.txt') -Encoding ascii

# ---- 5. Uninstall: delete the folder, then look for anything left outside it -----------------------------------
$stillRunning = @(Get-Process -Name ($hubExe -replace '\.exe$', '') -ErrorAction SilentlyContinue)
Note 'uninstall: no Hub process is left running' ($stillRunning.Count -eq 0) "$($stillRunning.Count) running"
foreach ($p in $stillRunning) { Stop-Process -Id $p.Id -Force -ErrorAction SilentlyContinue }
$removed = $false
for ($try = 0; $try -lt 5 -and -not $removed; ++$try) {
    try { Remove-Item -LiteralPath $install -Recurse -Force; $removed = $true } catch { Start-Sleep -Seconds 2 }
}
Note 'uninstall: the install folder deletes cleanly' ($removed -and -not (Test-Path -LiteralPath $install)) 'deleting the folder is the whole uninstall'

# Everything the Hub could have written lives in the throwaway profile or beside the install; both should hold nothing.
$leftInProfile = @(Get-ChildItem -LiteralPath $profileRoot -Recurse -Force -File -ErrorAction SilentlyContinue | ForEach-Object { $_.FullName.Substring($profileRoot.Length + 1) })
Note 'uninstall: nothing left in the user profile or temp folder it ran under' ($leftInProfile.Count -eq 0) $(if ($leftInProfile.Count) { 'left: ' + ($leftInProfile -join ', ') } else { 'APPDATA, LOCALAPPDATA, USERPROFILE and TEMP are all empty' })
$expectedTop = @('profile', 'synthetic')
$top = @(Get-ChildItem -LiteralPath $WorkDir -Force | ForEach-Object { $_.Name })
$unexpected = @($top | Where-Object { $_ -notin $expectedTop })
Note 'uninstall: nothing left beside where the install was' ($unexpected.Count -eq 0) $(if ($unexpected.Count) { 'extra: ' + ($unexpected -join ', ') } else { 'only the test folders remain' })
# The real profile and registry of this PC: any new entry with the product's name is a failure; other changes are listed for information.
$profileAfter = Get-RealProfileNames
$registryAfter = Get-RegistrySnapshot
$newProfile = @($profileAfter | Where-Object { $_ -notin $profileBefore })
$newRegistry = @($registryAfter | Where-Object { $_ -notin $registryBefore })
$ownName = '(?i)aladdin|acvr|hubtool|steamtool|arttool|' + [regex]::Escape($product.short_id)
$ours = @(($newProfile + $newRegistry) | Where-Object { $_ -match $ownName })
Note 'uninstall: nothing of ours in the real profile or registry' ($ours.Count -eq 0) $(if ($ours.Count) { 'found: ' + ($ours -join ', ') } else { "new unrelated entries ignored: $(($newProfile + $newRegistry).Count)" })
$newRegistry | Set-Content -LiteralPath (Join-Path $ArtifactDir 'registry-new-entries.txt') -Encoding utf8
$newProfile | Set-Content -LiteralPath (Join-Path $ArtifactDir 'profile-new-entries.txt') -Encoding utf8

Write-Summary
if ($failures.Count) { throw ("Fresh-install check failed:`n - " + ($failures -join "`n - ")) }
Write-Output ("Fresh-install check passed: {0} checks." -f $results.Count)
