// SPDX-License-Identifier: GPL-3.0-only
import QtQuick
import QtQuick.Controls
import AladdinsCastle.Hub
Item {
 id:root
 objectName:"gameGrid"
 property string view:"List"
 property string sizeValue:"S"
 property bool vrOverlayMode:false
 property bool reduceMotion:false
 property bool effectsEnabled:true
 signal detailRequested(string id)
 signal actionRequested(string id)
 signal exploreRequested()
 signal previewRequested(var game,var item)
 signal previewClosed()
 property var rows:null
 Component.onCompleted:rows=uiController.createGridModel(columns)
 Component.onDestruction:if(rows&&typeof uiController!=="undefined")uiController.releaseGridModel(rows)
 onColumnsChanged:if(rows)rows.columns=columns
 property real cardWidth:Theme.get("size.card.base_w")*Theme.get("size.card.scale."+sizeValue)
 property real cardHeight:Math.max(Theme.get("size.card.base_h")*Theme.get("size.card.scale."+sizeValue),vrOverlayMode?248:0)
 property int columns:Math.max(1,Math.floor((width-40)/(cardWidth+12)))
 function focusGame(id,dx,dy){if(!rows)return;let r=-1;let col=0;for(let i=0;i<rows.count;i++){let row=rows.row(i);if(row.games)for(let j=0;j<row.games.length;j++)if(row.games[j].gameId===id){r=i;col=j}}if(r<0)return;let target=r;if(dy){target=r+dy;while(target>=0&&target<rows.count&&rows.row(target).header)target+=dy}else{col+=dx;if(col<0){target--;while(target>=0&&rows.row(target).header)target--;if(target>=0)col=rows.row(target).games.length-1}else if(col>=rows.row(r).games.length){target++;while(target<rows.count&&rows.row(target).header)target++;col=0}}if(target<0||target>=rows.count)return;col=Math.min(col,rows.row(target).games.length-1);list.positionViewAtIndex(target,ListView.Contain);Qt.callLater(()=>{let item=list.itemAtIndex(target);if(item)item.focusCard(col)})}
 ListView {
  id:list;objectName:"sectionedGrid";anchors.fill:parent;clip:true;visible:root.view==="List";model:root.rows;spacing:12;reuseItems:true;cacheBuffer:cardHeight*2;boundsBehavior:Flickable.StopAtBounds;ScrollBar.vertical:ScrollBar{}
  header:Column {width:list.width;spacing:16
   FeaturedBanner {width:parent.width-40;anchors.horizontalCenter:parent.horizontalCenter;visible:!(uiSettings.values,uiSettings.get("bannerListDisabled",false));height:visible?implicitHeight||156:0;game:uiController.featured[featured.index]||({});sizeValue:root.sizeValue;vrOverlayMode:root.vrOverlayMode;onShowRequested:id=>root.detailRequested(id);onExploreRequested:root.exploreRequested();onDisableRequested:uiSettings.set("bannerListDisabled",true)}
   RecentlyPlayedRow {width:parent.width-40;anchors.horizontalCenter:parent.horizontalCenter;vrOverlayMode:root.vrOverlayMode;onDetailRequested:id=>root.detailRequested(id)}
  }
  delegate:Item {id:gridRow;objectName:"gridRow";required property var rowData;property var modelData:rowData;function focusCard(i){let c=cardRepeater.itemAt(i);if(c)c.forceActiveFocus()} width:list.width;height:modelData.header?40:root.cardHeight
   SectionHeader {anchors.horizontalCenter:parent.horizontalCenter;width:parent.width-40;visible:modelData.header;title:modelData.label||"";count:modelData.count||0;accent:Theme.get(modelData.id==="gun"?"color.genre.gun":"color.genre.racing")}
   Row {anchors.horizontalCenter:parent.horizontalCenter;spacing:12;visible:!modelData.header
    Repeater {id:cardRepeater;model:(gridRow.modelData.games||[]).length;GameCard {required property int index;game:gridRow.modelData.games[index];sizeValue:root.sizeValue;vrOverlayMode:root.vrOverlayMode;scrolling:list.moving||quiet.running;effectsEnabled:root.effectsEnabled;onDetailRequested:id=>root.detailRequested(id);onActionRequested:id=>root.actionRequested(id);onPreviewRequested:(game,item)=>root.previewRequested(game,item);onPreviewClosed:root.previewClosed();onNavigationRequested:(id,dx,dy)=>root.focusGame(id,dx,dy)}}
   }
  }
  onMovementStarted:{quiet.stop();root.previewClosed()}
  onMovementEnded:quiet.restart()
  Keys.onPressed:event=>{if(event.key===Qt.Key_Home){positionViewAtBeginning();event.accepted=true}else if(event.key===Qt.Key_End){positionViewAtEnd();event.accepted=true}else if(event.key===Qt.Key_PageDown){contentY=Math.min(contentHeight-height,contentY+height);event.accepted=true}else if(event.key===Qt.Key_PageUp){contentY=Math.max(0,contentY-height);event.accepted=true}}
 }
 GridView {
  id:portraits;objectName:"portraitGrid";anchors.fill:parent;visible:root.view==="Library";clip:true;model:visible?gameFilter:null;cellWidth:Theme.get("size.library_tile."+root.sizeValue+".w")+12;cellHeight:Theme.get("size.library_tile."+root.sizeValue+".h")+12;cacheBuffer:cellHeight;ScrollBar.vertical:ScrollBar{}
  delegate:LibraryTile {vrOverlayMode:root.vrOverlayMode;required property string gameId;required property string title;required property color stateColour;required property color accent;required property string hardwareLabel;required property string genreId;required property int year;required property string artPortrait;game:({gameId:gameId,title:title,stateColour:stateColour,accent:accent,hardwareLabel:hardwareLabel,genreId:genreId,year:year});source:artPortrait;sizeValue:root.sizeValue;onDetailRequested:id=>root.detailRequested(id)}
 }
 Timer{id:quiet;interval:Theme.get("motion.scroll_quiet")}
 Timer{id:featured;property int index:0;interval:300000+Math.random()*600000;repeat:true;running:true;onTriggered:{index=(index+1)%Math.max(1,uiController.featured.length);interval=300000+Math.random()*600000}}
 UiText{anchors.centerIn:parent;visible:gameFilter.visibleCount===0;text:"No games match these filters. Clear filters to show the catalog."}
}
