[CmdletBinding()]
param([Parameter(Mandatory)][string]$FixtureRoot,[switch]$FreshDeployProbe)
$ErrorActionPreference = 'Stop'
if ($PSVersionTable.PSVersion.Major -ne 5) { throw 'Run this regression with Windows PowerShell 5.1.' }
if (Test-Path -LiteralPath $FixtureRoot) { throw 'Use a new fixture folder.' }
$source = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../../..'))
$probe = Join-Path $source '.local/wii/ps51-probe.exe'
New-Item -ItemType Directory -Path $FixtureRoot | Out-Null
if ($FreshDeployProbe -or -not (Test-Path -LiteralPath $probe)) {
    # CI has no private local artifacts. Its built-in compiler makes only this
    # inert console fixture, never a product, SDK or third-party dependency.
    $probe = Join-Path $FixtureRoot 'inert-deploy.exe'
    Add-Type -TypeDefinition 'public class PortableDeployFixture { public static int Main(string[] args) { return 0; } }' -Language CSharp -OutputAssembly $probe -OutputType ConsoleApplication
}
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
    Copy-Item -LiteralPath $probe -Destination (Join-Path $root 'qt/bin/windeployqt.exe')
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
    return @{Root=$root;Repo=$repo;Build=$build;Revision=$revision;Receipt=$receipt;Program=$program;Output=(Join-Path $root 'package')}
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
function Assert-RefusedBeforeOutput($Fixture) {
    $refused = $false
    try { Invoke-Package $Fixture } catch { $refused = $true }
    Assert-Test $refused 'Unsafe package inputs were accepted.'
    Assert-Test (-not (Test-Path -LiteralPath $Fixture.Output)) 'Refusal created an output folder.'
}
function Test-Case([string]$Name,[scriptblock]$Body) {
    try { & $Body; Write-Output "PASS $Name" } catch { ++$script:failures; Write-Output "FAIL $Name : $($_.Exception.Message)" }
}
try {
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
        foreach($fail in @($false,$true)) {
            $f=New-Fixture ('producer-'+$fail)
            Remove-Item -LiteralPath (Join-Path $f.Build 'generated/portable-build.json')
            foreach($dependency in @('tomlplusplus','nlohmann_json','json_schema_validator','openvr','libarchive','xz','zlib','chdr')) {
                $dir=Join-Path $f.Root "deps/$dependency";New-Item -ItemType Directory -Force -Path $dir | Out-Null
                Add-Content -LiteralPath (Join-Path $f.Build 'CMakeCache.txt') -Value "${dependency}_SOURCE_DIR:STATIC=$dir"
            }
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
    Write-Output "Windows PowerShell $($PSVersionTable.PSVersion): $failures failed"
    if ($failures) { exit 1 }
} finally { $env:GITHUB_SHA = $oldCiRevision }
