# SPDX-License-Identifier: GPL-3.0-or-later
param(
 [Parameter(Mandatory=$true)][ValidateSet('ptblank2','lethalen','term2la3','bbusters','zeropnt')][string]$Game,
 [Parameter(Mandatory=$true)][string]$MameExe,
 [Parameter(Mandatory=$true)][string]$ViewerExe,
 [Parameter(Mandatory=$true)][string]$RomPath,
 [Parameter(Mandatory=$true)][string]$GunModel,
 [Parameter(Mandatory=$true)][string]$GunMetadata,
 [Parameter(Mandatory=$true)][string]$WorkingDirectory,
 [ValidateRange(1,86400)][int]$Seconds=3600,
 [string]$StopFile='',
 [switch]$Crt
)
$ErrorActionPreference='Stop'
foreach($file in @($MameExe,$ViewerExe,$GunModel,$GunMetadata)) {
 if(-not (Test-Path -LiteralPath $file -PathType Leaf)){throw "Missing local file: $file"}
}
New-Item -ItemType Directory -Force -Path $WorkingDirectory | Out-Null
$home=(Resolve-Path -LiteralPath $WorkingDirectory).Path
$run=Join-Path $home ('runs\'+[DateTime]::UtcNow.ToString('yyyyMMdd-HHmmss-fff'))
New-Item -ItemType Directory -Force -Path $run | Out-Null
if(-not $StopFile){$StopFile=Join-Path $run 'quit.request'}
if(Test-Path -LiteralPath $StopFile){throw "Stop file already exists: $StopFile"}
$env:MAME_VR_PUBLISH='1';$env:MAME_VR_GUN='1';$env:MAME_VR_CROSSHAIR='1'
# All writable MAME paths stay private. ROM paths are inputs only.
$argsM=@($Game,'-noreadconfig','-rompath',('"'+$RomPath+'"'),'-video','none','-sound','none','-numprocessors','2','-skip_gameinfo','-homepath',('"'+$home+'"'))
foreach($pair in @(@('cfg_directory','cfg'),@('nvram_directory','nvram'),@('snapshot_directory','snap'),@('diff_directory','diff'),@('input_directory','inp'),@('state_directory','sta'),@('share_directory','share'))) {
 $argsM+=@('-'+$pair[0],('"'+(Join-Path $home $pair[1])+'"'))
}
$mame=$null;$viewer=$null;$forced=@()
try {
 $mame=Start-Process -FilePath $MameExe -ArgumentList $argsM -WorkingDirectory $run -WindowStyle Hidden -PassThru -RedirectStandardOutput (Join-Path $run 'mame.log') -RedirectStandardError (Join-Path $run 'mame-errors.log')
 $mame.PriorityClass='BelowNormal';$mameHandle=$mame.Handle
 $argsV=@('--pid',$mame.Id,'--model',('"'+$GunModel+'"'),'--metadata',('"'+$GunMetadata+'"'),'--seconds',$Seconds,'--stop-file',('"'+$StopFile+'"'))
 if($Crt){$argsV+='--crt'}
 $viewer=Start-Process -FilePath $ViewerExe -ArgumentList $argsV -WorkingDirectory $run -WindowStyle Hidden -PassThru -RedirectStandardOutput (Join-Path $run 'viewer.log') -RedirectStandardError (Join-Path $run 'viewer-errors.log')
 $viewer.PriorityClass='BelowNormal';$viewerHandle=$viewer.Handle
 $deadline=[DateTime]::UtcNow.AddSeconds($Seconds+30)
 while(-not $mame.HasExited -and -not $viewer.HasExited -and [DateTime]::UtcNow -lt $deadline){Start-Sleep -Milliseconds 200}
 if(-not $mame.HasExited){
  [IO.File]::WriteAllText($StopFile,'quit')
  [void]$mame.WaitForExit(15000)
 }
} finally {
 # Retain the exact Process objects created here; never kill by process name.
 if($mame -and -not $mame.HasExited){$forced+='MAME';Stop-Process -InputObject $mame -Force}
 if($viewer -and -not $viewer.HasExited -and -not $viewer.WaitForExit(10000)){$forced+='viewer';Stop-Process -InputObject $viewer -Force}
 $mameCode=$null;$viewerCode=$null
 if($mame -and $mame.WaitForExit(5000)){$mame.WaitForExit();$mameCode=$mame.ExitCode}
 if($viewer -and $viewer.WaitForExit(5000)){$viewer.WaitForExit();$viewerCode=$viewer.ExitCode}
 @{game=$Game;stop_file=$StopFile;forced=$forced;mame_exit=$mameCode;viewer_exit=$viewerCode;normal_save_eligible=($forced.Count -eq 0 -and $mameCode -eq 0)} | ConvertTo-Json | Set-Content -LiteralPath (Join-Path $run 'close.json')
}
if($forced.Count){throw 'An owned child required forced closure; inspect close.json before claiming NVRAM save.'}
if($mameCode -ne 0 -or $viewerCode -ne 0){throw 'A child reported failure; inspect close.json and the logs.'}
