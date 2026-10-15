// SPDX-License-Identifier: GPL-3.0-only
import QtQuick
import QtQuick.Controls
import AladdinsCastle.Hub
ScrollView{
 id:page
 property bool vrOverlayMode:false
 clip:true
 Column{width:page.availableWidth;spacing:14
  GradientText{text:"Hub settings";pixelSize:28}
  UiText{width:parent.width;text:"Settings stay in the portable user folder. The Hub reads local art only."}
  PillButton{objectName:"settingShowAll";vrOverlayMode:page.vrOverlayMode;property bool on:(uiSettings.values,uiSettings.get("showAllGames",false))===true;selected:on
   text:"Show games without a setup · "+(on?"On":"Off");onClicked:uiSettings.set("showAllGames",!on)}
  UiText{width:parent.width;font.pixelSize:12;text:"Off: only games that can be played on this PC are listed. On: the whole catalog comes back, with its filters, search and order."}
  Repeater{model:[{key:"reduceMotion",label:"Reduce motion"},{key:"cheapEffects",label:"Use cheaper card effects"},{key:"scanOnStartup",label:"Scan remembered folders on startup",catalog:true},{key:"bannerListDisabled",label:"Hide list banner",catalog:true},{key:"bannerLibDisabled",label:"Hide library banner",catalog:true},{key:"recentlyPlayedHidden",label:"Hide recently played",catalog:true}].filter(m=>!m.catalog||(uiSettings.values,uiSettings.get("showAllGames",false))===true);PillButton{vrOverlayMode:page.vrOverlayMode;required property var modelData;text:modelData.label+" · "+(selected?"On":"Off");selected:Object.keys(uiSettings.values).length>=0&&(uiSettings.values,uiSettings.get(modelData.key,modelData.key==="cheapEffects"));onClicked:uiSettings.set(modelData.key,!selected)}}
  UiText{visible:(uiSettings.values,uiSettings.get("showAllGames",false))===true;width:parent.width;text:"Catalog warnings ("+gameModel.warnings.length+")";font.weight:600}
  UiText{visible:(uiSettings.values,uiSettings.get("showAllGames",false))===true;width:parent.width;text:gameModel.warnings.join("\n");font.pixelSize:12}
 }
}
