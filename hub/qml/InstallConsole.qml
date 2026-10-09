// SPDX-License-Identifier: GPL-3.0-only
import QtQuick
import QtQuick.Controls
import AladdinsCastle.Hub
Rectangle {
 id:installConsole
 property bool vrOverlayMode:false
 height:uiController.consoleEvents.length?Math.min(360,80+uiController.consoleEvents.length*22):0
 visible:height>0;color:Theme.get("color.console.bg");border.color:Theme.get("color.line.rule");radius:6
 UiText {anchors.top:parent.top;anchors.left:parent.left;anchors.margins:10;text:"AladdinsCastle installer · Last install";color:Theme.get("color.console.step")}
 PillButton {anchors.top:parent.top;anchors.right:parent.right;text:"Copy log";onClicked:uiController.copyLog()}
 ListView {id:events;objectName:"installEvents";anchors.fill:parent;anchors.topMargin:48;anchors.bottomMargin:uiController.installing?48:8;anchors.leftMargin:10;anchors.rightMargin:10;model:uiController.consoleEvents;clip:true;ScrollBar.vertical:ScrollBar{}
  property bool followTail:true
  onMovementStarted:followTail=contentY>=contentHeight-height-32
  onCountChanged:if(followTail)Qt.callLater(positionViewAtEnd)
  delegate:UiText {required property var modelData;width:events.width;text:modelData.line;font.family:Theme.get("font.mono")[0];font.pixelSize:13;color:Theme.get("color.console."+(modelData.kind==="done"?"ok":modelData.kind==="prompt"?"warn":modelData.kind));lineHeight:1.4}
 }
 Row{anchors.bottom:parent.bottom;anchors.left:parent.left;anchors.margins:6;spacing:8;visible:uiController.installing
  PillButton{text:"Cancel after this step";onClicked:uiController.cancelInstall()}
  PillButton{text:"Continue";visible:uiController.consoleEvents.length>0&&uiController.consoleEvents[uiController.consoleEvents.length-1].kind==="prompt";onClicked:uiController.answerPrompt(true)}
 }
}
