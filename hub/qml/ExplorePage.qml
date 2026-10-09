// SPDX-License-Identifier: GPL-3.0-only
import QtQuick
import QtQuick.Controls
import AladdinsCastle.Hub
ScrollView {
 id:root
 property string sizeValue:"S"
 property bool vrOverlayMode:false
 property int bucket:-1
 property int pick:0
 signal detailRequested(string id)
 clip:true
 Column{width:root.availableWidth;spacing:18
  FeaturedBanner{width:parent.width;explore:true;game:uiController.featured[root.pick]||({});vrOverlayMode:root.vrOverlayMode;onShowRequested:id=>root.detailRequested(id);onShuffleRequested:root.pick=Math.floor(Math.random()*Math.max(1,uiController.featured.length))}
  Flow{width:parent.width;spacing:5;PillButton{text:"All buckets";selected:root.bucket===-1;onClicked:root.bucket=-1} Repeater{model:uiController.exploreRows;PillButton{required property var modelData;text:modelData.label;selected:root.bucket===modelData.id;onClicked:root.bucket=modelData.id}}}
  Flow{width:parent.width;spacing:5;UiText{text:"VR"} Repeater{model:gameFilter.choices("vrKeys");PillButton{required property var modelData;text:modelData.label;selected:uiController.activeFacets.length>=0&&uiController.selected("vrKeys",modelData.id);onClicked:uiController.toggleFacet("vrKeys",modelData.id)}}}
  Repeater{model:uiController.exploreRows
   Column{required property var modelData;width:parent.width;visible:root.bucket===-1||root.bucket===modelData.id;height:visible?implicitHeight:0;spacing:8
    SectionHeader{width:parent.width;title:modelData.label;count:modelData.count}
    Row{width:parent.width;spacing:6;PillButton{text:"‹";width:40;Accessible.name:"Previous games";onClicked:row.positionViewAtIndex(Math.max(0,row.currentIndex-2),ListView.Beginning)}
     ListView{id:row;orientation:ListView.Horizontal;width:parent.width-92;height:Theme.get("size.explore_tile."+root.sizeValue+".h");model:modelData.games;spacing:10;clip:true;delegate:LibraryTile{required property var modelData;game:modelData;explore:true;sizeValue:root.sizeValue;onDetailRequested:id=>root.detailRequested(id)}}
     PillButton{text:"›";width:40;Accessible.name:"Next games";onClicked:{row.currentIndex=Math.min(row.count-1,row.currentIndex+2);row.positionViewAtIndex(row.currentIndex,ListView.Beginning)}}
    }
   }
  }
 }
}
