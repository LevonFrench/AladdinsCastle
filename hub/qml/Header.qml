// SPDX-License-Identifier: GPL-3.0-only
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import AladdinsCastle.Hub
Rectangle {
 id:header
 property bool vrOverlayMode:false
 property bool showAll:false
 // The page on show, so its tab is the filled one.
 property string view:""
 signal libraryRequested()
 signal pinballRequested()
 signal model3Requested()
 signal helpRequested()
 signal orderRequested()
 signal queryEdited(string text)
 property alias searchField:search
 implicitHeight:vrOverlayMode?84:72;height:implicitHeight;color:Theme.get("color.surface.header")
 function focusSearch(){search.forceActiveFocus()}
 RowLayout {anchors.fill:parent;anchors.leftMargin:header.vrOverlayMode?28:40;anchors.rightMargin:header.vrOverlayMode?28:40;spacing:8
  // Direction C: the wordmark, then pill tabs, the page's own tab filled white.
  Text {objectName:"brand";text:uiController.appName;color:Theme.get("color.text.primary");font.family:Theme.get("font.family");font.weight:900;font.pixelSize:header.vrOverlayMode?26:22;Layout.rightMargin:header.vrOverlayMode?18:28}
  PillButton {objectName:"model3Tab";text:"VR games";vrOverlayMode:header.vrOverlayMode;neon:header.view==="Model3";accent:Theme.get("color.text.primary");onClicked:header.model3Requested()}
  PillButton {objectName:"pinballTab";text:"Pinball";vrOverlayMode:header.vrOverlayMode;neon:header.view==="Pinball";accent:Theme.get("color.text.primary");onClicked:header.pinballRequested()}
  Item {Layout.fillWidth:true}
  Rectangle {objectName:"steamVrChip";Layout.preferredHeight:header.vrOverlayMode?44:34;Layout.preferredWidth:chip.implicitWidth+38;radius:height/2;color:Theme.alpha(Theme.get("color.text.primary"),0.08)
   Rectangle {x:14;anchors.verticalCenter:parent.verticalCenter;width:9;height:9;radius:5;color:uiController.steamVrRunning?Theme.get("color.state.ready_neon"):Theme.get("color.text.dim")}
   Text {id:chip;x:30;anchors.verticalCenter:parent.verticalCenter;text:uiController.steamVrRunning?"SteamVR":"SteamVR off";color:Theme.get("color.text.muted");font.family:Theme.get("font.family");font.weight:500;font.pixelSize:header.vrOverlayMode?16:13}}
  PillButton {objectName:"libraryButton";visible:header.showAll;text:"▦";width:vrOverlayMode?44:40;vrOverlayMode:header.vrOverlayMode;Accessible.name:"Open library";onClicked:header.libraryRequested()}
  PillButton {objectName:"helpButton";text:"···";width:vrOverlayMode?44:40;vrOverlayMode:header.vrOverlayMode;Accessible.name:"Help and settings";onClicked:header.helpRequested()}
  PillButton {objectName:"orderButton";visible:header.showAll;text:(header.width>=620?"ORDER  ":"")+gameFilter.sortMode+" ▾";Layout.preferredWidth:header.width>=780?144:112;vrOverlayMode:header.vrOverlayMode;onClicked:header.orderRequested()}
  TextField {id:search;objectName:"searchField";visible:header.showAll;Layout.preferredWidth:header.width>=780?165:115;Layout.preferredHeight:header.vrOverlayMode?56:40;text:gameFilter.query;placeholderText:"Search";color:Theme.get("color.text.primary");placeholderTextColor:Theme.get("color.text.faint");font.family:Theme.get("font.family");font.pixelSize:header.vrOverlayMode?16:12;selectByMouse:true;Accessible.name:"Search games";onActiveFocusChanged:if(activeFocus&&header.vrOverlayMode&&typeof overlayHost!=="undefined")overlayHost.requestKeyboard(search);onTextEdited:header.queryEdited(text);Keys.onEscapePressed:if(text.length){clear();header.queryEdited("")}else focus=false
   background:Rectangle {radius:6;color:Theme.get("color.surface.pill");border.color:search.activeFocus?Theme.get("color.glass.ring"):Theme.get("color.line.strong")}
  }
 }
 UiText {anchors.right:parent.right;anchors.bottom:parent.bottom;anchors.rightMargin:14;text:search.activeFocus&&search.text.length===0?["Try racing","Try gun true3d","Use -publisher to exclude"][hint.index]:"";font.pixelSize:10;color:Theme.get("color.text.faint")}
 Timer {id:hint;property int index:0;interval:Theme.get("motion.search_example");repeat:true;running:search.activeFocus&&search.text.length===0;onTriggered:index=(index+1)%3}
}
