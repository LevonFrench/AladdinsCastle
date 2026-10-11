# Windows PowerShell 5.1 helpers. No application, runtime or account is accessed.
function Assert-PlainPortablePath {
    param([Parameter(Mandatory)][string]$Path)
    $absolute = [IO.Path]::GetFullPath($Path)
    $cursor = $absolute
    while ($cursor) {
        if (Test-Path -LiteralPath $cursor) {
            $item = Get-Item -LiteralPath $cursor -Force
            if (($item.Attributes -band [IO.FileAttributes]::ReparsePoint) -ne 0) { throw 'Linked package inputs/output ancestors are refused.' }
        }
        $parent = Split-Path -Parent $cursor
        if (-not $parent -or $parent -eq $cursor) { break }
        $cursor = $parent
    }
    return $absolute
}
function Get-PlainPortableFiles {
    param([Parameter(Mandatory)][string]$Root)
    $rootPath = Assert-PlainPortablePath $Root
    $stack = New-Object 'Collections.Generic.Stack[string]'
    $stack.Push($rootPath)
    while ($stack.Count) {
        $directory = $stack.Pop()
        foreach ($entry in Get-ChildItem -LiteralPath $directory -Force) {
            if (($entry.Attributes -band [IO.FileAttributes]::ReparsePoint) -ne 0) { throw 'Linked entries in package input trees are refused.' }
            if ($entry.PSIsContainer) { $stack.Push($entry.FullName) } else { $entry }
        }
    }
}
function Get-PortableSourceRevision {
    param([Parameter(Mandatory)][string]$RepoRoot)
    # Refuse directory links before Git's untracked enumeration can visit them.
    foreach ($directory in @('hub','libacvr','data','assets','games','schemas','recipes')) {
        $root = Join-Path $RepoRoot $directory
        if (Test-Path -LiteralPath $root -PathType Container) { @(Get-PlainPortableFiles $root) | Out-Null }
    }
    $revision = (& git -C $RepoRoot rev-parse HEAD)
    if ($LASTEXITCODE -ne 0 -or $revision -notmatch '^[a-f0-9]{40}$') { throw 'A full source revision is required.' }
    & git -C $RepoRoot diff --quiet
    if ($LASTEXITCODE -ne 0) { throw 'Tracked source changes prevent portable provenance.' }
    & git -C $RepoRoot diff --cached --quiet
    if ($LASTEXITCODE -ne 0) { throw 'Staged source changes prevent portable provenance.' }
    $untracked = @(& git -C $RepoRoot ls-files --others --exclude-standard -- hub libacvr data assets games schemas recipes)
    if ($LASTEXITCODE -ne 0 -or $untracked.Count) { throw 'Untracked product source or metadata prevents portable provenance.' }
    return $revision.Trim()
}
function Read-PortableJson {
    param([Parameter(Mandatory)][string]$Path,[int64]$Maximum=65536)
    $plain = Assert-PlainPortablePath $Path
    $file = Get-Item -LiteralPath $plain
    if ($file.PSIsContainer -or $file.Length -le 0 -or $file.Length -gt $Maximum) { throw 'Package metadata is empty or exceeds its bound.' }
    $text = Get-Content -LiteralPath $plain -Raw
    $object = $text | ConvertFrom-Json
    if ($text.TrimStart()[0] -ne '{' -or $null -eq $object) { throw 'Package metadata must be a JSON object.' }
    return $object
}
function Assert-PortableBuildReceipt {
    param([string]$BuildDir,[string]$Revision,[string]$ProductFile,[string[]]$Binaries)
    $receipt = Read-PortableJson (Join-Path $BuildDir 'generated/portable-build.json')
    if ($receipt.schema -cne 'hub-portable-build/1' -or $receipt.source_commit -isnot [string] -or $receipt.source_commit -cne $Revision -or
        $receipt.build_succeeded -isnot [bool] -or -not $receipt.build_succeeded) { throw 'A successful matching four-EXE build receipt is required.' }
    if ($receipt.product_sha256 -isnot [string] -or $receipt.product_sha256 -cne (Get-FileHash -LiteralPath $ProductFile -Algorithm SHA256).Hash.ToLowerInvariant()) { throw 'Build receipt product identity differs.' }
    if (@(Compare-Object @($receipt.targets) @('aladdinscastle-hub','hubtool','arttool','steamtool')).Count) { throw 'Receipt must record the four shipping build targets.' }
    $names = @($receipt.binaries.PSObject.Properties.Name)
    if ($names.Count -ne 4 -or @(Compare-Object $names $Binaries).Count) { throw 'Build receipt must cover exactly the four shipping EXEs.' }
    foreach ($binary in $Binaries) {
        $path = Assert-PlainPortablePath (Join-Path $BuildDir "bin/$binary")
        $expected = $receipt.binaries.$binary
        if ($expected -isnot [string] -or $expected -cnotmatch '^[a-f0-9]{64}$' -or
            $expected -cne (Get-FileHash -LiteralPath $path -Algorithm SHA256).Hash.ToLowerInvariant()) { throw 'Shipping binary changed or has no matching build provenance.' }
    }
}
function Get-TrackedPortableMetadata {
    param([string]$RepoRoot)
    $tracked = @(& git -c core.quotepath=false -C $RepoRoot ls-files -- games data schemas recipes)
    if ($LASTEXITCODE -ne 0) { throw 'Cannot obtain tracked metadata allowlist.' }
    $allowed = @{}
    foreach ($relative in $tracked) {
        if ([IO.Path]::GetExtension($relative) -notin @('.toml','.json','.md','.csv','.tsv')) { continue }
        if ($relative -match '(^|/)(\.[^/]+|user|installed)(/|$)') { throw 'Private/reserved metadata directories cannot be staged.' }
        # Only the tracked, flat public catalog has this reserved name. Runtime
        # emulator folders elsewhere (or nested inside it) remain refused.
        if ($relative -match '(^|/)emulators(/|$)' -and $relative -cnotmatch '^data/emulators/[^/]+$') { throw 'Private/reserved metadata directories cannot be staged.' }
        $allowed[$relative] = Assert-PlainPortablePath (Join-Path $RepoRoot $relative)
    }
    foreach ($directory in @('games','data','schemas','recipes')) {
        $root = Join-Path $RepoRoot $directory
        if (-not (Test-Path -LiteralPath $root -PathType Container)) { continue }
        foreach ($file in Get-PlainPortableFiles $root) {
            if ($file.Extension -notin @('.toml','.json','.md','.csv','.tsv')) { continue }
            $relative = $file.FullName.Substring($RepoRoot.TrimEnd('\','/').Length+1).Replace('\','/')
            if (-not $allowed.ContainsKey($relative)) { throw 'Extra metadata is refused before staging.' }
        }
    }
    return $allowed
}
function Get-OptionalPortableRoomInputs {
    param([string]$RepoRoot,[string]$Executable,[string]$SourceCommit,[string]$Sha256)
    $executablePath = Assert-PlainPortablePath $Executable
    $binaryInfo = Get-Item -LiteralPath $executablePath
    if ($binaryInfo.PSIsContainer -or $binaryInfo.Length -le 0 -or $binaryInfo.Length -gt 256MB) { throw 'Native executable is empty or exceeds its bound.' }
    if ($SourceCommit -cnotmatch '^[a-f0-9]{40}$' -or $Sha256 -cnotmatch '^[a-f0-9]{64}$') { throw 'Explicit reviewed native source and binary hashes are required.' }
    $receiptPath = $executablePath+'.launch.json'
    $receipt = Read-PortableJson $receiptPath 16384
    $keys = @($receipt.PSObject.Properties.Name)
    if (@(Compare-Object $keys @('schema','control_stdin','calibrate_grip','source_commit','sha256') -CaseSensitive).Count -or
        $receipt.schema -cne 'gun-room-launch/1' -or $receipt.control_stdin -isnot [bool] -or -not $receipt.control_stdin -or
        $receipt.calibrate_grip -isnot [bool] -or -not $receipt.calibrate_grip -or
        $receipt.source_commit -isnot [string] -or $receipt.sha256 -isnot [string] -or
        $receipt.source_commit -cne $SourceCommit -or $receipt.sha256 -cne $Sha256 -or
        (Get-FileHash -LiteralPath $executablePath -Algorithm SHA256).Hash.ToLowerInvariant() -cne $Sha256) { throw 'Native room capability/bytes differ from the exact reviewed inputs.' }
    $tracked = @(& git -c core.quotepath=false -C $RepoRoot ls-files -- assets/guns assets/hands libacvr/LICENSE)
    if ($LASTEXITCODE -ne 0) { throw 'Cannot obtain authored asset allowlist.' }
    $assets = @{}
    foreach ($relative in $tracked) {
        if ($relative -cmatch '^assets/guns/([a-z0-9-]+\.glb|runtime-lighting\.json|LICENSE)$' -or
            $relative -cmatch '^assets/hands/(assembly\.json|[a-z0-9-]+\.(glb|toml)|LICENSE)$' -or $relative -ceq 'libacvr/LICENSE') {
            $assets[$relative] = Assert-PlainPortablePath (Join-Path $RepoRoot $relative)
        }
    }
    foreach ($required in @('assets/hands/assembly.json','assets/hands/grip-hand-left.glb','assets/hands/grip-hand-right.glb',
        'assets/hands/grip-hand-left.toml','assets/hands/grip-hand-right.toml','assets/guns/LICENSE','assets/hands/LICENSE','libacvr/LICENSE')) {
        if (-not $assets.ContainsKey($required)) { throw 'Required tracked authored hand assets/notices are missing.' }
    }
    if (-not @($assets.Keys | Where-Object { $_ -cmatch '^assets/guns/[a-z0-9-]+\.glb$' }).Count) { throw 'Tracked authored gun models are required.' }
    return @{Executable=$executablePath;Receipt=$receiptPath;Assets=$assets;SourceCommit=$SourceCommit;Sha256=$Sha256}
}
