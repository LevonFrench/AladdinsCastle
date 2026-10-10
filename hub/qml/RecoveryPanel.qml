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
  Row{width:parent.width;spacing:8;TextField{id:path;objectName:"handoverPath";width:parent.width-130;height:panel.vrOverlayMode?56:44;placeholderText:"Paste a full path";text:panel.handoverPath;color:Theme.get("color.text.primary");onTextEdited:panel.handoverPath=text;onAccepted:panel.recovery.retryKind==="tool"?uiController.retryFailedTool(false,text):uiController.retryInstall(false,text);background:Rectangle{color:Theme.get("color.surface.pill");border.color:Theme.get("color.line.button");radius:4}} PillButton{vrOverlayMode:panel.vrOverlayMode;text:"Choose file";onClicked:picker.open()}}
  Flow{width:parent.width;spacing:8
   PillButton{vrOverlayMode:panel.vrOverlayMode;objectName:"retrySelectedGame";text:"Retry failed game · "+(panel.recovery.retryGameId||"?")+" / "+(panel.recovery.retryVariantId||"?");visible:uiController.retryGameAvailable;onClicked:uiController.retryInstall(true,panel.handoverPath)}
   PillButton{vrOverlayMode:panel.vrOverlayMode;objectName:"retryFailedTool";text:"Retry failed "+(panel.recovery.retryLabel||"tool")+" · "+(panel.recovery.retryGameId||"")+" / "+(panel.recovery.retryVariantId||"");visible:uiController.retryToolAvailable;onClicked:uiController.retryFailedTool(true,panel.handoverPath)}
   PillButton{vrOverlayMode:panel.vrOverlayMode;text:"Open log";onClicked:uiController.openLocation("log")} PillButton{vrOverlayMode:panel.vrOverlayMode;text:"Open install folder";onClicked:uiController.openLocation("install")} PillButton{vrOverlayMode:panel.vrOverlayMode;text:"Open download cache";onClicked:uiController.openLocation("downloads")}
   PillButton{vrOverlayMode:panel.vrOverlayMode;text:"Clear handover";visible:panel.handoverPath.length>0;onClicked:panel.handoverPath=""}

   PillButton{vrOverlayMode:panel.vrOverlayMode;text:"Open download page";visible:!!panel.recovery.downloadUrl;onClicked:uiController.openLink(panel.recovery.downloadUrl)}
  }
 }
 DropArea{anchors.fill:parent;onDropped:drop=>{if(drop.urls.length)panel.handoverPath=uiController.localPath(drop.urls[0])}}
 FileDialog{id:picker;title:"Choose a file for the installer to validate";onAccepted:panel.handoverPath=uiController.localPath(selectedFile)}
}
