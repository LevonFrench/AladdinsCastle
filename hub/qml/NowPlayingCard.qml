// SPDX-License-Identifier: GPL-3.0-only
import QtQuick
import QtQuick.Layouts
import AladdinsCastle.Hub
Rectangle {
 id:card
 property bool vrOverlayMode:false
 property var session:uiController.nowPlaying
 visible:session.gameId!==undefined
 implicitHeight:visible?150:0
 color:Theme.get("color.surface.header")
 MouseArea {anchors.fill:parent;acceptedButtons:Qt.AllButtons;onWheel:event=>{event.accepted=true}}
 ColumnLayout {anchors.fill:parent;anchors.margins:16;spacing:8
  // The phase is in the first words, so starting, playing and stopping can be told apart at a glance.
  UiText {objectName:"nowPlayingPhase";Layout.fillWidth:true;font.pixelSize:20
   text:(card.session.stopping?"Stopping: ":card.session.closing?"Closing: ":card.session.running?(card.session.gunRoom?"":"Now playing: "):"Starting: ")+(card.session.title||"")}
  RowLayout {spacing:8
   PillButton {objectName:"nowPlayingExit";text:card.session.gunRoom?"Exit room":"Exit game";vrOverlayMode:card.vrOverlayMode;enabled:card.session.running===true&&!card.session.closing&&!card.session.closeUnavailable;onClicked:uiController.closeLaunch()}
   PillButton {objectName:"nowPlayingRestart";text:"Restart";vrOverlayMode:card.vrOverlayMode;enabled:card.session.running===true&&!card.session.closing&&!card.session.closeUnavailable;onClicked:uiController.restartLaunch()}
  }
  UiText {Layout.fillWidth:true;wrapMode:Text.Wrap;text:card.session.stopping?"The game is asked to close, and ended if it has not closed within a few seconds.":card.session.closing?(card.session.gunRoom?"Waiting for the room to close cleanly…":"Waiting for the game to close cleanly…"):!card.session.running?"Starting the game. It opens in a moment.":card.session.closeUnavailable||card.session.hint||(card.session.gunRoom?"Recentre is not available from the Hub for this room.":"Recentre is not available from the Hub for this launch. Use the game's recentre control.")}
 }
}
