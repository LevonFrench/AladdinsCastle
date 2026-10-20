# SPDX-License-Identifier: GPL-3.0-or-later
param(
 [Parameter(Mandatory=$true)][string]$Game,
 [Parameter(Mandatory=$true)][string]$MameExe,
 [Parameter(Mandatory=$true)][string]$ViewerExe,
 [Parameter(Mandatory=$true)][string]$RomPath,
 [Parameter(Mandatory=$true)][string]$GunModel,
 [Parameter(Mandatory=$true)][string]$GunMetadata,
 [Parameter(Mandatory=$true)][string]$WorkingDirectory,
 [ValidateRange(1,86400)][int]$Seconds=3600,
 [string]$StopFile='',
 [string]$AimTrim='',
 [switch]$Mute,
 [switch]$OpenFrame,
 [switch]$Crt
)
$ErrorActionPreference='Stop'
# Per-game settings, each measured offline against the game's own cursor or hit marks
# (README "Aim and per-game settings").
#  allow: files missing from the owner's set that MAME 0.289 lists but the driver never
#         reads (PLD/PROM dumps) or that only seed a blank EEPROM; MAME_VR_ALLOW_MISSING.
#  wide:  environment for -OpenFrame (the game's 3D drawn past 4:3, here 16:9), only where
#         a picture showed the added sides filled with game geometry.
#  aim:   per-game trim "x scale,x shift,y scale,y shift" around the centre; MAME_VR_AIM.
#  xhair: '1' shows MAME's crosshair (only where MAME's calibration matches the game), '0' hides it.
$games=@{
 'ptblank2'=@{allow='';aim='';xhair='1'}
 'lethalen'=@{allow='';aim='';xhair='1'}
 'term2la3'=@{allow='';aim='';xhair='1'}
 'bbusters'=@{allow='';aim='';xhair='1'}
 'zeropnt' =@{allow='';aim='';xhair='1'}
 # Time Crisis: own accuracy screen, 9 points within 0.0052 untrimmed; grip = foot pedal
 'timecris'=@{allow='';aim='';xhair='0';wide=@{ACVR_S22_WIDE='107'}}
 # Operation Wolf 3: four PLD dumps absent from older sets; its gun gain is fixed by the game,
 # so the trim sets scale and a one-time in-game calibration (seeded NVRAM) sets the offset
 'opwolf3' =@{allow='d74-09.8,d74-10.40,d74-12.1,d74-13.2';aim='0.7143,0,0.5001,0';xhair='0'}
 # Zombie Raid: game cursor within 0.0068 untrimmed; grip = pump reload
 'zombraid'=@{allow='';aim='';xhair='0'}
 # Rail Chase: sight within 0.003 on the playfield; the 5th value keeps the aim off the gun's
 # extreme readings, which the game takes as off-screen (the HUD band above stays unreachable)
 'rchase'  =@{allow='';aim='1.0768,0.0007,1.3296,-0.0700,0.04';xhair='0'}
 # Terminator 2 LA4: in-game three-target gun calibration (seeded NVRAM) plus trim; grip = bomb
 'term2'   =@{allow='';aim='0.9726,0.0032,0.9623,0.0033';xhair='0'}
 # Lethal Enforcers II: hit sparks within 0.004 after a small shift; off-screen trigger reloads
 'le2'     =@{allow='';aim='1,0.005,1,0.011';xhair='0'}
 # Police Trainer (Rev 1.3): the game's red hit circle within 0.001 after trim
 'policetr13'=@{allow='';aim='1,0.0065,0.9597,-0.0178';xhair='0'}
 # Alien3: The Gun (mounted guns read as AD sticks): sight within 0.005 after a y scale; grip = bomb
 'alien3'  =@{allow='';aim='1,0,1.1789,-0.0012';xhair='0'}
 # Rapid Fire: hit spark within 0.006 after a y shift
 'rapidfir'=@{allow='';aim='1,0,1,-0.0125';xhair='0'}
 # Exidy 440 gun games: the game's own 4x4 shot dot lands on the aim (no trim); NVRAM seeded
 # past the first-boot "E2 PROM initialized" screen. Older sets lack the unused sync PROMs.
 'crossbow'=@{allow='';aim='';xhair='0'}
 'cheyenne'=@{allow='';aim='';xhair='0'}
 'chiller' =@{allow='';aim='';xhair='0'}
 'claypign'=@{allow='';aim='';xhair='0'}
 'combat'  =@{allow='';aim='';xhair='0'}
 'hitnmiss'=@{allow='xbl.12h,xbl.9h,xbl.2h,xml-3k_mmi_6331.bin,xbl.4k,xbl.5k,xbl.6k,xbl.7k,xbl.8k';aim='';xhair='0'}
 'whodunit8'=@{allow='xbl.12h,xbl.9h,xbl.2h,xml-3k_mmi_6331.bin,xbl.4k,xbl.5k,xbl.6k,xbl.7k,xbl.8k';aim='';xhair='0'}
}
if(-not $games.ContainsKey($Game)){throw ('Unknown game key ' + $Game + '; known: ' + (($games.Keys | Sort-Object) -join ', '))}
foreach($file in @($MameExe,$ViewerExe,$GunModel,$GunMetadata)) {
 if(-not (Test-Path -LiteralPath $file -PathType Leaf)){throw "Missing local file: $file"}
}
New-Item -ItemType Directory -Force -Path $WorkingDirectory | Out-Null
# ($home is a read-only automatic variable in Windows PowerShell; never assign it.)
$gameHome=(Resolve-Path -LiteralPath $WorkingDirectory).Path
$run=Join-Path $gameHome ('runs\'+[DateTime]::UtcNow.ToString('yyyyMMdd-HHmmss-fff'))
New-Item -ItemType Directory -Force -Path $run | Out-Null
if(-not $StopFile){$StopFile=Join-Path $run 'quit.request'}
if(Test-Path -LiteralPath $StopFile){throw "Stop file already exists: $StopFile"}
$env:MAME_VR_PUBLISH='1';$env:MAME_VR_GUN='1';$env:MAME_VR_CROSSHAIR=$games[$Game].xhair
Remove-Item Env:MAME_VR_SCRIPT -ErrorAction SilentlyContinue
$env:MAME_VR_ALLOW_MISSING=$games[$Game].allow
$env:MAME_VR_AIM=$(if($AimTrim){$AimTrim}else{$games[$Game].aim})
Remove-Item Env:ACVR_S22_WIDE -ErrorAction SilentlyContinue
if($OpenFrame){
 if(-not $games[$Game].wide){throw ('No open-frame mode proven for ' + $Game)}
 foreach($k in $games[$Game].wide.Keys){Set-Item -Path ('Env:'+$k) -Value $games[$Game].wide[$k]}
}
# All writable MAME paths stay private. ROM paths are inputs only. Sound plays unless -Mute.
$argsM=@($Game,'-noreadconfig','-rompath',('"'+$RomPath+'"'),'-video','none','-sound',$(if($Mute){'none'}else{'auto'}),'-numprocessors','2','-skip_gameinfo','-homepath',('"'+$gameHome+'"'))
foreach($pair in @(@('cfg_directory','cfg'),@('nvram_directory','nvram'),@('snapshot_directory','snap'),@('diff_directory','diff'),@('input_directory','inp'),@('state_directory','sta'),@('share_directory','share'))) {
 $argsM+=@('-'+$pair[0],('"'+(Join-Path $gameHome $pair[1])+'"'))
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
