# SPDX-License-Identifier: GPL-3.0-only
# Runs one program under a deadline and, on timeout, ends it together with everything it started.
# CMake's execute_process TIMEOUT ends only the direct child, so a Hub or stand-in left behind by a stuck
# test kept its log and temporary files locked. Used by RunQtTest.cmake on Windows.
# The settings come from the environment so that no argument needs quoting:
#   AC_RUN_EXE      the program
#   AC_RUN_ARGS     its command line, already quoted
#   AC_RUN_TIMEOUT  the deadline in seconds
$ErrorActionPreference = 'Stop'
$info = New-Object System.Diagnostics.ProcessStartInfo
$info.FileName = $env:AC_RUN_EXE
$info.Arguments = $env:AC_RUN_ARGS
$info.UseShellExecute = $false # No redirection: the child shares this process's output and error handles.
$process = [System.Diagnostics.Process]::Start($info)
if (-not $process.WaitForExit([int]([double]$env:AC_RUN_TIMEOUT * 1000))) {
    & "$env:SystemRoot\System32\taskkill.exe" /T /F /PID $process.Id | Out-Null
    [void]$process.WaitForExit(15000)
    [Console]::Error.WriteLine("Process tree ended after the timeout of $($env:AC_RUN_TIMEOUT) s.")
    exit 124
}
exit $process.ExitCode
