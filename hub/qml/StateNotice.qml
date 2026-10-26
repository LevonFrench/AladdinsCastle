// SPDX-License-Identifier: GPL-3.0-only
import QtQuick
import QtQuick.Layouts
import AladdinsCastle.Hub
// An empty or error state: one plain sentence and one action. The notice is one of uiController's (see UiController::notice);
// the button does uiController.noticeAction(notice.action).
Rectangle {
 id:box
 objectName:"stateNotice"
 property var notice:({})
 property bool vrOverlayMode:false
 readonly property bool trouble:notice.tone==="trouble"
 readonly property string kind:notice.kind||""
 visible:!!notice.text
 implicitHeight:visible?Math.max(row.implicitHeight+(vrOverlayMode?32:24),vrOverlayMode?72:56):0
 radius:Theme.get("radius.card")
 color:trouble?Theme.get("color.notice.fill"):Theme.get("color.surface.pill")
 border.width:trouble?1:0;border.color:Theme.get("color.notice.line")
 Accessible.name:notice.text||"";Accessible.role:Accessible.AlertMessage
 RowLayout {
  id:row;anchors.left:parent.left;anchors.right:parent.right;anchors.verticalCenter:parent.verticalCenter;anchors.leftMargin:16;anchors.rightMargin:16;spacing:14
  UiText {objectName:"stateNoticeText";Layout.fillWidth:true;text:box.notice.text||"";font.pixelSize:box.vrOverlayMode?18:14
   color:box.trouble?Theme.get("color.notice.body"):Theme.get("color.text.primary")}
  PillButton {objectName:"stateNoticeAction";visible:!!box.notice.action;text:box.notice.actionLabel||"";neon:true;vrOverlayMode:box.vrOverlayMode
   accent:box.trouble?Theme.get("color.state.needs_neon"):Theme.get("color.brand.orange");onClicked:uiController.noticeAction(box.notice.action)}
 }
}
