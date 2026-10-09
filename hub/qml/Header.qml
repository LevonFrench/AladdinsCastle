// SPDX-License-Identifier: GPL-3.0-only
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import AladdinsCastle.Hub
Rectangle {
 id:header
 property bool vrOverlayMode:false
 signal libraryRequested()
 signal helpRequested()
 signal orderRequested()
 signal queryEdited(string text)
 property alias searchField:search
 implicitHeight:vrOverlayMode?88:78;height:implicitHeight;color:Theme.get("color.surface.header")
 function focusSearch(){search.forceActiveFocus()}
 RowLayout {anchors.fill:parent;anchors.margins:14;spacing:10
  UiText {visible:header.width>=620;text:"◉";font.pixelSize:32;color:Theme.get("color.brand.orange")}
  Column {visible:header.width>=620;Layout.fillWidth:true;spacing:3
   Row {spacing:8;GradientText {text:"AladdinsCastle";pixelSize:22} UiText {visible:header.width>=780;text:"v"+uiController.appVersion;font.pixelSize:10;color:Theme.get("color.text.faint")} }
   UiText {visible:header.width>=780;text:"3D light gun & racing games in true VR";font.pixelSize:11;color:Theme.get("color.text.faint")}
  }
  Item {visible:header.width<620;Layout.fillWidth:true}
  PillButton {text:"▦";width:vrOverlayMode?44:40;vrOverlayMode:header.vrOverlayMode;Accessible.name:"Open library";onClicked:header.libraryRequested()}
  PillButton {text:"···";width:vrOverlayMode?44:40;vrOverlayMode:header.vrOverlayMode;Accessible.name:"Help and settings";onClicked:header.helpRequested()}
  PillButton {text:(header.width>=620?"ORDER  ":"")+gameFilter.sortMode+" ▾";Layout.preferredWidth:header.width>=780?144:112;vrOverlayMode:header.vrOverlayMode;onClicked:header.orderRequested()}
  TextField {id:search;objectName:"searchField";Layout.preferredWidth:header.width>=780?165:115;Layout.preferredHeight:header.vrOverlayMode?56:40;text:gameFilter.query;placeholderText:"Search";color:Theme.get("color.text.primary");placeholderTextColor:Theme.get("color.text.faint");font.family:Theme.get("font.family");font.pixelSize:header.vrOverlayMode?16:12;selectByMouse:true;Accessible.name:"Search games";onActiveFocusChanged:if(activeFocus&&header.vrOverlayMode&&typeof overlayHost!=="undefined")overlayHost.requestKeyboard(search);onTextEdited:header.queryEdited(text);Keys.onEscapePressed:if(text.length){clear();header.queryEdited("")}else focus=false
   background:Rectangle {radius:6;color:Theme.get("color.surface.pill");border.color:search.activeFocus?Theme.get("color.glass.ring"):Theme.get("color.line.strong")}
  }
 }
 UiText {anchors.right:parent.right;anchors.bottom:parent.bottom;anchors.rightMargin:14;text:search.activeFocus&&search.text.length===0?["Try racing","Try gun true3d","Use -publisher to exclude"][hint.index]:"";font.pixelSize:10;color:Theme.get("color.text.faint")}
 Timer {id:hint;property int index:0;interval:Theme.get("motion.search_example");repeat:true;running:search.activeFocus&&search.text.length===0;onTriggered:index=(index+1)%3}
}
