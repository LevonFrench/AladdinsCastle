// SPDX-License-Identifier: GPL-3.0-only
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Dialogs
import AladdinsCastle.Hub
Item {
 id:page
 property bool vrOverlayMode:false
 property var scanData:uiController.pinballData
 property var selected:uiController.pinballSelection
 property var rows:scanData.tables||[]
 function makerOptions(){let result=["All makers"];for(let row of rows)if(row.maker&&result.indexOf(row.maker)<0)result.push(row.maker);return result}
 property var filtered:rows.filter(function(r){
  return (excluded.checked||r.vrReady===true)&&(!query.text||r.title.toLowerCase().indexOf(query.text.toLowerCase())>=0)
   &&(maker.currentIndex===0||r.maker===maker.currentText)
   &&(decade.currentText==="All decades"||Math.floor(Number(r.year)/10)*10===Number(decade.currentText))
   &&(type.currentText==="All types"||r.type===type.currentText)
   &&(!ready.checked||r.romState==="ready"||r.romState==="needs no ROM")
   &&(!favourites.checked||r.favourite===true)&&(!recent.checked||Number(r.lastPlayed)>0)
 }).sort(function(a,b){return sort.currentText==="Recent"?Number(b.lastPlayed||0)-Number(a.lastPlayed||0):String(a.title).localeCompare(String(b.title))})
 property string locationKey:"pinballFolder"
 FolderDialog {id:folder;onAccepted:{uiSettings.set(page.locationKey,uiController.localPath(selectedFolder));uiController.scanPinball()}}
 FileDialog {id:file;onAccepted:{uiSettings.set(page.locationKey,uiController.localPath(selectedFile));if(page.locationKey!=="pinballExecutable")uiController.scanPinball()}}
 ColumnLayout {anchors.fill:parent;spacing:8;visible:!page.selected.id
  RowLayout {spacing:8
   UiText {text:"Pinball";font.pixelSize:24;Layout.fillWidth:true}
   PillButton {text:"Choose folder";vrOverlayMode:page.vrOverlayMode;onClicked:{page.locationKey="pinballFolder";folder.open()}}
   PillButton {text:uiController.pinballScanning?"Scanning…":"Scan";vrOverlayMode:page.vrOverlayMode;enabled:!uiController.pinballScanning;onClicked:uiController.scanPinball()}
   PillButton {text:"Database";vrOverlayMode:page.vrOverlayMode;onClicked:{page.locationKey="pinballDatabase";file.nameFilters=["JSON (*.json)"];file.open()}}
   PillButton {text:"ROM folder";vrOverlayMode:page.vrOverlayMode;onClicked:{page.locationKey="pinballRoms";folder.open()}}
   PillButton {text:"Media folder";vrOverlayMode:page.vrOverlayMode;onClicked:{page.locationKey="pinballMedia";folder.open()}}
  }
  UiText {text:String(page.scanData.vrReady||0)+" VR-ready of "+String(page.scanData.total||0)+" tables"}
  RowLayout {spacing:8
   TextField {id:query;objectName:"pinballSearch";Layout.fillWidth:true;Layout.preferredHeight:page.vrOverlayMode?56:40;placeholderText:"Search tables";Accessible.name:"Search Pinball tables"}
   PillButton {text:"Keyboard";vrOverlayMode:page.vrOverlayMode;visible:page.vrOverlayMode;onClicked:if(typeof overlayHost!=="undefined")overlayHost.requestKeyboard(query)}
   ComboBox {id:maker;Layout.preferredWidth:150;Layout.preferredHeight:page.vrOverlayMode?56:40;model:page.makerOptions();delegate:ItemDelegate{width:maker.width;height:page.vrOverlayMode?56:40;text:modelData}}
   ComboBox {id:decade;Layout.preferredHeight:page.vrOverlayMode?56:40;model:["All decades","1950","1960","1970","1980","1990","2000","2010","2020"];delegate:ItemDelegate{width:decade.width;height:page.vrOverlayMode?56:40;text:modelData}}
   ComboBox {id:type;Layout.preferredHeight:page.vrOverlayMode?56:40;model:["All types","SS","EM","Original"];delegate:ItemDelegate{width:type.width;height:page.vrOverlayMode?56:40;text:modelData}}
   ComboBox {id:sort;Layout.preferredHeight:page.vrOverlayMode?56:40;model:["Title","Recent"];delegate:ItemDelegate{width:sort.width;height:page.vrOverlayMode?56:40;text:modelData}}
  }
  RowLayout {spacing:8
   PillButton {id:ready;checkable:true;text:"Ready to play";selected:checked;vrOverlayMode:page.vrOverlayMode}
   PillButton {id:favourites;checkable:true;text:"Favourites";selected:checked;vrOverlayMode:page.vrOverlayMode}
   PillButton {id:recent;checkable:true;text:"Recently played";selected:checked;vrOverlayMode:page.vrOverlayMode}
   PillButton {id:excluded;checkable:true;text:"Show excluded";selected:checked;vrOverlayMode:page.vrOverlayMode}
  }
  GridView {id:grid;objectName:"pinballGrid";Layout.fillWidth:true;Layout.fillHeight:true;clip:true;cellWidth:Math.max(210,width/Math.max(1,Math.floor(width/210)));cellHeight:196;model:page.filtered;boundsBehavior:Flickable.StopAtBounds
   ScrollBar.vertical:ScrollBar{}
   delegate:Button {objectName:"pinballTile";required property var modelData;width:grid.cellWidth-8;height:grid.cellHeight-8;Accessible.name:modelData.title;onClicked:uiController.selectPinball(modelData.id)
    contentItem:Column {spacing:8;Image{width:parent.width;height:visible?64:0;visible:!!modelData.wheelUrl;source:modelData.wheelUrl||"";sourceSize.width:256;sourceSize.height:64;fillMode:Image.PreserveAspectFit}UiText{width:parent.width;text:modelData.title;wrapMode:Text.Wrap;font.pixelSize:18}UiText{width:parent.width;text:(modelData.maker||"")+" "+(modelData.year||"");wrapMode:Text.Wrap}UiText{width:parent.width;text:modelData.romState;wrapMode:Text.Wrap}}
   }
  }
  RowLayout {spacing:8;visible:page.vrOverlayMode
   PillButton {text:"Previous page";vrOverlayMode:true;onClicked:grid.contentY=Math.max(0,grid.contentY-grid.height)}
   PillButton {text:"Next page";vrOverlayMode:true;onClicked:grid.contentY=Math.min(Math.max(0,grid.contentHeight-grid.height),grid.contentY+grid.height)}
  }
 }
 Loader {anchors.fill:parent;active:!!page.selected.id;sourceComponent:Component{ScrollView {clip:true
  Column {width:parent.width;spacing:12
   PillButton {text:"Back to tables";vrOverlayMode:page.vrOverlayMode;onClicked:uiController.selectPinball("")}
   UiText {objectName:"pinballDetailTitle";width:parent.width;text:page.selected.title||"";font.pixelSize:24;wrapMode:Text.Wrap}
   UiText {width:parent.width;wrapMode:Text.Wrap;text:[page.selected.maker||"",page.selected.year||"",page.selected.authors||"",page.selected.version||""].join(" · ")}
   UiText {width:parent.width;wrapMode:Text.Wrap;text:(page.selected.evidence||[]).join(", ")+"\n"+(page.selected.features||[]).join(", ")+"\n"+(page.selected.error||page.selected.romState||"")}
   UiText {width:parent.width;wrapMode:Text.Wrap;text:"Y exits the table. X opens the in-game menu."}
   UiText {width:parent.width;wrapMode:Text.Wrap;text:uiController.pinballControlsStatus;visible:text.length>0}
   Loader {width:parent.width;active:uiController.pinballControls.rows.length>0;visible:active;height:item?item.implicitHeight:0;source:"qrc:/controls/ControlsView.qml"
    onLoaded:{item.width=width;item.controlsModel=uiController.pinballControls;item.textSize=page.vrOverlayMode?18:14}
    onWidthChanged:if(item)item.width=width
   }
   Row {spacing:8
    PillButton {text:page.selected.favourite?"Unfavourite":"Favourite";vrOverlayMode:page.vrOverlayMode;onClicked:uiController.favouritePinball()}
    PillButton {text:"Mark VR-ready";vrOverlayMode:page.vrOverlayMode;onClicked:uiController.markPinball(true)}
    PillButton {text:"Exclude";vrOverlayMode:page.vrOverlayMode;onClicked:uiController.markPinball(false)}
    PillButton {text:"Locate VPX";vrOverlayMode:page.vrOverlayMode;onClicked:{page.locationKey="pinballExecutable";file.nameFilters=["Program (*.exe)"];file.open()}}
    PillButton {text:"Launch plan";vrOverlayMode:page.vrOverlayMode;onClicked:uiController.planPinball()}
   }
   UiText {width:parent.width;wrapMode:Text.Wrap;text:"Launch is waiting for verified isolation of scores, ROM nvram and table/plugin writes."}
   TextArea {width:parent.width;text:uiController.pinballPlan;readOnly:true;wrapMode:Text.Wrap}
  }
 }}}
}
