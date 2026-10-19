# Builds the patched Supermodel solution at low priority. Windows PowerShell 5.1.
#   -Source   the Supermodel source root (with the patch already applied)
#   -MSBuild  path to MSBuild.exe of a Visual Studio that has the solution's platform toolset
#   -OutName  optional: build into MSVC\x64\<OutName>\ so a running copy is not replaced
param(
    [Parameter(Mandatory = $true)][string]$Source,
    [Parameter(Mandatory = $true)][string]$MSBuild,
    [string]$OutName = '',
    [int]$Jobs = 2                                    # parallel projects; keep it low so the PC stays usable
)
$ErrorActionPreference = 'Stop'
$log = Join-Path $Source 'acvr-build.log'
if ($OutName) {
    $target = Join-Path $Source 'MSVC\Supermodel.vcxproj'
    $extra = @("/p:OutDir=$(Join-Path $Source "MSVC\x64\$OutName")\", "/p:SolutionDir=$(Join-Path $Source 'MSVC')\")
} else {
    $target = Join-Path $Source 'MSVC\Supermodel.sln'
    $extra = @()
}
$arguments = @("`"$target`"", '/p:Configuration=Release', '/p:Platform=x64', "/m:$Jobs", '/v:minimal', '/nologo', '/fl', "/flp:logfile=$log;verbosity=minimal") + $extra
$p = Start-Process -FilePath $MSBuild -ArgumentList $arguments -PassThru -WindowStyle Hidden
try { $p.PriorityClass = 'BelowNormal' } catch {}
$p.WaitForExit()
"msbuild exit: $($p.ExitCode)"
Get-Content $log | Select-String -Pattern ' error ' | Select-Object -First 12 | ForEach-Object { $_.Line }
exit $p.ExitCode
