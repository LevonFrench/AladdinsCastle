// SPDX-License-Identifier: GPL-3.0-only
import QtQuick
import AladdinsCastle.Hub
Column {
 id:root
 property bool vrOverlayMode:false
 signal detailRequested(string id)
 visible:uiController.recent.length>0&&!(uiSettings.values,uiSettings.get("recentlyPlayedHidden",false))
 height:visible?implicitHeight:0;spacing:8
 Row{spacing:8;UiText{text:"▶ Recently played";color:Theme.get("color.brand.recent_teal")} PillButton{text:"Hide row";onClicked:uiSettings.set("recentlyPlayedHidden",true)}}
 Flickable{width:parent.width;height:vrOverlayMode?248:160;contentWidth:recentRow.width;clip:true
 Row{id:recentRow;spacing:10;Repeater{model:uiController.recent;Column{required property var modelData;GameCard{game:modelData;vrOverlayMode:root.vrOverlayMode;onDetailRequested:id=>root.detailRequested(id);onActionRequested:id=>uiController.primary(id)} PillButton{text:"Remove from recent";onClicked:uiController.removeRecent(modelData.gameId)}}}}
 }
}
