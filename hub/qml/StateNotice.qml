// SPDX-License-Identifier: GPL-3.0-only
import QtQuick
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
 // Anchored, not a RowLayout: a layout hidden while the overlay mode changes would keep the button at its desktop height.
 Item {
  id:row;anchors.left:parent.left;anchors.right:parent.right;anchors.verticalCenter:parent.verticalCenter;anchors.leftMargin:16;anchors.rightMargin:16
  implicitHeight:Math.max(noticeText.implicitHeight,action.visible?action.height:0);height:implicitHeight
  UiText {id:noticeText;objectName:"stateNoticeText";anchors.left:parent.left;anchors.verticalCenter:parent.verticalCenter;width:parent.width-(action.visible?action.width+14:0)
   text:box.notice.text||"";font.pixelSize:box.vrOverlayMode?18:14;color:box.trouble?Theme.get("color.notice.body"):Theme.get("color.text.primary")}
  PillButton {id:action;objectName:"stateNoticeAction";anchors.right:parent.right;anchors.verticalCenter:parent.verticalCenter;visible:!!box.notice.action;text:box.notice.actionLabel||"";neon:true;vrOverlayMode:box.vrOverlayMode
   accent:box.trouble?Theme.get("color.state.needs_neon"):Theme.get("color.brand.orange");onClicked:uiController.noticeAction(box.notice.action)}
 }
}
