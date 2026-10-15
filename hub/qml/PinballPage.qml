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
 // With the catalog switch off, only tables that can be started are listed.
 property bool showAll:(uiSettings.values,uiSettings.get("showAllGames",false))===true
 function fileUrl(path){return path?"file:///"+String(path).replace(/\\/g,"/"):""}
 // Backglass pictures of the listed tables, for the header.
 property var backglasses:{let list=[];for(let row of filtered)if(row.artBackglass)list.push(row.artBackglass);return list}
 // The header starts past the first row of tiles, so it shows other tables.
 property int headerStart:6
 function stateText(state){return {"ready":"Ready to play","needs no ROM":"Ready to play","ROM missing":"ROM missing","unknown":"ROM not checked"}[state]||state||""}
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
 // Two launches per table, each exactly what it says. The headset one waits for SteamVR; the desktop one never needs it.
 property bool idle:Object.keys(uiController.nowPlaying).length===0
 function startable(row){return !!row&&!!row.id&&!row.error&&row.romState!=="ROM missing"&&page.idle}
 function headsetReady(row){return page.startable(row)&&row.vrReady===true&&uiController.steamVrRunning}
 readonly property string headsetReason:uiController.steamVrRunning?"":"Launch in headset needs SteamVR running, with the headset connected."
 // A folder chosen earlier shows its last scan when the tab opens, so the tables are there without a desktop step.
 property string startItem:""
 Component.onCompleted:{uiController.openPinball();if(startItem.length)uiController.selectPinball(startItem)}
 FolderDialog {id:folder;onAccepted:{uiSettings.set(page.locationKey,uiController.localPath(selectedFolder));uiController.scanPinball()}}
 FileDialog {id:file;onAccepted:{uiSettings.set(page.locationKey,uiController.localPath(selectedFile));if(page.locationKey!=="pinballExecutable")uiController.scanPinball()}}
 ColumnLayout {anchors.fill:parent;spacing:8;visible:!page.selected.id
  RowLayout {spacing:8
   UiText {text:"Pinball";font.pixelSize:24;Layout.fillWidth:true}
   PillButton {text:"Choose folder";visible:!page.vrOverlayMode;vrOverlayMode:page.vrOverlayMode;onClicked:{page.locationKey="pinballFolder";folder.open()}}
   PillButton {text:uiController.pinballScanning?"Scanning…":"Scan";vrOverlayMode:page.vrOverlayMode;enabled:!uiController.pinballScanning;onClicked:uiController.scanPinball()}
   PillButton {text:"Database";visible:!page.vrOverlayMode;vrOverlayMode:page.vrOverlayMode;onClicked:{page.locationKey="pinballDatabase";file.nameFilters=["JSON (*.json)"];file.open()}}
   PillButton {text:"ROM folder";visible:!page.vrOverlayMode;vrOverlayMode:page.vrOverlayMode;onClicked:{page.locationKey="pinballRoms";folder.open()}}
   PillButton {text:"Media folder";visible:!page.vrOverlayMode;vrOverlayMode:page.vrOverlayMode;onClicked:{page.locationKey="pinballMedia";folder.open()}}
  }
  Row {id:header;objectName:"pinballHeader";Layout.fillWidth:true;Layout.preferredHeight:visible?110:0;spacing:8;clip:true;visible:!page.vrOverlayMode&&page.backglasses.length>0
   Repeater {model:Math.min(page.backglasses.length,Math.max(1,Math.floor((header.width+8)/154)))
    Image {required property int index;height:110;width:146;fillMode:Image.PreserveAspectCrop;asynchronous:true;sourceSize.width:480
     source:page.fileUrl(page.backglasses[(page.headerStart+index)%page.backglasses.length])}}
   Timer {interval:6000;repeat:true;running:parent.visible&&!(uiSettings.values,uiSettings.get("reduceMotion",false));onTriggered:page.headerStart=(page.headerStart+1)%Math.max(1,page.backglasses.length)}
  }
  UiText {objectName:"pinballHeadsetReason";visible:text.length>0;text:page.headsetReason;color:Theme.get("color.state.needs_neon")}
  UiText {objectName:"pinballCount";text:page.showAll?String(page.scanData.vrReady||0)+" VR-ready of "+String(page.scanData.total||0)+" tables":page.filtered.length+" tables ready to play"}
  RowLayout {spacing:8
   TextField {id:query;objectName:"pinballSearch";Layout.fillWidth:true;Layout.preferredHeight:page.vrOverlayMode?56:40;placeholderText:"Search tables";Accessible.name:"Search Pinball tables"
    color:Theme.get("color.text.primary");placeholderTextColor:Theme.get("color.text.faint");font.family:Theme.get("font.family");font.pixelSize:page.vrOverlayMode?16:12;selectByMouse:true
    onActiveFocusChanged:if(activeFocus&&page.vrOverlayMode&&typeof overlayHost!=="undefined")overlayHost.requestKeyboard(query)
    background:Rectangle {radius:6;color:Theme.get("color.surface.pill");border.color:query.activeFocus?Theme.get("color.glass.ring"):Theme.get("color.line.strong")}}
   UiCombo {id:maker;vrOverlayMode:page.vrOverlayMode;Layout.preferredWidth:150;Layout.preferredHeight:page.vrOverlayMode?56:40;model:page.makerOptions()}
   UiCombo {id:decade;vrOverlayMode:page.vrOverlayMode;Layout.preferredHeight:page.vrOverlayMode?56:40;model:["All decades","1950","1960","1970","1980","1990","2000","2010","2020"]}
   UiCombo {id:type;vrOverlayMode:page.vrOverlayMode;Layout.preferredHeight:page.vrOverlayMode?56:40;model:["All types","SS","EM","Original"]}
   UiCombo {id:sort;vrOverlayMode:page.vrOverlayMode;Layout.preferredHeight:page.vrOverlayMode?56:40;model:["Title","Recent"]}
  }
  RowLayout {spacing:8
   PillButton {id:ready;objectName:"pinballReady";checkable:true;checked:!page.showAll;visible:page.showAll;text:"Ready to play";selected:checked;vrOverlayMode:page.vrOverlayMode}
   PillButton {id:favourites;checkable:true;text:"Favourites";selected:checked;vrOverlayMode:page.vrOverlayMode}
   PillButton {id:recent;checkable:true;text:"Recently played";selected:checked;vrOverlayMode:page.vrOverlayMode}
   PillButton {id:excluded;objectName:"pinballExcluded";checkable:true;visible:page.showAll;text:"Show excluded";selected:checked;vrOverlayMode:page.vrOverlayMode}
  }
  GridView {id:grid;objectName:"pinballGrid";property bool anyWheel:page.filtered.some(r=>!!(r.wheelUrl||r.artThumb));Layout.fillWidth:true;Layout.fillHeight:true;clip:true;cellWidth:Math.max(210,width/Math.max(1,Math.floor(width/210)));cellHeight:page.vrOverlayMode?Math.max(230,Math.floor(height/Math.max(1,Math.floor(height/230)))):anyWheel?350:250;model:page.filtered;boundsBehavior:Flickable.StopAtBounds
   ScrollBar.vertical:ScrollBar{}
   delegate:Button {id:tile;objectName:"pinballTile";required property var modelData;width:grid.cellWidth-8;height:grid.cellHeight-8;padding:12;Accessible.name:modelData.title;onClicked:uiController.selectPinball(modelData.id)
    background:Rectangle {radius:Theme.get("size.card.radius");color:tile.down?Theme.get("color.surface.menu_hover"):Theme.get("color.surface.card");border.width:tile.activeFocus?2:1;border.color:tile.activeFocus?Theme.get("color.glass.ring"):tile.hovered?Theme.get("color.line.button"):Theme.get("color.line.strong")}
    contentItem:Column {spacing:8;Image{objectName:"pinballTileArt";width:parent.width;height:visible?(modelData.artThumb?(page.vrOverlayMode?80:110):64):0;visible:!page.vrOverlayMode&&!!(modelData.artThumb||modelData.wheelUrl);source:modelData.artThumb?page.fileUrl(modelData.artThumb):(modelData.wheelUrl||"");sourceSize.width:480;fillMode:modelData.artThumb?Image.PreserveAspectCrop:Image.PreserveAspectFit;clip:true;asynchronous:true}UiText{width:parent.width;text:modelData.title;wrapMode:Text.Wrap;font.pixelSize:18;color:Theme.get("color.text.primary");maximumLineCount:grid.cellHeight>=190?3:2;elide:Text.ElideRight}UiText{width:parent.width;text:(modelData.maker||"")+" "+(modelData.year||"");wrapMode:Text.Wrap}UiText{width:parent.width;text:page.stateText(modelData.romState);wrapMode:Text.Wrap;color:modelData.romState==="ready"||modelData.romState==="needs no ROM"?Theme.get("color.state.ready_neon"):Theme.get("color.state.needs_neon")}
     PillButton{objectName:"pinballTileDesktop";width:parent.width;height:page.vrOverlayMode?44:30;text:"Launch on desktop";neon:true;accent:Theme.get("color.state.ready_neon");enabled:page.startable(modelData);vrOverlayMode:page.vrOverlayMode;onClicked:uiController.launchPinball(modelData.id,"desktop")}
     PillButton{objectName:"pinballTileHeadset";width:parent.width;height:page.vrOverlayMode?44:30;text:"Launch in headset";neon:true;accent:Theme.get("color.state.ready_neon");enabled:page.headsetReady(modelData);vrOverlayMode:page.vrOverlayMode;onClicked:uiController.launchPinball(modelData.id,"headset")}}
   }
  }
  RowLayout {spacing:8;visible:page.vrOverlayMode
   property real step:Math.max(1,Math.floor(grid.height/grid.cellHeight))*grid.cellHeight
   PillButton {text:"Previous page";vrOverlayMode:true;enabled:grid.contentY>0;onClicked:grid.contentY=Math.max(0,grid.contentY-parent.step)}
   PillButton {text:"Next page";vrOverlayMode:true;enabled:grid.contentY+grid.height<grid.contentHeight-1;onClicked:grid.contentY=Math.min(Math.max(0,grid.contentHeight-grid.height),grid.contentY+parent.step)}
   UiText {text:grid.count?"Page "+(Math.round(grid.contentY/Math.max(1,parent.step))+1)+" of "+Math.max(1,Math.ceil(grid.contentHeight/Math.max(1,parent.step))):""}
  }
 }
 Loader {anchors.fill:parent;active:!!page.selected.id;sourceComponent:Component{ScrollView {id:detail;clip:true;contentWidth:availableWidth
  Column {width:detail.availableWidth;spacing:12
   PillButton {text:"Back to tables";vrOverlayMode:page.vrOverlayMode;onClicked:uiController.selectPinball("")}
   UiText {objectName:"pinballDetailTitle";width:parent.width;text:page.selected.title||"";font.pixelSize:24;wrapMode:Text.Wrap}
   Image {objectName:"pinballDetailBackglass";visible:!!page.selected.artBackglass;width:parent.width;height:visible?Math.min(page.vrOverlayMode?260:340,width*0.6):0;source:page.fileUrl(page.selected.artBackglass);fillMode:Image.PreserveAspectFit;horizontalAlignment:Image.AlignLeft;asynchronous:true}
   UiText {width:parent.width;wrapMode:Text.Wrap;text:[page.selected.maker||"",page.selected.year||"",page.selected.authors||"",page.selected.version||""].join(" · ")}
   UiText {width:parent.width;wrapMode:Text.Wrap;text:Array.from(page.selected.evidence||[]).join(", ")+"\n"+Array.from(page.selected.features||[]).join(", ")+"\n"+(page.selected.error||page.stateText(page.selected.romState))}
   // Actions come first so Play needs no scrolling, in the headset least of all.
   Flow {width:parent.width;spacing:8
    PillButton {objectName:"pinballLaunchDesktop";neon:true;accent:Theme.get("color.state.ready_neon");text:"Launch on desktop";vrOverlayMode:page.vrOverlayMode;enabled:page.startable(page.selected);onClicked:uiController.playPinball("desktop")}
    PillButton {objectName:"pinballLaunchHeadset";neon:true;accent:Theme.get("color.state.ready_neon");text:"Launch in headset";vrOverlayMode:page.vrOverlayMode;enabled:page.headsetReady(page.selected);onClicked:uiController.playPinball("headset")}
    PillButton {text:page.selected.favourite?"Unfavourite":"Favourite";vrOverlayMode:page.vrOverlayMode;onClicked:uiController.favouritePinball()}
    PillButton {text:"Mark VR-ready";vrOverlayMode:page.vrOverlayMode;onClicked:uiController.markPinball(true)}
    PillButton {text:"Exclude";vrOverlayMode:page.vrOverlayMode;onClicked:uiController.markPinball(false)}
    PillButton {text:"Locate VPX";visible:!page.vrOverlayMode;vrOverlayMode:page.vrOverlayMode;onClicked:{page.locationKey="pinballExecutable";file.nameFilters=["Program (*.exe)"];file.open()}}
   }
   UiText {objectName:"pinballAccepted";width:parent.width;wrapMode:Text.Wrap;color:Theme.get("color.text.muted_detail")
    text:{let when=(uiSettings.values,uiSettings.acceptedOn("pinball-"+(page.selected.id||""),page.selected.title||""));return "In the headset: "+(when?"accepted by you ("+when+")":"not yet accepted by you")+"."}}
   UiText {width:parent.width;wrapMode:Text.Wrap;text:(page.headsetReason?page.headsetReason+" ":"")+(page.selected.vrReady===true?"":"This table is not marked VR-ready, so it launches on the desktop only. ")+"On the desktop VR stays off and SteamVR is not needed. In the headset, VR controllers: X opens the in-game menu; hold Y for 3 seconds to close the table (with the Hub open as the SteamVR panel), or press Y together with the left stick click."}
   // The gamepad layout the Hub sets for an Xbox-style controller, in both views.
   UiText {width:parent.width;text:"Gamepad";font.pixelSize:page.vrOverlayMode?20:16;font.weight:600}
   Grid {objectName:"pinballGamepad";columns:2;columnSpacing:16;rowSpacing:4
    // A Grid fills row by row: control, then what it does.
    Repeater {model:{let cells=[];for(let row of uiController.pinballGamepadLayout())cells.push({text:row.control,strong:true},{text:row.action,strong:false});return cells}
     UiText {required property var modelData;text:modelData.text;font.pixelSize:page.vrOverlayMode?16:13;color:modelData.strong?Theme.get("color.text.primary"):Theme.get("color.text.body")}}
   }
   UiText {width:parent.width;wrapMode:Text.Wrap;text:uiController.pinballControlsStatus;visible:text.length>0}
   Loader {width:parent.width;active:uiController.pinballControls.rows.length>0;visible:active;height:item?item.implicitHeight:0;source:"qrc:/controls/ControlsView.qml"
    onLoaded:{item.width=width;item.controlsModel=uiController.pinballControls;item.textSize=page.vrOverlayMode?18:14}
    onWidthChanged:if(item)item.width=width
   }
   UiText {objectName:"pinballLaunchHelp";width:parent.width;wrapMode:Text.Wrap;text:uiController.pinballLaunchHelp}
  }
 }}}
}
