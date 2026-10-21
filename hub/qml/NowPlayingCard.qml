// SPDX-License-Identifier: GPL-3.0-only
import QtQuick
import QtQuick.Layouts
import AladdinsCastle.Hub
// While a game runs: the whole page becomes its now-playing screen, in the C hero style.
Rectangle {
 id:card
 property bool vrOverlayMode:false
 property var session:uiController.nowPlaying
 visible:session.gameId!==undefined
 implicitHeight:visible?150:0
 color:Theme.get("color.surface.window")
 readonly property string system:{const id=String(session.gameId||"");for(const key of ["model2","ps2","ps1","n64","saturn","dreamcast","system22","pinball"])if(id.startsWith(key+"-"))return key;return id.startsWith("__gun_room")?"catalog":"model3"}
 // A table's backglass, when the Hub has made one.
 readonly property string art:{if(!session.pinballId)return "";for(const row of (uiController.pinballData.tables||[]))if(row.id===session.pinballId&&row.artBackglass)return "file:///"+String(row.artBackglass).replace(/\\/g,"/");return ""}
 // The watchdog's word on the running game: not responding, without a window, or minimised.
 readonly property string trouble:session.running?uiController.gameTrouble:""
 readonly property string troubleText:trouble==="hung"?(session.title||"The game")+" isn't responding.":trouble==="hidden"?(session.title||"The game")+" is running in the background, with no window on the monitor.":trouble==="minimized"?(session.title||"The game")+" is minimised.":""
 readonly property string phase:session.stopping?"Stopping":session.closing?"Closing":session.running?(session.gunRoom?"":"Now playing"):"Starting"
 MouseArea {anchors.fill:parent;acceptedButtons:Qt.AllButtons;onWheel:event=>{event.accepted=true}}
 HeroBanner {anchors.fill:parent;vrOverlayMode:card.vrOverlayMode;system:card.system;art:card.art
  eyebrow:card.trouble==="hung"?"NOT RESPONDING":card.trouble?"IN THE BACKGROUND":card.phase.toUpperCase()
  title:card.session.title||""
  meta:card.troubleText?card.troubleText+(card.trouble==="hung"?" Give it a moment, or end it.":" Bring it to the front, or end it."):card.session.stopping?"The game is asked to close, and ended if it has not closed within a few seconds.":card.session.closing?(card.session.gunRoom?"Waiting for the room to close cleanly…":"Waiting for the game to close cleanly…"):!card.session.running?"Starting the game. It opens in a moment.":(card.session.closeUnavailable?card.session.closeUnavailable+" ":"")+(card.session.hint||"")||(card.session.gunRoom?"Recentre is not available from the Hub for this room.":"Use the game's own recentre control.")
  PillButton {objectName:"nowPlayingExit";targetHeight:card.vrOverlayMode?56:48;neon:true;accent:Theme.get("color.text.primary");text:card.session.gunRoom?"Exit room":"Exit game";vrOverlayMode:card.vrOverlayMode;visible:!card.session.closeUnavailable;enabled:card.session.running===true&&!card.session.closing;onClicked:uiController.closeLaunch()}
  PillButton {objectName:"nowPlayingFront";visible:!!card.trouble&&card.trouble!=="hung";targetHeight:card.vrOverlayMode?56:48;text:"Bring to front";vrOverlayMode:card.vrOverlayMode;onClicked:uiController.bringGameToFront()}
  PillButton {objectName:"nowPlayingEnd";visible:!!card.trouble;targetHeight:card.vrOverlayMode?56:48;text:"End game";neon:true;accent:Theme.get("color.state.uninstall");vrOverlayMode:card.vrOverlayMode;onClicked:uiController.stopLaunch()}
  PillButton {objectName:"nowPlayingRestart";targetHeight:card.vrOverlayMode?56:48;text:"Restart";vrOverlayMode:card.vrOverlayMode;visible:!card.session.closeUnavailable;enabled:card.session.running===true&&!card.session.closing;onClicked:uiController.restartLaunch()}
 }
 // The phase and the title together, for reading out and for checks.
 Text {objectName:"nowPlayingPhase";visible:false;text:(card.phase?card.phase+": ":"")+(card.session.title||"")}
}
