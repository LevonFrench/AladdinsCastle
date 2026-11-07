# SPDX-License-Identifier: GPL-3.0-only
# Executes the production cleanup statements and Invoke-Hub functions against throwaway PowerShell processes.
# -SourcePath can point at a baseline Test-FreshInstall.ps1; its unsafe name-based cleanup must fail this check.
[CmdletBinding()]
param([string]$SourcePath)
$ErrorActionPreference = 'Stop'
if ($PSVersionTable.PSVersion.Major -ne 5) { throw 'Run this check with Windows PowerShell 5.1.' }
if (-not $SourcePath) { $SourcePath = Join-Path $PSScriptRoot 'Test-FreshInstall.ps1' }
$tokens = $null; $parseErrors = $null
$ast = [Management.Automation.Language.Parser]::ParseFile([IO.Path]::GetFullPath($SourcePath), [ref]$tokens, [ref]$parseErrors)
if ($parseErrors.Count) { throw ($parseErrors | Out-String) }
$source = [IO.File]::ReadAllText([IO.Path]::GetFullPath($SourcePath))
$start = $source.IndexOf('$stillRunning = @(')
$end = $source.IndexOf('$removed = $false', $start)
if ($start -lt 0 -or $end -lt 0) { throw 'Cannot locate the production uninstall cleanup statements.' }
$cleanup = [scriptblock]::Create($source.Substring($start, $end - $start))
$fixtures = New-Object 'Collections.Generic.List[System.Diagnostics.Process]'
$hubProcesses = New-Object 'Collections.Generic.List[System.Diagnostics.Process]'
$work = Join-Path ([IO.Path]::GetTempPath()) ('fresh-ownership-' + [guid]::NewGuid().ToString('N'))
New-Item -ItemType Directory -Path $work | Out-Null
function Assert([bool]$Ok, [string]$Message) { if (-not $Ok) { throw $Message }; Write-Host "[pass] $Message" }
function New-Fixture([bool]$ExitImmediately = $false) {
    $command = if ($ExitImmediately) { 'exit 0' } else { 'Start-Sleep -Seconds 180' }
    $encoded = [Convert]::ToBase64String([Text.Encoding]::Unicode.GetBytes($command))
    $p = Microsoft.PowerShell.Management\Start-Process -FilePath (Join-Path $PSHOME 'powershell.exe') `
        -ArgumentList @('-NoLogo', '-NoProfile', '-NonInteractive', '-EncodedCommand', $encoded) -WindowStyle Hidden -PassThru
    $null = $p.Handle
    $fixtures.Add($p)
    return $p
}
# Baseline discovery and stop commands are constrained to these two synthetic fixtures. They can never see a Hub.
function Get-Process { param($Name, $ErrorAction) return @($unrelated, $owned) }
function Stop-Process {
    param([int]$Id, [switch]$Force, $ErrorAction)
    $matches = @($fixtures | Where-Object { $_.Id -eq $Id })
    if ($matches.Count -ne 1) { throw 'Baseline attempted to stop something outside the fixture set.' }
    if (-not $matches[0].HasExited) { $matches[0].Kill(); $null = $matches[0].WaitForExit(15000) }
}
function Note([string]$Check, [bool]$Ok, [string]$Detail) { $script:cleanupCheck = $Ok; $script:cleanupDetail = $Detail }
# Invoke-Hub still executes its real launch, wait, timeout, environment and finally code. Only executable creation
# is redirected to a retained fixture; its packaged-Hub arguments must never reach a real application.
function Start-Process {
    param($FilePath, $ArgumentList, $WorkingDirectory, $RedirectStandardOutput, $RedirectStandardError, [switch]$PassThru, $WindowStyle)
    if ($script:failLaunch) { throw 'synthetic launch failure' }
    return $script:launchFixture
}
try {
    foreach ($name in @('Stop-HubProcess', 'Invoke-Hub')) {
        $definition = $ast.Find({ param($node) $node -is [Management.Automation.Language.FunctionDefinitionAst] -and $node.Name -eq $name }, $true)
        if ($definition) { . ([scriptblock]::Create($definition.Extent.Text)) }
    }
    $unrelated = New-Fixture
    $owned = New-Fixture
    Assert ($unrelated.ProcessName -eq $owned.ProcessName) 'fixtures have the same executable name'
    $hubExe = $owned.ProcessName + '.exe'
    $hubProcesses.Add($owned)
    $cleanupCheck = $null; $cleanupDetail = $null
    . $cleanup
    Assert (-not $unrelated.HasExited) 'unrelated same-name process survives production uninstall cleanup'
    Assert ($owned.HasExited) 'owned lingering process is stopped by production uninstall cleanup'
    Assert ($cleanupCheck -eq $false -and $cleanupDetail -eq '1 running') 'owned lingering process is reported as a failed uninstall assertion'

    $ArtifactDir = $work; $WorkDir = $work; $package = $work; $sandbox = @{}
    $HubTimeoutSeconds = 0
    $script:launchFixture = New-Fixture
    $beforeCount = $hubProcesses.Count
    $run = Invoke-Hub 'timeout' @{}
    Assert ($run.TimedOut -and $run.ExitCode -eq -1) 'production Invoke-Hub preserves timeout failure'
    Assert ($hubProcesses.Count -eq ($beforeCount + 1) -and [object]::ReferenceEquals($hubProcesses[$beforeCount], $script:launchFixture)) 'Invoke-Hub retains the exact launched Process object'
    Assert ($script:launchFixture.HasExited -and -not $unrelated.HasExited) 'timeout cleanup stops only the owned fixture'

    $script:launchFixture = New-Fixture $true
    Assert ($script:launchFixture.WaitForExit(15000)) 'normal-exit fixture exits within the test bound'
    $HubTimeoutSeconds = 15
    $run = Invoke-Hub 'normal-exit' @{}
    Assert (-not $run.TimedOut -and $run.ExitCode -eq 0) 'production Invoke-Hub preserves successful normal exit'
    $beforeEnv = [Environment]::GetEnvironmentVariable('AC_UI_CAPTURE', 'Process')
    $beforeCount = $hubProcesses.Count
    $script:failLaunch = $true
    $caught = $false
    try { $null = Invoke-Hub 'launch-error' @{} } catch { $caught = $_.Exception.Message -eq 'synthetic launch failure' }
    Assert ($caught -and $hubProcesses.Count -eq $beforeCount) 'launch failure propagates without adopting any process'
    Assert ([Environment]::GetEnvironmentVariable('AC_UI_CAPTURE', 'Process') -eq $beforeEnv) 'launch failure restores the original environment'
    Assert (-not $unrelated.HasExited) 'unrelated same-name fixture survives every Invoke-Hub path'
    # Execute the actual outer finally, including disposal, as an assertion/receipt exception would.
    $outer = $ast.Find({ param($node) $node -is [Management.Automation.Language.TryStatementAst] -and $node.Finally -and $node.Finally.Extent.Text -match 'foreach \(\$p in \$hubProcesses\)' }, $true)
    Assert ($null -ne $outer) 'production has an outer exceptional-exit cleanup'
    $script:launchFixture = New-Fixture
    $hubProcesses.Add($script:launchFixture)
    $observer = [Diagnostics.Process]::GetProcessById($script:launchFixture.Id)
    $null = $observer.Handle
    $finallyText = $outer.Finally.Extent.Text
    $outerCleanup = [scriptblock]::Create($finallyText.Substring(1, $finallyText.Length - 2))
    try {
        try { throw 'synthetic receipt failure' } finally { . $outerCleanup }
    } catch { if ($_.Exception.Message -ne 'synthetic receipt failure') { throw } }
    try { Assert ($observer.WaitForExit(15000) -and -not $unrelated.HasExited) 'outer finally stops the exact owned process after an exception' } finally { $observer.Dispose() }
    Write-Output 'Fresh-install ownership regression passed.'
} finally {
    # Fixture-only emergency cleanup uses the original retained handles, including when testing the unsafe baseline.
    foreach ($p in $fixtures) {
        try { if (-not $p.HasExited) { $p.Kill(); $null = $p.WaitForExit(15000) } } catch [ObjectDisposedException] { }
        catch [InvalidOperationException] { } # The production outer finally already disposed an owned Process.
        finally { $p.Dispose() }
    }
    Remove-Item -LiteralPath $work -Recurse -Force
}
