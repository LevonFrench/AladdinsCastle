#Requires -Version 5.1
# Pure helpers; no process, socket, settings or desktop activity.
function Get-AcPhase1Arguments($Run, [string]$UserRoot) {
    $nativeModes = @{'native-off'=0;'native-auto'=-1;'native-72'=72;'native-90'=90;'native-120'=120}
    $native = $nativeModes.ContainsKey($Run.experiment)
    $vr = if ($native) { 'True' } else { 'False' }
    $code = if ($Run.experiment -eq 'baseline' -or $native) { 'False' } else { 'True' }
    $vbi = if ($Run.experiment -eq 'code-vbi90') { 'True' } else { 'False' }
    $ratio = if ($vbi -eq 'True') { '1.5' } else { '1.0' }
    $values = @('--user', $UserRoot, '--batch', '--exec', $Run.disc,
        '-C',"Graphics.VR.EnableOpenXR=$vr", '-C','Graphics.Hardware.VSync=False',
        '-C','Dolphin.Core.EmulationSpeed=1.0', '-C','Dolphin.Core.OverclockEnable=False',
        '-C','Dolphin.Core.Overclock=1.0', '-C','SYSCONF.IPL.E60=True', '-C','Dolphin.Core.GFXBackend=D3D',
        '-C',"Dolphin.Core.EnableCheats=$code", '-C',"Dolphin.Core.VIOverclockEnable=$vbi", '-C',"Dolphin.Core.VIOverclock=$ratio")
    if ($native) {
        $values += @('-C',"Graphics.VR.ForcedVBIFrequency=$($nativeModes[$Run.experiment])",
            '-C','Graphics.VR.AutoVBIFromHMD=False', '-C','Graphics.VR.EagerHeartbeat=False', '-C','Graphics.VR.MirrorView=0')
    }
    return $values
}

function Join-AcPhase1Arguments([string[]]$Arguments) {
    # Direct CreateProcess command line, never passed through a shell.
    $quoted = foreach ($argument in $Arguments) {
        if ($null -eq $argument -or $argument -match '["\r\n\x00]') { throw 'Unsupported character in process argument' }
        $escaped = [regex]::Replace($argument, '\\+$', '$0$0')
        '"' + $escaped + '"'
    }
    return $quoted -join ' '
}

function Get-AcPhase1ByteHash([byte[]]$Bytes) {
    $hash = [Security.Cryptography.SHA256]::Create()
    try { return ([BitConverter]::ToString($hash.ComputeHash($Bytes))).Replace('-','').ToLowerInvariant() }
    finally { $hash.Dispose() }
}

function Assert-AcPhase1DsuBinding($Ready, $InputContract, $Lease, $Producer, [string]$ProducerExecutableHash) {
    # Inputs are snapshots supplied by the collector. This helper has no runtime activity.
    if ($InputContract.ready_protocol -ne 1 -or
        $InputContract.session_token -isnot [string] -or $InputContract.session_token -cnotmatch '^[0-9a-f]{32}$' -or
        $InputContract.script_sha256 -isnot [string] -or $InputContract.script_sha256 -cnotmatch '^[0-9a-f]{64}$' -or
        $InputContract.producer_executable_sha256 -isnot [string] -or $InputContract.producer_executable_sha256 -cnotmatch '^[0-9a-f]{64}$' -or
        -not $InputContract.producer_executable) { throw 'Missing attached DSU producer contract; prepare fresh input' }
    if ($Ready.ready -isnot [bool] -or $Ready.ready -ne $true -or
        ($Ready.producer_pid -isnot [int] -and $Ready.producer_pid -isnot [long]) -or $Ready.producer_pid -le 0 -or
        ($Lease.dsu_server_pid -isnot [int] -and $Lease.dsu_server_pid -isnot [long]) -or
        $Ready.producer_pid -ne $Lease.dsu_server_pid -or
        ($Ready.port -isnot [int] -and $Ready.port -isnot [long]) -or $Ready.port -ne $InputContract.port -or
        $Ready.script_sha256 -cne $InputContract.script_sha256 -or
        $Ready.session_token -cne $InputContract.session_token -or
        $Lease.dsu_session_token -cne $InputContract.session_token) { throw 'DSU ready receipt does not bind this run, timeline and lease' }
    if (-not $Producer -or $Producer.Id -ne $Ready.producer_pid -or -not $Producer.Path -or
        [IO.Path]::GetFullPath($Producer.Path) -ine [IO.Path]::GetFullPath($InputContract.producer_executable) -or
        $ProducerExecutableHash -cne $InputContract.producer_executable_sha256) {
        throw 'DSU ready producer is absent or differs from the pinned interpreter'
    }
}
