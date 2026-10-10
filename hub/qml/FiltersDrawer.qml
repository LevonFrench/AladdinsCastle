// SPDX-License-Identifier: GPL-3.0-only
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import AladdinsCastle.Hub
Popup {
 id:drawer
 property bool vrOverlayMode:false
 property var expansionById: ({})
 function expandedFor(nodeId, fallback) { return Object.prototype.hasOwnProperty.call(expansionById,nodeId) ? expansionById[nodeId] : fallback }
 function setExpanded(nodeId, value) { var next = {}; for (var key in expansionById) next[key]=expansionById[key]; next[nodeId]=value; expansionById=next }
 objectName:"filtersDrawer"
 width:Math.min(parent?parent.width-20:420,420);height:Math.min(parent?parent.height-20:650,650);padding:16;modal:true;closePolicy:Popup.CloseOnEscape|Popup.CloseOnPressOutside
 background:Rectangle{color:Theme.get("color.surface.panel");radius:8;border.color:Theme.get("color.line.button")}
 ScrollView {anchors.fill:parent;clip:true;ScrollBar.vertical.policy:ScrollBar.AlwaysOn
 Column {width:drawer.availableWidth-16;spacing:10
  UiText{text:"HARDWARE";color:Theme.get("color.text.label");font.pixelSize:11}
  Repeater {model:uiController.hardwareTree
   Column {required property var modelData;objectName:"hardwareKind-"+modelData.id;property bool expanded:drawer.expandedFor(modelData.id,true);width:parent.width
    Row {spacing:5;PillButton{vrOverlayMode:drawer.vrOverlayMode;text:parent.parent.expanded?"▾":"▸";width:vrOverlayMode?44:40;Accessible.name:"Expand "+modelData.label;onClicked:drawer.setExpanded(modelData.id,!parent.parent.expanded)} PillButton{vrOverlayMode:drawer.vrOverlayMode;text:(uiController.hardwareSelection(modelData.id)===1?"− ":"")+modelData.label+" ("+modelData.count+")";selected:uiController.activeFacets.length>=0&&uiController.hardwareSelection(modelData.id)===2;onClicked:uiController.toggleFacet("hardwareIds",modelData.id)}}
    Repeater {model:parent.expanded?modelData.children:[]
     Column {required property var modelData;objectName:"hardwareFamily-"+modelData.id;property bool expanded:drawer.expandedFor(modelData.id,false);width:parent.width
      Row {x:16;spacing:5;PillButton{vrOverlayMode:drawer.vrOverlayMode;text:parent.parent.expanded?"▾":"▸";width:vrOverlayMode?44:40;Accessible.name:"Expand "+modelData.label;onClicked:drawer.setExpanded(modelData.id,!parent.parent.expanded)} PillButton{vrOverlayMode:drawer.vrOverlayMode;text:(uiController.hardwareSelection(modelData.id)===1?"− ":"")+modelData.label+" ("+modelData.count+")";selected:uiController.activeFacets.length>=0&&uiController.hardwareSelection(modelData.id)===2;onClicked:uiController.toggleFacet("hardwareIds",modelData.id)}}
      Repeater {model:parent.expanded?modelData.children:[];PillButton {vrOverlayMode:drawer.vrOverlayMode;required property var modelData;x:32;width:parent.width-32;text:modelData.label+" ("+modelData.count+")";selected:uiController.activeFacets.length>=0&&uiController.hardwareSelection(modelData.id)===2;onClicked:uiController.toggleFacet("hardwareIds",modelData.id)}}
     }
    }
   }
  }
  Repeater {model:[{title:"GRAPHICS",key:"graphicsIds"},{title:"MANUFACTURER",key:"manufacturerIds"},{title:"DECADE",key:"decades"},{title:"VR",key:"vrKeys"},{title:"PLAYERS",key:"playersBuckets"},{title:"CONTROLS",key:"controlsTypes"}]
   Column {required property var modelData;width:parent.width;spacing:5
    UiText{text:modelData.title;color:Theme.get("color.text.label");font.pixelSize:11}
    Flow {width:parent.width;spacing:5;property string facetKey:parent.modelData.key;Repeater {model:(gameFilter.visibleCount,uiController.activeFacets,gameFilter.choices(parent.facetKey));PillButton {vrOverlayMode:drawer.vrOverlayMode;required property var modelData;text:modelData.label+" ("+modelData.count+")";selected:uiController.activeFacets.length>=0&&uiController.selected(parent.facetKey,modelData.id);onClicked:uiController.toggleFacet(parent.facetKey,modelData.id)}}}
   }
  }
  UiText{text:"YEAR";color:Theme.get("color.text.label");font.pixelSize:11}
  RangeSlider {id:years;width:parent.width;from:1970;to:2026;stepSize:1;first.value:gameFilter.facet("yearMin")||1970;second.value:gameFilter.facet("yearMax")||2026;first.onMoved:gameFilter.setFacet("yearMin",Math.round(first.value));second.onMoved:gameFilter.setFacet("yearMax",Math.round(second.value));Accessible.name:"Year range"}
  UiText{text:Math.round(years.first.value)+" – "+Math.round(years.second.value)}
  PillButton{vrOverlayMode:drawer.vrOverlayMode;text:gameFilter.scanComplete?"IN MY LIBRARY":"Scan my files first";enabled:gameFilter.scanComplete;selected:gameFilter.facet("inLibraryOnly")===true;onClicked:gameFilter.setFacet("inLibraryOnly",!selected)}
  Row {spacing:10;PillButton{vrOverlayMode:drawer.vrOverlayMode;text:"Clear";onClicked:gameFilter.clearFacets()} PillButton{vrOverlayMode:drawer.vrOverlayMode;text:"Done";onClicked:drawer.close()}}
 }
 }
 WheelHandler {onWheel:drawer.close()}
}
