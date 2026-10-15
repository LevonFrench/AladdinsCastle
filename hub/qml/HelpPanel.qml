// SPDX-License-Identifier: GPL-3.0-only
import QtQuick
import QtQuick.Controls
import AladdinsCastle.Hub
Popup{
 id:panel
 objectName:"helpPanel"
 property bool vrOverlayMode:false
 property bool showAll:false
 signal settingsRequested()
 signal gunRoomRequested()
 signal setupRequested()
 width:300;padding:12;modal:true;focus:true;closePolicy:Popup.CloseOnEscape|Popup.CloseOnPressOutside
 background:Rectangle{color:Theme.get("color.surface.panel");radius:Theme.get("radius.hero");border.color:Theme.get("color.line.strong")}
 Column{width:parent.width;spacing:8;UiText{text:"HELP & SETTINGS";font.pixelSize:panel.vrOverlayMode?14:12;font.weight:700;font.letterSpacing:1.5;color:Theme.get("color.text.muted")}
  PillButton{objectName:"helpSettings";vrOverlayMode:panel.vrOverlayMode;text:"Settings";width:parent.width;onClicked:{panel.close();panel.settingsRequested()}}
  PillButton{objectName:"helpSetup";vrOverlayMode:panel.vrOverlayMode;text:"Set-up";width:parent.width;onClicked:{panel.close();panel.setupRequested()}}
  PillButton{objectName:"helpGunRoom";text:"Gun room";visible:!panel.vrOverlayMode&&panel.showAll;width:parent.width;onClicked:{panel.close();panel.gunRoomRequested()}}
  PillButton{objectName:"helpSuggest";visible:!panel.vrOverlayMode&&panel.showAll;vrOverlayMode:panel.vrOverlayMode;text:"Suggest a game";width:parent.width;onClicked:uiController.openLink("https://github.com/LevonFrench/AladdinsCastle/issues/new")}
  PillButton{objectName:"helpLogs";visible:!panel.vrOverlayMode;vrOverlayMode:panel.vrOverlayMode;text:"Logs and report a problem";width:parent.width;onClicked:{panel.close();uiController.openLocation("log")}}
  PillButton{objectName:"helpStopEverything";vrOverlayMode:panel.vrOverlayMode;text:"Stop everything";width:parent.width;onClicked:{panel.close();uiController.stopEverything()}}
  PillButton{objectName:"helpClose";vrOverlayMode:panel.vrOverlayMode;text:"Close";width:parent.width;onClicked:panel.close()}
 }
}
