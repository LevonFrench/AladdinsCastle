// SPDX-License-Identifier: GPL-3.0-only
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import AladdinsCastle.Hub
Rectangle {
 id:bar
 property bool vrOverlayMode:false
 property string sizeValue:"S"
 property bool statesRevealed:false
 Timer{id:reveal;interval:Theme.get("motion.state_pill_reveal");onTriggered:bar.statesRevealed=gameFilter.scanComplete}
 Connections{target:gameFilter;function onFacetsChanged(){if(!gameFilter.scanComplete)bar.statesRevealed=false;else if(!bar.statesRevealed&&!reveal.running)reveal.restart()}}
 Component.onCompleted:if(gameFilter.scanComplete)reveal.restart()
 signal backPressed()
 signal filtersRequested()
 signal scanRequested()
 signal sizeChosen(string value)
 implicitHeight:column.implicitHeight+16;height:implicitHeight;color:Theme.get("color.surface.filterbar")
 Column {id:column;anchors.left:parent.left;anchors.right:parent.right;anchors.margins:14;anchors.top:parent.top;anchors.topMargin:6;spacing:6
  Flow {width:parent.width;spacing:6
   PillButton {text:"‹";width:40;vrOverlayMode:bar.vrOverlayMode;Accessible.name:"Back";onClicked:bar.backPressed()}
   Repeater {model:[{id:"",label:"All"},{id:"gun",label:"Light gun"},{id:"racing",label:"Racing"}];PillButton {required property var modelData;text:modelData.label;selected:uiController.activeFacets.length>=0&&(gameFilter.facet("genre")||[]).join("")===modelData.id;vrOverlayMode:bar.vrOverlayMode;onClicked:gameFilter.setFacet("genre",modelData.id?[modelData.id]:[])}}
   PillButton {text:"Filters "+uiController.activeFacets.length+" ▾";vrOverlayMode:bar.vrOverlayMode;onClicked:bar.filtersRequested()}
   ScanProgress {vrOverlayMode:bar.vrOverlayMode;onRequested:bar.scanRequested()}
   Repeater {model:["S","M","L"];PillButton {required property string modelData;text:modelData;width:40;selected:bar.sizeValue===modelData;vrOverlayMode:bar.vrOverlayMode;onClicked:bar.sizeChosen(modelData)}}
  }
  Flow {width:parent.width;spacing:6;visible:bar.statesRevealed
   Repeater {model:[{id:"toInstall",label:"To install"},{id:"ready",label:"Ready"},{id:"updates",label:"Updates"},{id:"needsFiles",label:"Needs files"}];PillButton {required property var modelData;text:modelData.label;selected:uiController.activeFacets.length>=0&&uiController.selected("statePills",modelData.id);vrOverlayMode:bar.vrOverlayMode;onClicked:uiController.toggleFacet("statePills",modelData.id)}}
   PillButton {text:"IN MY LIBRARY";selected:uiController.activeFacets.length>=0&&gameFilter.facet("inLibraryOnly")===true;vrOverlayMode:bar.vrOverlayMode;onClicked:gameFilter.setFacet("inLibraryOnly",!selected)}
  }
  Flow {width:parent.width;spacing:4;visible:uiController.activeFacets.length>0
   Repeater {model:uiController.activeFacets;PillButton {required property var modelData;text:modelData.label+" ×";onClicked:if(modelData.key==="inLibraryOnly")gameFilter.setFacet(modelData.key,false);else if(modelData.key==="yearMin"||modelData.key==="yearMax")gameFilter.setFacet(modelData.key,null);else uiController.toggleFacet(modelData.key,modelData.value)}}
   PillButton {text:"Clear all";onClicked:{gameFilter.clearFacets();gameFilter.query=""}}
  }
 }
 Rectangle {anchors.bottom:parent.bottom;width:parent.width;height:1;color:Theme.get("color.surface.filterbar_rule")}
}
