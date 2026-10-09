// SPDX-License-Identifier: GPL-3.0-only
import QtQuick
import QtQuick.Controls
import QtQuick.Dialogs
import AladdinsCastle.Hub
Rectangle {
 id:panel
 property bool vrOverlayMode:false
 property var recovery:uiController.recovery
 property string handoverPath:""
 visible:Object.keys(recovery).length>0
 height:visible?body.implicitHeight+24:0;color:Theme.get("color.notice.fill");border.color:Theme.get("color.notice.line");radius:6
 Column{id:body;anchors.left:parent.left;anchors.right:parent.right;anchors.top:parent.top;anchors.margins:12;spacing:8
  UiText{width:parent.width;text:"INSTALL STOPPED AT STEP "+(panel.recovery.step||"?")+" OF "+(panel.recovery.total||"?")+" — nothing was marked installed";font.weight:700;color:Theme.get("color.notice.heading")}
  UiText{width:parent.width;text:panel.recovery.text||"The installer needs your input.";color:Theme.get("color.notice.body")}
  UiText{width:parent.width;text:"Drag a downloaded file, folder or archive here, or choose it. The installer validates it before replacing anything."}
  Row{width:parent.width;spacing:8;TextField{id:path;objectName:"handoverPath";width:parent.width-130;height:44;placeholderText:"Paste a full path";text:panel.handoverPath;color:Theme.get("color.text.primary");onTextEdited:panel.handoverPath=text;onAccepted:uiController.retryInstall(false,text);background:Rectangle{color:Theme.get("color.surface.pill");border.color:Theme.get("color.line.button");radius:4}} PillButton{text:"Choose file";onClicked:picker.open()}}
  Flow{width:parent.width;spacing:8
   PillButton{text:"Recover and retry";onClicked:uiController.retryInstall(true,panel.handoverPath)}
   PillButton{text:"Open log";onClicked:uiController.openLocation("log")} PillButton{text:"Open install folder";onClicked:uiController.openLocation("install")} PillButton{text:"Open download cache";onClicked:uiController.openLocation("downloads")}
   PillButton{text:"Clear handover";visible:panel.handoverPath.length>0;onClicked:panel.handoverPath=""}

   PillButton{text:"Open download page";visible:!!panel.recovery.downloadUrl;onClicked:uiController.openLink(panel.recovery.downloadUrl)}
  }
 }
 DropArea{anchors.fill:parent;onDropped:drop=>{if(drop.urls.length)panel.handoverPath=uiController.localPath(drop.urls[0])}}
 FileDialog{id:picker;title:"Choose a file for the installer to validate";onAccepted:panel.handoverPath=uiController.localPath(selectedFile)}
}
