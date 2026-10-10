// SPDX-License-Identifier: GPL-3.0-only
import QtQuick
import QtQuick.Controls
import AladdinsCastle.Hub
Item {
 id:scan
 property bool vrOverlayMode:false
 signal requested()
 implicitWidth:button.implicitWidth
 implicitHeight:button.implicitHeight
 PillButton{id:button;anchors.fill:parent;text:uiController.scanning?"Scanning… / Cancel":"Scan my files";neon:true;accent:Theme.get(uiController.scanning?"color.scan.scanning":"color.scan.found");vrOverlayMode:scan.vrOverlayMode;onClicked:scan.requested()}
 Rectangle{id:light;width:28;height:2;radius:1;color:Theme.get("color.scan.spinner");visible:uiController.scanning;SequentialAnimation on x{running:uiController.scanning;loops:Animation.Infinite;NumberAnimation{from:0;to:scan.width-light.width;duration:Theme.get("motion.scan_spinner_loop")/2}NumberAnimation{from:scan.width-light.width;to:0;duration:Theme.get("motion.scan_spinner_loop")/2}}}
}
