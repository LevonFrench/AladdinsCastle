// SPDX-License-Identifier: GPL-3.0-only
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import AladdinsCastle.Hub
// One setting: its name and what it does on the left, an on/off switch on the right. The whole row is the button.
Button {
 id:row
 property string label:""
 property string detail:""
 property bool on:false
 property bool vrOverlayMode:false
 // Kept for the older checks that read a toggle's state from "selected".
 readonly property bool selected:on
 hoverEnabled:true;padding:vrOverlayMode?20:16
 Accessible.name:label;Accessible.checked:on
 background:Rectangle {radius:Theme.get("radius.card");color:row.down?Theme.get("color.surface.menu_hover"):Theme.get("color.surface.card");border.width:row.activeFocus||row.hovered?2:0;border.color:Theme.get("color.glass.ring")}
 contentItem:RowLayout {spacing:20
  ColumnLayout {Layout.fillWidth:true;spacing:4
   Text {Layout.fillWidth:true;text:row.label;color:Theme.get("color.text.primary");font.family:Theme.get("font.family");font.weight:700;font.pixelSize:row.vrOverlayMode?20:17;wrapMode:Text.Wrap}
   Text {Layout.fillWidth:true;visible:text.length>0;text:row.detail;color:Theme.get("color.text.muted");font.family:Theme.get("font.family");font.weight:500;font.pixelSize:row.vrOverlayMode?16:14;wrapMode:Text.Wrap}
  }
  Rectangle {objectName:"settingSwitch";Layout.preferredWidth:row.vrOverlayMode?64:52;Layout.preferredHeight:row.vrOverlayMode?36:30;radius:height/2
   color:row.on?Theme.get("color.state.ready_neon"):Theme.get("color.line.button")
   Rectangle {width:parent.height-8;height:width;radius:width/2;y:4;x:row.on?parent.width-width-4:4;color:"white"
    Behavior on x {enabled:!(uiSettings.values,uiSettings.get("reduceMotion",false));NumberAnimation{duration:120}}}
  }
 }
}
