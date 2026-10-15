// SPDX-License-Identifier: GPL-3.0-only
import QtQuick
import QtQuick.Layouts
import AladdinsCastle.Hub
// The focused game across the top: its picture (or a plain shape in its system's colour) fading into the page,
// with a small eyebrow, a large title, one meta line and the page's own buttons.
Item {
 id:hero
 property string eyebrow:""
 property string title:""
 property string meta:""
 property color metaColour:Theme.get("color.text.muted")
 property string system:"model3"
 property url art:""
 property bool vrOverlayMode:false
 default property alias actions:actionRow.data
 property alias extra:extraRow.data
 readonly property color tone:Theme.get("color.system."+hero.system)||Theme.get("color.brand.blue")
 clip:true
 Rectangle {anchors.fill:parent;visible:hero.art=="";gradient:Gradient{orientation:Gradient.Horizontal
  GradientStop{position:0;color:Qt.darker(hero.tone,1.3)} GradientStop{position:1;color:Qt.darker(hero.tone,3.4)}}}
 Repeater {model:hero.art==""?4:0;Rectangle {width:hero.height*(0.9+index*0.45);height:width;radius:width/2;color:"transparent";border.width:Math.max(1,hero.height*0.02);border.color:"white";opacity:0.07;x:hero.width*0.72-width/2;y:hero.height*0.5-height/2}}
 Image {anchors.right:parent.right;width:parent.width*0.62;height:parent.height;visible:hero.art!="";source:hero.art;fillMode:Image.PreserveAspectCrop;asynchronous:true}
 Rectangle {anchors.fill:parent;gradient:Gradient{orientation:Gradient.Horizontal
  GradientStop{position:0;color:Theme.get("color.surface.window")} GradientStop{position:0.38;color:Theme.alpha(Theme.get("color.surface.window"),0.86)} GradientStop{position:0.78;color:"transparent"}}}
 Rectangle {anchors.left:parent.left;anchors.right:parent.right;anchors.bottom:parent.bottom;height:parent.height*0.35
  gradient:Gradient{GradientStop{position:0;color:"transparent"} GradientStop{position:1;color:Theme.get("color.surface.window")}}}
 Row {id:extraRow;anchors.right:parent.right;anchors.top:parent.top;anchors.margins:hero.vrOverlayMode?16:20;spacing:8}
 ColumnLayout {anchors.left:parent.left;anchors.leftMargin:hero.vrOverlayMode?28:40;anchors.verticalCenter:parent.verticalCenter;width:parent.width*(hero.vrOverlayMode?0.6:0.52);spacing:hero.vrOverlayMode?8:10
  Text {objectName:"heroEyebrow";text:hero.eyebrow;visible:text.length>0;color:Theme.get("color.brand.blue");font.family:Theme.get("font.family");font.weight:700;font.pixelSize:hero.vrOverlayMode?16:14;font.letterSpacing:2}
  Text {objectName:"heroTitle";Layout.fillWidth:true;text:hero.title;wrapMode:Text.Wrap;maximumLineCount:2;elide:Text.ElideRight;color:Theme.get("color.text.primary");font.family:Theme.get("font.family");font.weight:900;font.pixelSize:hero.vrOverlayMode?44:52;lineHeight:0.95}
  Text {objectName:"heroMeta";Layout.fillWidth:true;text:hero.meta;visible:text.length>0;wrapMode:Text.Wrap;maximumLineCount:3;elide:Text.ElideRight;color:hero.metaColour;font.family:Theme.get("font.family");font.weight:500;font.pixelSize:hero.vrOverlayMode?18:16}
  Row {id:actionRow;spacing:10;Layout.topMargin:6}
 }
}
