[CmdletBinding()]
param([Parameter(Mandatory)][string]$FixtureRoot,[switch]$FreshDeployProbe,[string]$ProducerSource='',[string]$PolicySource='')
$ErrorActionPreference = 'Stop'
if ($PSVersionTable.PSVersion.Major -ne 5) { throw 'Run this regression with Windows PowerShell 5.1.' }
if (Test-Path -LiteralPath $FixtureRoot) { throw 'Use a new fixture folder.' }
$source = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../../..'))
$probe = Join-Path $FixtureRoot 'inert-deploy.exe'
New-Item -ItemType Directory -Path $FixtureRoot | Out-Null
# CI has no private local artifacts. Its built-in compiler makes only this inert console fixture, never a
# product, SDK or third-party dependency. Like windeployqt it writes the files named in deploy-files.txt
# (beside it) into the --dir folder, as empty-looking stand-ins that are never loaded.
Add-Type -TypeDefinition @'
public class PortableDeployFixture {
    public static int Main(string[] args) {
        string dir = null;
        for (int i = 0; i < args.Length - 1; i++) { if (args[i] == "--dir") { dir = args[i + 1]; } }
        string list = System.IO.Path.Combine(System.AppDomain.CurrentDomain.BaseDirectory, "deploy-files.txt");
        if (dir == null || !System.IO.File.Exists(list)) { return 0; }
        foreach (string line in System.IO.File.ReadAllLines(list)) {
            if (line.Length == 0) { continue; }
            string target = System.IO.Path.Combine(dir, line.Replace('/', '\\'));
            System.IO.Directory.CreateDirectory(System.IO.Path.GetDirectoryName(target));
            System.IO.File.WriteAllText(target, "synthetic deployed runtime; never loaded");
        }
        return 0;
    }
}
'@ -Language CSharp -OutputAssembly $probe -OutputType ConsoleApplication
. (Join-Path $source 'hub/packaging/PortablePolicy.ps1')
$oldCiRevision = $env:GITHUB_SHA
$failures = 0
function Write-FixtureFile([string]$Path,[string]$Text) {
    New-Item -ItemType Directory -Force -Path (Split-Path -Parent $Path) | Out-Null
    [IO.File]::WriteAllText($Path,$Text,[Text.UTF8Encoding]::new($false))
}
function Assert-Test([bool]$Condition,[string]$Message) { if (-not $Condition) { throw $Message } }
function New-Fixture([string]$Name) {
    $root = Join-Path $FixtureRoot $Name
    $repo = Join-Path $root 'repo'; $build = Join-Path $root 'build'
    foreach ($dir in @('repo/hub/packaging/licenses','repo/data','repo/games','build/bin/resources','build/generated','build/licenses','qt/bin','qt/sbom','redist/x64/Microsoft.VC143.CRT','native')) {
        New-Item -ItemType Directory -Force -Path (Join-Path $root $dir) | Out-Null
    }
    foreach ($script in Get-ChildItem -LiteralPath (Join-Path $source 'hub/packaging') -File -Filter '*.ps1') {
        Copy-Item -LiteralPath $script.FullName -Destination (Join-Path $repo 'hub/packaging')
    }
    if($ProducerSource){Copy-Item -LiteralPath $ProducerSource -Destination (Join-Path $repo 'hub/packaging/build-portable-inputs.ps1')}
    if($PolicySource){Copy-Item -LiteralPath $PolicySource -Destination (Join-Path $repo 'hub/packaging/PortablePolicy.ps1')}
    New-Item -ItemType Directory -Path (Join-Path $repo 'hub/cmake') | Out-Null
    foreach($module in @('Dependencies.cmake','ArchiveDependencies.cmake','SevenZip.cmake','ChdMetadata.cmake')) {
        Copy-Item -LiteralPath (Join-Path $source "hub/cmake/$module") -Destination (Join-Path $repo 'hub/cmake')
    }
    Copy-Item -LiteralPath $probe -Destination (Join-Path $root 'qt/bin/windeployqt.exe')
    $deployList = Join-Path $root 'qt/bin/deploy-files.txt'
    [IO.File]::WriteAllLines($deployList,[string[]]@(Get-PortableQtRuntimeFiles),[Text.UTF8Encoding]::new($false))
    $product = Get-Content -LiteralPath (Join-Path $source 'data/product.json') -Raw
    Write-FixtureFile (Join-Path $repo 'data/product.json') $product
    Write-FixtureFile (Join-Path $build 'generated/product.json') $product
    $program = (ConvertFrom-Json $product).program_name + '.exe'
    foreach ($binary in @($program,'hubtool.exe','arttool.exe','steamtool.exe','openvr_api.dll')) {
        Write-FixtureFile (Join-Path $build "bin/$binary") "synthetic $binary; never launched"
    }
    foreach ($file in @('LICENSE','hub/packaging/THIRD-PARTY-NOTICES.md','hub/packaging/licenses/LGPL-3.0-only.txt','data/nested/ordinary.toml','games/synthetic/README.md')) {
        Write-FixtureFile (Join-Path $repo $file) 'synthetic public metadata/notice'
    }
    Write-FixtureFile (Join-Path $build 'bin/resources/sample.txt') 'synthetic resource'
    $shortId = (ConvertFrom-Json $product).short_id
    foreach ($resource in @("$shortId.vrmanifest",'overlay-thumbnail.png','hub-actions.json','hub-bindings-oculus_touch.json','hub-bindings-knuckles.json')) {
        Write-FixtureFile (Join-Path $build "bin/resources/$resource") 'synthetic runtime resource'
    }
    Write-FixtureFile (Join-Path $build 'licenses/sample.txt') 'synthetic dependency notice'
    Write-FixtureFile (Join-Path $root 'qt/sbom/sample.spdx.json') '{"synthetic":true}'
    foreach ($dll in @('msvcp140.dll','vcruntime140.dll','vcruntime140_1.dll')) {
        Write-FixtureFile (Join-Path $root "redist/x64/Microsoft.VC143.CRT/$dll") 'synthetic CRT; never loaded'
    }
    Write-FixtureFile (Join-Path $repo 'hub/CMakeLists.txt') '# synthetic build home; no compiler is run'
    Write-FixtureFile (Join-Path $build 'CMakeCache.txt') "CMAKE_HOME_DIRECTORY:INTERNAL=$repo/hub`nCMAKE_GENERATOR:INTERNAL=Ninja`nFETCHCONTENT_FULLY_DISCONNECTED:BOOL=ON`n"
    & git -C $repo init --quiet
    if ($LASTEXITCODE) { throw 'Fixture Git init failed' }
    & git -C $repo add .
    & git -C $repo -c user.name='Synthetic fixture' -c user.email='fixture@example.invalid' commit --quiet -m 'Synthetic metadata fixture'
    if ($LASTEXITCODE) { throw 'Fixture Git commit failed' }
    $revision = (& git -C $repo rev-parse HEAD).Trim()
    $binaries = [ordered]@{}
    foreach ($binary in @($program,'hubtool.exe','arttool.exe','steamtool.exe')) {
        $binaries[$binary] = (Get-FileHash -LiteralPath (Join-Path $build "bin/$binary") -Algorithm SHA256).Hash.ToLowerInvariant()
    }
    $receipt = [ordered]@{schema='hub-portable-build/1';source_commit=$revision;product_sha256=(Get-FileHash (Join-Path $repo 'data/product.json')).Hash.ToLowerInvariant();build_succeeded=$true;targets=@('aladdinscastle-hub','hubtool','arttool','steamtool');binaries=$binaries}
    $receipt | ConvertTo-Json -Depth 5 | Set-Content -LiteralPath (Join-Path $build 'generated/portable-build.json') -Encoding utf8
    return @{Root=$root;Repo=$repo;Build=$build;Revision=$revision;Receipt=$receipt;Program=$program;ShortId=$shortId;DeployList=$deployList;Output=(Join-Path $root 'package')}
}
function Invoke-Package($Fixture,[hashtable]$Extra=@{}) {
    $env:GITHUB_SHA = $Fixture.Revision
    & (Join-Path $Fixture.Repo 'hub/packaging/portable-windows.ps1') -RepoRoot $Fixture.Repo -BuildDir $Fixture.Build -QtDir (Join-Path $Fixture.Root 'qt') -VsRedistDir (Join-Path $Fixture.Root 'redist') -OutputDir $Fixture.Output @Extra | Out-Null
}
function Save-FixtureRevision($Fixture) {
    & git -C $Fixture.Repo add .
    & git -C $Fixture.Repo -c user.name='Synthetic fixture' -c user.email='fixture@example.invalid' commit --quiet -m 'Synthetic tracked authored inputs'
    if ($LASTEXITCODE) { throw 'Fixture commit failed' }
    $Fixture.Revision = (& git -C $Fixture.Repo rev-parse HEAD).Trim()
    $Fixture.Receipt.source_commit = $Fixture.Revision
    $Fixture.Receipt | ConvertTo-Json -Depth 5 | Set-Content -LiteralPath (Join-Path $Fixture.Build 'generated/portable-build.json') -Encoding utf8
}
function Add-NativeFixture($Fixture) {
    foreach ($path in @('assets/guns/generic-pistol.glb','assets/hands/grip-hand-left.glb','assets/hands/grip-hand-right.glb',
        'assets/hands/grip-hand-left.toml','assets/hands/grip-hand-right.toml','assets/guns/LICENSE','assets/hands/LICENSE','libacvr/LICENSE')) {
        Write-FixtureFile (Join-Path $Fixture.Repo $path) 'synthetic authored asset/notice; no game content'
    }
    Write-FixtureFile (Join-Path $Fixture.Repo 'assets/hands/assembly.json') '{"schema":"hand-assembly/1","models":{}}'
    Write-FixtureFile (Join-Path $Fixture.Repo 'data/guns/generic-pistol.toml') 'id="generic-pistol"'
    Save-FixtureRevision $Fixture
    $binary = Join-Path $Fixture.Root 'native/reviewed room.exe'
    Write-FixtureFile $binary 'synthetic native; never executed'
    $hash = (Get-FileHash -LiteralPath $binary).Hash.ToLowerInvariant()
    $receipt = [ordered]@{schema='gun-room-launch/1';control_stdin=$true;calibrate_grip=$true;source_commit=('b'*40);sha256=$hash}
    $receipt | ConvertTo-Json | Set-Content -LiteralPath ($binary+'.launch.json') -Encoding utf8
    return @{NativeRoomExecutable=$binary;NativeRoomSourceCommit=('b'*40);NativeRoomSha256=$hash}
}
function Set-FixtureFetchContentCache($Fixture,[bool]$UseOverrides) {
    # Names are extracted independently from the actual declarations. Cache
    # layout follows configured CMake's SOURCE_DIR overrides/base-dir schema,
    # including header-only/prebuilt dependencies with no PROJECT cache key.
    $names=@()
    foreach($module in Get-ChildItem -LiteralPath (Join-Path $Fixture.Repo 'hub/cmake') -File -Filter '*.cmake') {
        foreach($match in [regex]::Matches((Get-Content $module.FullName -Raw),'FetchContent_Declare\(\s*([A-Za-z0-9_]+)')) {$names+=$match.Groups[1].Value.ToLowerInvariant()}
    }
    $names=@($names | Sort-Object -Unique)
    Assert-Test ($names -contains 'openvr' -and $names -contains 'sevenzip' -and $names -contains 'json_schema_validator') 'Canonical declarations missing required header/prebuilt dependency.'
    $base=Join-Path $Fixture.Build '_deps'
    Add-Content -LiteralPath (Join-Path $Fixture.Build 'CMakeCache.txt') -Value "FETCHCONTENT_BASE_DIR:PATH=$base"
    foreach($name in $names) {
        $path=if($UseOverrides){Join-Path $Fixture.Root "local-sources/$name"}else{Join-Path $base ($name+'-src')}
        New-Item -ItemType Directory -Force -Path $path | Out-Null
        $value=if($UseOverrides){$path}else{''}
        Add-Content -LiteralPath (Join-Path $Fixture.Build 'CMakeCache.txt') -Value ('FETCHCONTENT_SOURCE_DIR_'+$name.ToUpperInvariant()+':PATH='+$value)
    }
}
function Assert-RefusedBeforeOutput($Fixture) {
    $refused = $false
    try { Invoke-Package $Fixture } catch { $refused = $true }
    Assert-Test $refused 'Unsafe package inputs were accepted.'
    Assert-Test (-not (Test-Path -LiteralPath $Fixture.Output)) 'Refusal created an output folder.'
}
function Assert-PreflightRefuses($Fixture,[string]$Expected) {
    $reason = ''
    try { Invoke-Package $Fixture } catch { $reason = $_.Exception.Message }
    Assert-Test ($reason -like '*preflight failed*') "Package was not refused by the preflight: $reason"
    Assert-Test ($reason -like "*$Expected*") "Preflight did not name '$Expected': $reason"
    Assert-Test (-not (Test-Path -LiteralPath ($Fixture.Output+'.zip'))) 'A refused package still produced a ZIP.'
}
function New-PayloadFolder([string]$Name) {
    # A hand-made payload for calling the preflight on its own: every required file, nothing else.
    $root = Join-Path $FixtureRoot "payload-$Name"
    $product = Get-Content -LiteralPath (Join-Path $source 'data/product.json') -Raw | ConvertFrom-Json
    foreach ($relative in Get-PortableRequiredFiles -HubProgram ($product.program_name+'.exe') -ShortId $product.short_id) {
        Write-FixtureFile (Join-Path $root $relative) 'synthetic payload file'
    }
    New-Item -ItemType Directory -Force -Path (Join-Path $root 'user') | Out-Null
    return @{Root=$root;Program=($product.program_name+'.exe');ShortId=$product.short_id}
}
function Invoke-Preflight($Payload,[string[]]$Markers=@()) {
    Assert-PortablePayload -Root $Payload.Root -HubProgram $Payload.Program -ShortId $Payload.ShortId -PrivateMarkers $Markers
}
function Assert-Preflight($Payload,[string]$Expected,[string[]]$Markers=@()) {
    $reason = ''
    try { Invoke-Preflight $Payload $Markers } catch { $reason = $_.Exception.Message }
    Assert-Test ($reason -like '*preflight failed*') "Preflight accepted the payload (wanted '$Expected')."
    Assert-Test ($reason -like "*$Expected*") "Preflight did not name '$Expected': $reason"
    Assert-Test ($reason -notlike "*$($Payload.Root)*") 'Preflight leaked an absolute path.'
}
function Test-Case([string]$Name,[scriptblock]$Body) {
    try { & $Body; Write-Output "PASS $Name" } catch { ++$script:failures; Write-Output "FAIL $Name : $($_.Exception.Message)" }
}
try {
    Test-Case 'canonical tracked emulator catalog ships while reserved and extra metadata refuse' {
        $f=New-Fixture 'emulator-catalog'
        Write-FixtureFile (Join-Path $f.Repo 'data/emulators/synthetic.toml') 'id="synthetic"'
        Save-FixtureRevision $f
        Invoke-Package $f
        Assert-Test (Test-Path -LiteralPath (Join-Path $f.Output 'data/emulators/synthetic.toml')) 'Tracked public emulator catalog was not staged.'
        Add-Type -AssemblyName System.IO.Compression.FileSystem
        $archive=[IO.Compression.ZipFile]::OpenRead($f.Output+'.zip')
        try { Assert-Test (@($archive.Entries | Where-Object { $_.FullName.Replace('\','/') -eq 'package/data/emulators/synthetic.toml' }).Count -eq 1) 'Tracked emulator catalog missing from ZIP.' } finally { $archive.Dispose() }
        $index=0
        foreach($reserved in @('games/emulators/synthetic.toml','data/nested/emulators/synthetic.toml','data/emulators/nested/synthetic.toml','data/emulators/user/synthetic.toml','data/emulators/installed/synthetic.toml','data/emulators/.private/synthetic.toml','data/user/synthetic.toml','data/installed/synthetic.toml','data/.private/synthetic.toml')) {
            $negative=New-Fixture ('reserved-catalog-'+$index);$index++
            Write-FixtureFile (Join-Path $negative.Repo $reserved) 'synthetic reserved metadata'
            Save-FixtureRevision $negative
            Assert-RefusedBeforeOutput $negative
        }
        $extra=New-Fixture 'untracked-emulator-catalog'
        Write-FixtureFile (Join-Path $extra.Repo 'data/emulators/untracked.toml') 'id="untracked"'
        Assert-RefusedBeforeOutput $extra
    }
    Test-Case 'preflight accepts a complete payload and names a missing runtime file' {
        $p = New-PayloadFolder 'complete'
        Invoke-Preflight $p
        foreach ($missing in @('Qt6Quick.dll','platforms/qwindows.dll','vcruntime140_1.dll','openvr_api.dll',"resources/$($p.ShortId).vrmanifest",'resources/hub-actions.json','data/product.json','BUILD-INFO.txt')) {
            $q = New-PayloadFolder ('missing-'+($missing -replace '[^A-Za-z0-9]','-'))
            Remove-Item -LiteralPath (Join-Path $q.Root $missing.Replace('/','\'))
            Assert-Preflight $q "missing runtime file: $missing"
        }
        $empty = New-PayloadFolder 'empty'
        [IO.File]::WriteAllText((Join-Path $empty.Root 'Qt6Core.dll'),'')
        Assert-Preflight $empty 'empty runtime file: Qt6Core.dll'
    }
    Test-Case 'preflight refuses a private path and names the file and line, not the path' {
        # Built from pieces so this test file holds no private-looking path itself.
        $drive = 'C' + ':' + [char]92 + 'Synthetic' + [char]92 + 'work'
        $userFolder = '/' + 'Users' + '/synthetic/work'
        foreach ($text in @(("line one`nroot = "+$drive),("x`ny`nplace: file:///C"+':/synthetic/x'),("path "+$userFolder+"/file"),("a = D"+':'+[char]92+'x'))) {
            $p = New-PayloadFolder 'private-path'
            Write-FixtureFile (Join-Path $p.Root 'data/note.toml') $text
            Assert-Preflight $p 'private path (a drive or user folder) in data/note.toml, line'
            $reason = ''; try { Invoke-Preflight $p } catch { $reason = $_.Exception.Message }
            Assert-Test ($reason -notlike '*Synthetic*' -and $reason -notlike '*synthetic/work*') 'Preflight repeated the private text.'
            Remove-Item -LiteralPath $p.Root -Recurse -Force
        }
        $p = New-PayloadFolder 'private-line'
        Write-FixtureFile (Join-Path $p.Root 'BUILD-INFO.txt') ("ok`nfine`nbuilt in "+$drive)
        Assert-Preflight $p 'in BUILD-INFO.txt, line 3'
        $m = New-PayloadFolder 'marker'
        Write-FixtureFile (Join-Path $m.Root 'BUILD-INFO.txt') 'built in /workspace/synthetic-builder/repo'
        Assert-Preflight $m "this PC's build path in BUILD-INFO.txt, line 1" @('/workspace/synthetic-builder','')
        # URLs, JSON-escaped URLs and third-party notices are not private paths.
        $ok = New-PayloadFolder 'public-text'
        Write-FixtureFile (Join-Path $ok.Root 'data/links.json') '{"a":"https://example.org/x","b":"http:\/\/example.org\/y","c":"7-Zip C SDK"}'
        Write-FixtureFile (Join-Path $ok.Root 'licenses/Qt-sbom/sample.spdx.json') ('{"path":"'+$drive.Replace('\','/')+'"}')
        Invoke-Preflight $ok
    }
    Test-Case 'preflight refuses games, BIOS, keys, archives and personal data in the payload' {
        foreach ($intruder in @('resources/synthetic game.iso','data/disc.chd','data/Track 01.bin','games/rom.z64','games/pack.zip','games/pack.7z','data/prod.keys','data/save.srm',
                                'data/scph5501.rom','data/ps2-bios.dat','data/dc_boot.dat','user/settings.json','user/library/cache.json')) {
            $p = New-PayloadFolder ('intruder-'+($intruder -replace '[^A-Za-z0-9]','-'))
            Write-FixtureFile (Join-Path $p.Root $intruder) 'synthetic stand-in; no game content'
            Assert-Preflight $p $intruder
        }
        # Several problems are all reported in one go.
        $p = New-PayloadFolder 'several'
        Remove-Item -LiteralPath (Join-Path $p.Root 'Qt6Gui.dll')
        Write-FixtureFile (Join-Path $p.Root 'data/disc.iso') 'synthetic'
        $reason = ''; try { Invoke-Preflight $p } catch { $reason = $_.Exception.Message }
        Assert-Test ($reason -like '*Qt6Gui.dll*' -and $reason -like '*data/disc.iso*') "Not every problem was listed: $reason"
    }
    Test-Case 'packaging stops before the ZIP for a missing runtime file, a private path and a player file' {
        $f = New-Fixture 'preflight-missing-qt'
        [IO.File]::WriteAllLines($f.DeployList,[string[]]@(Get-PortableQtRuntimeFiles | Where-Object { $_ -ne 'Qt6Quick.dll' }),[Text.UTF8Encoding]::new($false))
        Assert-PreflightRefuses $f 'missing runtime file: Qt6Quick.dll'
        $f = New-Fixture 'preflight-missing-resource'
        Remove-Item -LiteralPath (Join-Path $f.Build 'bin/resources/hub-actions.json')
        Assert-PreflightRefuses $f 'missing runtime file: resources/hub-actions.json'
        $f = New-Fixture 'preflight-private-path'
        Write-FixtureFile (Join-Path $f.Build 'bin/resources/sample.txt') ('built in C'+':'+[char]92+'synthetic'+[char]92+'checkout')
        Assert-PreflightRefuses $f 'private path (a drive or user folder) in resources/sample.txt, line 1'
        $f = New-Fixture 'preflight-build-path'
        Write-FixtureFile (Join-Path $f.Build 'bin/resources/sample.txt') ("copied from "+$f.Repo)
        Assert-PreflightRefuses $f 'in resources/sample.txt, line 1'
        $f = New-Fixture 'preflight-game-file'
        Write-FixtureFile (Join-Path $f.Build 'bin/resources/synthetic.iso') 'synthetic stand-in; no game content'
        Assert-PreflightRefuses $f 'game, firmware, key or archive file in the package: resources/synthetic.iso'
    }
    Test-Case 'missing four-EXE provenance' {
        $f=New-Fixture 'missing-provenance'
        Remove-Item -LiteralPath (Join-Path $f.Build 'generated/portable-build.json')
        Assert-RefusedBeforeOutput $f
    }
    Test-Case 'mixed helper binary' {
        $f=New-Fixture 'mixed-helper'
        Write-FixtureFile (Join-Path $f.Build 'bin/hubtool.exe') 'different synthetic helper'
        Assert-RefusedBeforeOutput $f
    }
    Test-Case 'extra untracked metadata' {
        $f=New-Fixture 'extra-metadata'
        Write-FixtureFile (Join-Path $f.Repo 'data/private-receipt.json') '{"syntheticPrivate":true}'
        Assert-RefusedBeforeOutput $f
    }
    Test-Case 'existing archive' {
        $f=New-Fixture 'existing-archive'
        Write-FixtureFile ($f.Output+'.zip') 'existing synthetic archive'
        Assert-RefusedBeforeOutput $f
    }
    Test-Case 'default package keeps optional native absent' {
        $f=New-Fixture 'default'
        Invoke-Package $f
        Assert-Test (Test-Path -LiteralPath ($f.Output+'.zip')) 'Default ZIP is missing.'
        Assert-Test (Test-Path -LiteralPath (Join-Path $f.Output 'data/nested/ordinary.toml')) 'Nested tracked metadata is missing.'
        Assert-Test (-not (Test-Path -LiteralPath (Join-Path $f.Output 'optional'))) 'Default package added native staging.'
        Assert-Test (-not (Test-Path -LiteralPath (Join-Path $f.Output 'generated/portable-build.json'))) 'Private build receipt was copied.'
        Assert-Test (Test-Path -LiteralPath (Join-Path $f.Output 'licenses/Qt-sbom/sample.spdx.json')) 'Qt SBOM was dropped.'
        Assert-Test ((Get-Content (Join-Path $f.Output 'BUILD-INFO.txt') -Raw).Contains($f.Revision)) 'Wrong source provenance.'
    }
    Test-Case 'native optional payload is archived without selecting it' {
        $f=New-Fixture 'native-optional';$native=Add-NativeFixture $f
        Invoke-Package $f $native
        $destination=Join-Path $f.Output 'optional/gun-room-calibration/acvr_gun_room.exe'
        Assert-Test ((Get-FileHash $destination).Hash.ToLowerInvariant() -ceq $native.NativeRoomSha256) 'Native bytes changed.'
        Assert-Test (Test-Path -LiteralPath ($destination+'.launch.json')) 'Minimal native capability receipt missing.'
        Assert-Test (Test-Path -LiteralPath (Join-Path $f.Output 'assets/hands/grip-hand-left.toml')) 'Hand motions missing.'
        Assert-Test (Test-Path -LiteralPath (Join-Path $f.Output 'assets/guns/LICENSE')) 'Authored asset notice missing.'
        Assert-Test (@(Get-ChildItem -LiteralPath (Join-Path $f.Output 'user') -Force).Count -eq 0) 'Native selection/preferences were added.'
        Add-Type -AssemblyName System.IO.Compression.FileSystem
        $archive=[IO.Compression.ZipFile]::OpenRead($f.Output+'.zip')
        try { Assert-Test (@($archive.Entries | Where-Object {$_.FullName.Replace('\','/').EndsWith('/optional/gun-room-calibration/acvr_gun_room.exe')}).Count -eq 1) 'Native staging happened after archive creation.' } finally {$archive.Dispose()}
    }
    Test-Case 'native capability types and private receipt fields are refused' {
        foreach ($mode in @('false','string','number','missing','extra','case')) {
            $f=New-Fixture ('native-'+$mode);$native=Add-NativeFixture $f
            $path=$native.NativeRoomExecutable+'.launch.json'
            $receipt=Get-Content -LiteralPath $path -Raw | ConvertFrom-Json
            if($mode -eq 'false'){$receipt.calibrate_grip=$false}
            if($mode -eq 'string'){$receipt.calibrate_grip='true'}
            if($mode -eq 'number'){$receipt.calibrate_grip=1}
            if($mode -eq 'missing'){$receipt.PSObject.Properties.Remove('calibrate_grip')}
            if($mode -eq 'extra'){$receipt | Add-Member -NotePropertyName private_path -NotePropertyValue 'synthetic-private'}
            if($mode -eq 'case'){$receipt.PSObject.Properties.Remove('calibrate_grip');$receipt | Add-Member -NotePropertyName CALIBRATE_GRIP -NotePropertyValue $true}
            $receipt | ConvertTo-Json | Set-Content -LiteralPath $path -Encoding utf8
            $refused=$false;try{Invoke-Package $f $native}catch{$refused=$true}
            Assert-Test ($refused -and -not (Test-Path -LiteralPath $f.Output)) "Native receipt $mode was not refused before output."
        }
    }
    Test-Case 'metadata junction is refused without visiting target' {
        $f=New-Fixture 'junction'
        $target=Join-Path $f.Root 'private';New-Item -ItemType Directory -Path $target | Out-Null
        Write-FixtureFile (Join-Path $target 'private.json') '{"syntheticPrivate":true}'
        New-Item -ItemType Junction -Path (Join-Path $f.Repo 'data/escape') -Target $target | Out-Null
        Assert-RefusedBeforeOutput $f
    }
    Test-Case 'actual build helper captures four changed outputs and failed build refuses receipt' {
        foreach($mode in @('override','default','failure')) {
            $fail=$mode -eq 'failure'
            $f=New-Fixture ('producer-'+$mode)
            Remove-Item -LiteralPath (Join-Path $f.Build 'generated/portable-build.json')
            Set-FixtureFetchContentCache $f ($mode -ne 'default')
            $global:PortableFakeBuild=$f;$global:PortableFakeFail=$fail;$global:PortableFakeCalls=@()
            function global:cmake {
                $global:PortableFakeCalls+=,@($args)
                if($args[0] -eq '--build') {
                    if($global:PortableFakeFail){$global:LASTEXITCODE=9;return}
                    foreach($name in @($global:PortableFakeBuild.Program,'hubtool.exe','arttool.exe','steamtool.exe')) {
                        Write-FixtureFile (Join-Path $global:PortableFakeBuild.Build "bin/$name") "new synthetic build $name"
                    }
                }
                $global:LASTEXITCODE=0
            }
            $blocked=$false
            try { & (Join-Path $f.Repo 'hub/packaging/build-portable-inputs.ps1') -RepoRoot $f.Repo -BuildDir $f.Build | Out-Null } catch {$blocked=$true} finally {Remove-Item Function:cmake}
            $receiptPath=Join-Path $f.Build 'generated/portable-build.json'
            if($fail){Assert-Test ($blocked -and -not (Test-Path $receiptPath)) 'Failed compiler produced valid provenance.';continue}
            Assert-Test (-not $blocked) 'Successful inert compiler provider did not produce provenance.'
            $built=Get-Content $receiptPath -Raw | ConvertFrom-Json
            Assert-Test (@($built.binaries.PSObject.Properties).Count -eq 4) 'Missing four-EXE build hashes.'
            Assert-Test ($global:PortableFakeCalls.Count -eq 2) 'Configure/build provider was not invoked.'
            Assert-Test (($global:PortableFakeCalls[1] -join ' ').Contains('--parallel 2 --target aladdinscastle-hub hubtool arttool steamtool')) 'Shipping targets/CPU bound were not passed to compiler.'
            Invoke-Package $f
            # Windows CI asks the same producer for all targets. Its receipt must
            # still bind each of the four shipping binaries, without launches.
            $global:PortableFakeCalls=@();$global:PortableFakeFail=$false
            function global:cmake {$global:PortableFakeCalls+=,@($args);$global:LASTEXITCODE=0}
            try { & (Join-Path $f.Repo 'hub/packaging/build-portable-inputs.ps1') -RepoRoot $f.Repo -BuildDir $f.Build -AllTargets | Out-Null } finally {Remove-Item Function:cmake}
            $all=Get-Content $receiptPath -Raw | ConvertFrom-Json
            Assert-Test ($all.all_targets -is [bool] -and $all.all_targets) 'CI all-target build mode was not recorded.'
            Assert-Test (@($all.binaries.PSObject.Properties).Count -eq 4) 'CI receipt omitted shipping outputs.'
            Assert-Test (($global:PortableFakeCalls[1] -join ' ').EndsWith('--parallel 2')) 'CI producer did not use the bounded all-target command.'
        }
    }
    Test-Case 'writable build junctions refuse before any configure/build call' {
        foreach($leaf in @('generated','bin')) {
            $f=New-Fixture ('build-junction-'+$leaf)
            Set-FixtureFetchContentCache $f $true
            $original=(Resolve-Path -LiteralPath (Join-Path $f.Build $leaf)).Path
            $saved=[IO.Path]::GetFullPath((Join-Path $f.Build ($leaf+'-original')))
            $prefix=[IO.Path]::GetFullPath($f.Build).TrimEnd('\','/')+'\'
            if(-not $original.StartsWith($prefix,[StringComparison]::OrdinalIgnoreCase) -or -not $saved.StartsWith($prefix,[StringComparison]::OrdinalIgnoreCase)){throw 'Fixture move escaped its build workspace.'}
            Move-Item -LiteralPath $original -Destination $saved
            $external=Join-Path $f.Root 'external';New-Item -ItemType Directory -Path $external | Out-Null
            Write-FixtureFile (Join-Path $external 'sentinel.txt') 'unchanged external sentinel'
            New-Item -ItemType Junction -Path $original -Target $external | Out-Null
            $global:PortableCmakeSentinel=Join-Path $f.Root 'cmake-called'
            function global:cmake {Write-FixtureFile $global:PortableCmakeSentinel 'called';$global:LASTEXITCODE=0}
            $reason=''
            try{& (Join-Path $f.Repo 'hub/packaging/build-portable-inputs.ps1') -RepoRoot $f.Repo -BuildDir $f.Build | Out-Null}catch{$reason=$_.Exception.Message}finally{Remove-Item Function:cmake}
            Assert-Test ($reason -like '*Linked*') 'Producer did not specifically refuse the build junction.'
            Assert-Test (-not (Test-Path -LiteralPath $global:PortableCmakeSentinel)) 'CMake was invoked before rejecting writable junction.'
            Assert-Test ((Get-Content (Join-Path $external 'sentinel.txt') -Raw) -ceq 'unchanged external sentinel') 'External target was modified.'
            Assert-Test (@(Get-ChildItem -LiteralPath $external -Force).Count -eq 1) 'Producer wrote through the junction.'
        }
    }
    Write-Output "Windows PowerShell $($PSVersionTable.PSVersion): $failures failed"
    if ($failures) { exit 1 }
} finally { $env:GITHUB_SHA = $oldCiRevision }
