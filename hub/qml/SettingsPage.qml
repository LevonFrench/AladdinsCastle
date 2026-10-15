// SPDX-License-Identifier: GPL-3.0-only
import QtQuick
import QtQuick.Controls
import AladdinsCastle.Hub
ScrollView{
 id:page
 property bool vrOverlayMode:false
 readonly property real side:vrOverlayMode?28:40
 readonly property bool catalogOn:(uiSettings.values,uiSettings.get("showAllGames",false))===true
 clip:true
 contentWidth:availableWidth
 Column{x:page.side;width:Math.min(page.availableWidth-2*page.side,880);spacing:12;topPadding:page.vrOverlayMode?20:28;bottomPadding:28
  Text{text:"Settings";color:Theme.get("color.text.primary");font.family:Theme.get("font.family");font.weight:900;font.pixelSize:page.vrOverlayMode?40:36}
  Text{width:parent.width;wrapMode:Text.Wrap;bottomPadding:8;text:"Settings stay in the Hub's own folder on this PC. The Hub reads local art only.";color:Theme.get("color.text.muted");font.family:Theme.get("font.family");font.weight:500;font.pixelSize:page.vrOverlayMode?18:16}
  SettingRow{objectName:"settingShowAll";width:parent.width;vrOverlayMode:page.vrOverlayMode;on:page.catalogOn;label:"Show games without a setup"
   detail:"Off: only games that can be played on this PC are listed. On: the whole catalog comes back, with its filters, search and order.";onClicked:uiSettings.set("showAllGames",!on)}
  Repeater{model:[{key:"reduceMotion",label:"Reduce motion",detail:"Rows and pictures change without moving."},{key:"cheapEffects",label:"Use cheaper card effects",detail:"Plainer cards, lighter on the graphics card."},{key:"scanOnStartup",label:"Scan remembered folders on startup",catalog:true},{key:"bannerListDisabled",label:"Hide list banner",catalog:true},{key:"bannerLibDisabled",label:"Hide library banner",catalog:true},{key:"recentlyPlayedHidden",label:"Hide recently played",catalog:true}].filter(m=>!m.catalog||page.catalogOn)
   SettingRow{required property var modelData;width:parent.width;vrOverlayMode:page.vrOverlayMode;label:modelData.label;detail:modelData.detail||""
    on:Object.keys(uiSettings.values).length>=0&&(uiSettings.values,uiSettings.get(modelData.key,modelData.key==="cheapEffects"))===true;onClicked:uiSettings.set(modelData.key,!on)}}
  Text{visible:page.catalogOn;width:parent.width;topPadding:12;text:"Catalog warnings ("+gameModel.warnings.length+")";color:Theme.get("color.text.primary");font.family:Theme.get("font.family");font.weight:700;font.pixelSize:page.vrOverlayMode?20:17}
  UiText{visible:page.catalogOn;width:parent.width;text:gameModel.warnings.join("\n");font.pixelSize:12}
 }
}
