// SPDX-License-Identifier: GPL-3.0-only
import QtQuick
import AladdinsCastle.Hub
// One row of tiles under a heading. The page keeps which row and which tile have the focus;
// the row only shows it, reports hovers and clicks, and keeps the focused tile in view.
Column {
 id:shelf
 property string heading:""
 property var items:[]
 property int focusIndex:-1
 property bool vrOverlayMode:false
 property real tileWidth:vrOverlayMode?300:280
 property string tileName:"shelfTile"
 // Optional controls under each tile (the item is given as modelData), and their height.
 property Component footer:null
 property real footerHeight:0
 // Each item as the tile shows it: {title, system, art, badge, badgeColour}.
 property var tileOf:function(item){return item}
 signal hovered(int index)
 signal activated(int index)
 spacing:vrOverlayMode?10:12
 onFocusIndexChanged:if(focusIndex>=0)list.positionViewAtIndex(focusIndex,ListView.Contain)
 Text {text:shelf.heading;color:Theme.get("color.text.primary");font.family:Theme.get("font.family");font.weight:700;font.pixelSize:shelf.vrOverlayMode?22:19;leftPadding:shelf.vrOverlayMode?28:40}
 ListView {id:list;objectName:"shelfList";width:shelf.width;height:shelf.tileWidth*9/16+16+(shelf.footer?shelf.footerHeight+10:0);orientation:ListView.Horizontal;spacing:16;model:shelf.items;clip:false
  leftMargin:shelf.vrOverlayMode?28:40;rightMargin:40;boundsBehavior:Flickable.StopAtBounds;interactive:contentWidth>width;cacheBuffer:shelf.tileWidth*2
  delegate:Item {id:cell;required property var modelData;required property int index;width:shelf.tileWidth;height:list.height
   readonly property var view:shelf.tileOf(modelData)
   Loader {active:!!shelf.footer;sourceComponent:shelf.footer;width:parent.width;height:shelf.footerHeight;y:parent.width*9/16+18
    property var modelData:cell.modelData}
   GameTile {objectName:shelf.tileName;y:8;width:parent.width;height:parent.width*9/16;vrOverlayMode:shelf.vrOverlayMode
    title:parent.view.title||"";system:parent.view.system||"model3";art:parent.view.art||"";badge:parent.view.badge||"";badgeColour:parent.view.badgeColour||Theme.get("color.state.needs_neon")
    focused:parent.index===shelf.focusIndex
    onHoveredChanged:if(hovered)shelf.hovered(parent.index)
    onClicked:shelf.activated(parent.index)}}
 }
}
