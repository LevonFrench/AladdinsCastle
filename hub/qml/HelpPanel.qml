// SPDX-License-Identifier: GPL-3.0-only
import QtQuick
import QtQuick.Controls
import AladdinsCastle.Hub
Popup{
 id:panel
 signal settingsRequested()
 width:300;padding:12;modal:true;closePolicy:Popup.CloseOnEscape|Popup.CloseOnPressOutside
 background:Rectangle{color:Theme.get("color.surface.panel");radius:8;border.color:Theme.get("color.line.button")}
 Column{width:parent.width;spacing:8;UiText{text:"HELP & FEEDBACK";font.pixelSize:10;color:Theme.get("color.text.menu_caption")}
  PillButton{text:"Settings";width:parent.width;onClicked:{panel.close();panel.settingsRequested()}}
  PillButton{text:"Suggest a game";width:parent.width;onClicked:uiController.openLink("https://github.com/LevonFrench/AladdinsCastle/issues/new")}
  PillButton{text:"Logs and report a problem";width:parent.width;onClicked:uiController.openLocation("log")}
  UiText{width:parent.width;text:"Supply your own game files. M1 supports flat emulator routes. VR setups and sideloading follow in later milestones.";font.pixelSize:12}
 }
 WheelHandler{onWheel:panel.close()}
}
