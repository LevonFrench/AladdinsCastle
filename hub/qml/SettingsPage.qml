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
  Repeater{model:[{key:"reduceMotion",label:"Reduce motion"},{key:"cheapEffects",label:"Use cheaper card effects"},{key:"scanOnStartup",label:"Scan remembered folders on startup"},{key:"bannerListDisabled",label:"Hide list banner"},{key:"bannerLibDisabled",label:"Hide library banner"},{key:"recentlyPlayedHidden",label:"Hide recently played"}];PillButton{vrOverlayMode:page.vrOverlayMode;required property var modelData;text:modelData.label+" · "+(selected?"On":"Off");selected:Object.keys(uiSettings.values).length>=0&&(uiSettings.values,uiSettings.get(modelData.key,modelData.key==="cheapEffects"));onClicked:uiSettings.set(modelData.key,!selected)}}
  UiText{width:parent.width;text:"Catalog warnings ("+gameModel.warnings.length+")";font.weight:600}
  UiText{width:parent.width;text:gameModel.warnings.join("\n");font.pixelSize:12}
 }
}
