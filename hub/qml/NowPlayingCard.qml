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
  UiText {Layout.fillWidth:true;text:(card.session.gunRoom?"":"Now playing: ")+(card.session.title||"");font.pixelSize:20}
  RowLayout {spacing:8
   PillButton {objectName:"nowPlayingExit";text:card.session.gunRoom?"Exit room":"Exit game";vrOverlayMode:card.vrOverlayMode;enabled:card.session.running===true&&!card.session.closing&&!card.session.closeUnavailable;onClicked:uiController.closeLaunch()}
   PillButton {objectName:"nowPlayingRestart";text:"Restart";vrOverlayMode:card.vrOverlayMode;enabled:card.session.running===true&&!card.session.closing&&!card.session.closeUnavailable;onClicked:uiController.restartLaunch()}
   PillButton {objectName:"nowPlayingRecentre";text:"Recentre";vrOverlayMode:card.vrOverlayMode;enabled:false}
  }
  UiText {Layout.fillWidth:true;wrapMode:Text.Wrap;text:card.session.closing?(card.session.gunRoom?"Waiting for the room to close cleanly…":"Waiting for the game to close cleanly…"):card.session.closeUnavailable||card.session.hint||(card.session.gunRoom?"Recentre is not available from the Hub for this room.":"Recentre is not available from the Hub for this launch. Use the game's recentre control.")}
 }
}
