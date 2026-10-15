// SPDX-License-Identifier: GPL-3.0-only
import QtQuick
import QtQuick.Controls
import AladdinsCastle.Hub
// One game or table in a row: its picture, or a plain shape in its system's colour, with the title on a soft shade.
// Focus (controller or laser hover) draws a white ring and lifts the tile a little.
Button {
 id:tile
 property string title:""
 property string system:"model3"
 property url art:""
 property string badge:""
 property color badgeColour:Theme.get("color.state.needs_neon")
 property bool focused:false
 property bool vrOverlayMode:false
 readonly property color tone:Theme.get("color.system."+tile.system)||Theme.get("color.brand.blue")
 hoverEnabled:true;padding:0
 Accessible.name:title
 scale:focused?1.06:1;z:focused?2:1
 Behavior on scale {enabled:!(uiSettings.values,uiSettings.get("reduceMotion",false));NumberAnimation{duration:120}}
 background:Item {
  Rectangle {anchors.fill:parent;anchors.margins:-5;radius:Theme.get("radius.card")+4;color:"transparent";border.width:3;border.color:Theme.get("color.glass.ring");visible:tile.focused}
  Rectangle {id:face;anchors.fill:parent;radius:Theme.get("radius.card");clip:true;color:Theme.get("color.surface.card")
   // The plain shape: the system's colour fading out, with soft rings.
   Rectangle {anchors.fill:parent;visible:tile.art=="";gradient:Gradient{orientation:Gradient.Horizontal
    GradientStop{position:0;color:Qt.darker(tile.tone,1.25)} GradientStop{position:1;color:Qt.darker(tile.tone,3.2)}}}
   Repeater {model:tile.art==""?3:0;Rectangle {width:face.height*(0.9+index*0.45);height:width;radius:width/2;color:"transparent";border.width:2;border.color:"white";opacity:0.08;x:face.width*0.74-width/2;y:face.height*0.5-height/2}}
   Image {anchors.fill:parent;visible:tile.art!="";source:tile.art;fillMode:Image.PreserveAspectCrop;asynchronous:true;sourceSize.width:480}
   Rectangle {anchors.left:parent.left;anchors.right:parent.right;anchors.bottom:parent.bottom;height:parent.height*0.6
    gradient:Gradient{GradientStop{position:0;color:"transparent"} GradientStop{position:1;color:Theme.alpha("black",tile.art==""?0.25:0.85)}}}
  }
 }
 contentItem:Item {
  Rectangle {visible:tile.badge.length>0;anchors.left:parent.left;anchors.top:parent.top;anchors.margins:10;radius:height/2;height:tile.vrOverlayMode?28:24;width:badgeText.implicitWidth+20;color:Theme.alpha("black",0.6)
   Text {id:badgeText;anchors.centerIn:parent;text:tile.badge;color:tile.badgeColour;font.family:Theme.get("font.family");font.weight:700;font.pixelSize:tile.vrOverlayMode?15:12}}
  Text {anchors.left:parent.left;anchors.right:parent.right;anchors.bottom:parent.bottom;anchors.margins:12;text:tile.title;wrapMode:Text.Wrap;maximumLineCount:2;elide:Text.ElideRight
   color:"white";font.family:Theme.get("font.family");font.weight:tile.art==""?900:700;font.pixelSize:tile.vrOverlayMode?21:17;lineHeight:0.95}
 }
}
