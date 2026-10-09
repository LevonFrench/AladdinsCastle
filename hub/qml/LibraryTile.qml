// SPDX-License-Identifier: GPL-3.0-only
import QtQuick
import AladdinsCastle.Hub
FocusScope {
 id:tile
 objectName:"libraryTile"
 property var game:({})
 property url source:""
 property string sizeValue:"S"
 property bool explore:false
 property bool vrOverlayMode:false
 signal detailRequested(string id)
 implicitWidth:Math.max(vrOverlayMode?44:0,Theme.get((explore?"size.explore_tile.":"size.library_tile.")+sizeValue+".w"))
 implicitHeight:Math.max(vrOverlayMode?56:0,Theme.get((explore?"size.explore_tile.":"size.library_tile.")+sizeValue+".h"))
 activeFocusOnTab:true
 Accessible.name:game.title||"Game";Accessible.role:Accessible.Button
 GameArt{objectName:"tileArt";anchors.fill:parent;game:tile.game;kind:"portrait";source:tile.source}
 Rectangle{anchors.left:parent.left;anchors.right:parent.right;anchors.bottom:parent.bottom;height:4;color:tile.game.stateColour||Theme.get("color.state.planned_neon")}
 Rectangle{anchors.fill:parent;color:Theme.alpha(Theme.get("color.surface.window"),0);border.width:tile.activeFocus?2:0;border.color:Theme.get("color.glass.ring")}
 UiText{anchors.bottom:parent.bottom;anchors.left:parent.left;anchors.right:parent.right;anchors.margins:12;text:tile.game.title||"";font.weight:600;color:Theme.get("color.text.primary")}
 TapHandler{onTapped:tile.detailRequested(tile.game.gameId)}
 Keys.onReturnPressed:detailRequested(game.gameId)
}
