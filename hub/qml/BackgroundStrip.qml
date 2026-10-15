// SPDX-License-Identifier: GPL-3.0-only
import QtQuick
import QtQuick.Layouts
import AladdinsCastle.Hub
// Programs of the set-up systems still running outside any game: left behind by a crash or a closed
// window, or started some other way. End asks each to close, then ends it and what it started.
Rectangle {
 id:strip
 property bool vrOverlayMode:false
 readonly property var programs:uiController.backgroundPrograms||[]
 visible:programs.length>0
 implicitHeight:visible?row.implicitHeight+(vrOverlayMode?24:18):0
 color:Theme.get("color.surface.pill")
 Rectangle {width:6;height:parent.height;color:Theme.get("color.state.needs_neon")}
 RowLayout {id:row;anchors.left:parent.left;anchors.right:parent.right;anchors.verticalCenter:parent.verticalCenter;anchors.leftMargin:strip.vrOverlayMode?28:40;anchors.rightMargin:strip.vrOverlayMode?28:40;spacing:12
  Text {objectName:"backgroundTitle";text:"Running in the background ("+strip.programs.length+")";color:Theme.get("color.state.needs_neon");font.family:Theme.get("font.family");font.weight:700;font.pixelSize:strip.vrOverlayMode?18:14}
  Flickable {Layout.fillWidth:true;Layout.preferredHeight:chips.implicitHeight;contentWidth:chips.implicitWidth;clip:true;flickableDirection:Flickable.HorizontalFlick;interactive:contentWidth>width
   Row {id:chips;spacing:8
    Repeater {model:strip.programs
     Rectangle {required property var modelData;objectName:"backgroundProgram";height:chipRow.implicitHeight+8;width:chipRow.implicitWidth+18;radius:height/2;color:Theme.get("color.surface.card")
      Row {id:chipRow;anchors.centerIn:parent;spacing:8
       Text {anchors.verticalCenter:parent.verticalCenter;text:modelData.label;color:Theme.get("color.text.primary");font.family:Theme.get("font.family");font.weight:500;font.pixelSize:strip.vrOverlayMode?16:13}
       PillButton {objectName:"backgroundEnd";text:"End";targetHeight:strip.vrOverlayMode?44:30;vrOverlayMode:strip.vrOverlayMode;onClicked:uiController.endBackgroundProgram(modelData.pid)}
      }
     }
    }
   }
  }
  PillButton {objectName:"backgroundEndAll";visible:strip.programs.length>1;text:"End all";neon:true;accent:Theme.get("color.state.uninstall");vrOverlayMode:strip.vrOverlayMode;onClicked:uiController.endAllBackgroundPrograms()}
 }
}
