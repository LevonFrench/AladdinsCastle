// SPDX-License-Identifier: GPL-3.0-only
import QtQuick
import QtQuick.Layouts
import AladdinsCastle.Hub
// Play in headset while SteamVR was off: SteamVR is starting, then the game starts. Cancel starts nothing.
Rectangle {
 id:card
 property bool vrOverlayMode:false
 readonly property string game:uiController.steamVrStarting
 visible:game.length>0
 color:Theme.get("color.surface.window")
 MouseArea {anchors.fill:parent;acceptedButtons:Qt.AllButtons;onWheel:event=>{event.accepted=true}}
 Keys.onEscapePressed:uiController.cancelSteamVrStart()
 HeroBanner {anchors.fill:parent;vrOverlayMode:card.vrOverlayMode;system:"catalog"
  eyebrow:"STARTING STEAMVR…"
  title:card.game
  meta:"SteamVR is starting. "+card.game+" starts in the headset as soon as SteamVR is ready. Put the headset on when its light comes on."
  PillButton {objectName:"steamVrStartingCancel";targetHeight:card.vrOverlayMode?56:48;text:"Cancel";vrOverlayMode:card.vrOverlayMode;onClicked:uiController.cancelSteamVrStart()}
 }
 // Read out and checked: what is happening, in one line.
 Text {objectName:"steamVrStartingState";visible:false;text:card.visible?"Starting SteamVR… for "+card.game:""}
}
