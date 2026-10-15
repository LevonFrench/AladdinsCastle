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
 // Direction C: rows of tables. Recently played and favourites first, then one row per era.
 property var shelves:{
  let list=[],recentRows=filtered.filter(r=>Number(r.lastPlayed)>0).sort((a,b)=>Number(b.lastPlayed)-Number(a.lastPlayed)).slice(0,12),favouriteRows=filtered.filter(r=>r.favourite===true)
  if(recentRows.length&&!recent.checked)list.push({heading:"Recently played  ·  "+recentRows.length,items:recentRows})
  if(favouriteRows.length&&!favourites.checked)list.push({heading:"Favourites  ·  "+favouriteRows.length,items:favouriteRows})
  let eras={};for(let row of filtered){const year=Number(row.year);const key=year>1900?String(Math.floor(year/10)*10):"other";(eras[key]=eras[key]||[]).push(row)}
  for(let key of Object.keys(eras).sort()){const items=eras[key];list.push({heading:(key==="other"?"Other tables":key+"s")+"  ·  "+items.length,items:items})}
  return list
 }
 property int shelfIndex:0
 property int tileIndex:0
 readonly property var focusedTable:{const shelf=shelves[Math.min(shelfIndex,shelves.length-1)];return shelf?shelf.items[Math.min(tileIndex,shelf.items.length-1)]||({}):({})}
 function moveFocus(dShelf,dTile){
  if(!shelves.length)return
  shelfIndex=Math.max(0,Math.min(shelves.length-1,shelfIndex+dShelf))
  tileIndex=Math.max(0,Math.min(shelves[shelfIndex].items.length-1,tileIndex+dTile))
  grid.positionViewAtIndex(shelfIndex,ListView.Contain)
 }
 function tileOf(row){const ok=row.romState==="ready"||row.romState==="needs no ROM";return {title:row.title,system:"pinball",art:row.artThumb?fileUrl(row.artThumb):(row.wheelUrl||""),badge:row.error?"Scan error":ok?(row.favourite?"Favourite":""):stateText(row.romState),badgeColour:ok?Theme.get("color.text.primary"):Theme.get("color.state.needs_neon")}}
 function tableLine(row){return [row.maker||"",row.year||""].join(" ").trim()}
 focus:!selected.id
 Keys.onLeftPressed:moveFocus(0,-1)
 Keys.onRightPressed:moveFocus(0,1)
 Keys.onUpPressed:moveFocus(-1,0)
 Keys.onDownPressed:moveFocus(1,0)
 Keys.onReturnPressed:if(focusedTable.id)uiController.selectPinball(focusedTable.id)
 Keys.onEnterPressed:if(focusedTable.id)uiController.selectPinball(focusedTable.id)
 // A folder chosen earlier shows its last scan when the tab opens, so the tables are there without a desktop step.
 property string startItem:""
 Component.onCompleted:{uiController.openPinball();if(startItem.length)uiController.selectPinball(startItem);page.forceActiveFocus()}
 FolderDialog {id:folder;onAccepted:{uiSettings.set(page.locationKey,uiController.localPath(selectedFolder));uiController.scanPinball()}}
 FileDialog {id:file;onAccepted:{uiSettings.set(page.locationKey,uiController.localPath(selectedFile));if(page.locationKey!=="pinballExecutable")uiController.scanPinball()}}
 readonly property real side:vrOverlayMode?28:40
 ColumnLayout {anchors.fill:parent;spacing:0;visible:!page.selected.id
  HeroBanner {objectName:"pinballHero";Layout.fillWidth:true;Layout.preferredHeight:page.height*(page.vrOverlayMode?0.38:0.42);vrOverlayMode:page.vrOverlayMode;system:"pinball"
   art:page.fileUrl(page.focusedTable.artBackglass)
   eyebrow:("Pinball"+(page.tableLine(page.focusedTable)?"  ·  "+page.tableLine(page.focusedTable):"")).toUpperCase()
   title:page.focusedTable.title||"Pinball"
   meta:page.focusedTable.id?(page.focusedTable.error||page.stateText(page.focusedTable.romState))+(page.focusedTable.vrReady===true?"":" · Desktop only: not marked VR-ready"):"Choose your table folder, then Scan."
   metaColour:page.focusedTable.error||page.focusedTable.romState==="ROM missing"?Theme.get("color.state.needs_neon"):Theme.get("color.text.muted")
   PillButton {objectName:"pinballHeroDesktop";visible:!!page.focusedTable.id;targetHeight:page.vrOverlayMode?56:48;vrOverlayMode:page.vrOverlayMode;neon:true;accent:Theme.get("color.state.ready_neon");text:"Launch on desktop";enabled:page.startable(page.focusedTable);onClicked:uiController.launchPinball(page.focusedTable.id,"desktop")}
   PillButton {objectName:"pinballHeroHeadset";visible:!!page.focusedTable.id;targetHeight:page.vrOverlayMode?56:48;vrOverlayMode:page.vrOverlayMode;text:"Launch in headset";enabled:page.headsetReady(page.focusedTable);onClicked:uiController.launchPinball(page.focusedTable.id,"headset")}
   PillButton {visible:!!page.focusedTable.id;targetHeight:page.vrOverlayMode?56:48;vrOverlayMode:page.vrOverlayMode;text:"Details";onClicked:uiController.selectPinball(page.focusedTable.id)}
   extra:[
    PillButton {text:uiController.pinballScanning?"Scanning…":"Scan";vrOverlayMode:page.vrOverlayMode;enabled:!uiController.pinballScanning;onClicked:uiController.scanPinball()},
    PillButton {text:"Choose folder";visible:!page.vrOverlayMode;vrOverlayMode:page.vrOverlayMode;onClicked:{page.locationKey="pinballFolder";folder.open()}},
    PillButton {text:"Database";visible:!page.vrOverlayMode;vrOverlayMode:page.vrOverlayMode;onClicked:{page.locationKey="pinballDatabase";file.nameFilters=["JSON (*.json)"];file.open()}},
    PillButton {text:"ROM folder";visible:!page.vrOverlayMode;vrOverlayMode:page.vrOverlayMode;onClicked:{page.locationKey="pinballRoms";folder.open()}},
    PillButton {text:"Media folder";visible:!page.vrOverlayMode;vrOverlayMode:page.vrOverlayMode;onClicked:{page.locationKey="pinballMedia";folder.open()}}]
  }
  // Search and filters, on one slim line; the reason for a waiting headset launch beside the count.
  RowLayout {Layout.fillWidth:true;Layout.leftMargin:page.side;Layout.rightMargin:page.side;Layout.topMargin:2;spacing:8
   UiText {objectName:"pinballCount";text:page.showAll?String(page.scanData.vrReady||0)+" VR-ready of "+String(page.scanData.total||0)+" tables":page.filtered.length+" tables ready to play";color:Theme.get("color.text.muted");font.pixelSize:page.vrOverlayMode?16:13;wrapMode:Text.NoWrap}
   UiText {objectName:"pinballHeadsetReason";visible:text.length>0;text:page.headsetReason?"Start SteamVR for Launch in headset":"";color:Theme.get("color.state.needs_neon");font.pixelSize:page.vrOverlayMode?16:13;Layout.fillWidth:true;elide:Text.ElideRight;wrapMode:Text.NoWrap}
   Item {visible:!page.headsetReason;Layout.fillWidth:true}
   TextField {id:query;objectName:"pinballSearch";Layout.preferredWidth:page.vrOverlayMode?220:200;Layout.preferredHeight:page.vrOverlayMode?52:38;placeholderText:"Search tables";Accessible.name:"Search Pinball tables"
    color:Theme.get("color.text.primary");placeholderTextColor:Theme.get("color.text.faint");font.family:Theme.get("font.family");font.pixelSize:page.vrOverlayMode?16:13;selectByMouse:true
    onActiveFocusChanged:if(activeFocus&&page.vrOverlayMode&&typeof overlayHost!=="undefined")overlayHost.requestKeyboard(query)
    background:Rectangle {radius:Theme.get("radius.pill");color:Theme.get("color.surface.pill");border.width:query.activeFocus?2:0;border.color:Theme.get("color.glass.ring")}}
   UiCombo {id:maker;vrOverlayMode:page.vrOverlayMode;Layout.preferredWidth:page.vrOverlayMode?170:140;Layout.preferredHeight:page.vrOverlayMode?52:38;model:page.makerOptions()}
   UiCombo {id:decade;vrOverlayMode:page.vrOverlayMode;Layout.preferredHeight:page.vrOverlayMode?52:38;model:["All decades","1950","1960","1970","1980","1990","2000","2010","2020"]}
   UiCombo {id:type;visible:!page.vrOverlayMode;vrOverlayMode:page.vrOverlayMode;Layout.preferredHeight:38;model:["All types","SS","EM","Original"]}
   UiCombo {id:sort;visible:!page.vrOverlayMode;vrOverlayMode:page.vrOverlayMode;Layout.preferredHeight:38;model:["Title","Recent"]}
   PillButton {id:favourites;checkable:true;text:"Favourites";selected:checked;vrOverlayMode:page.vrOverlayMode;targetHeight:page.vrOverlayMode?52:38}
   PillButton {id:recent;checkable:true;text:"Recent";selected:checked;vrOverlayMode:page.vrOverlayMode;targetHeight:page.vrOverlayMode?52:38}
   PillButton {id:ready;objectName:"pinballReady";checkable:true;checked:!page.showAll;visible:page.showAll;text:"Ready to play";selected:checked;vrOverlayMode:page.vrOverlayMode;targetHeight:page.vrOverlayMode?52:38}
   PillButton {id:excluded;objectName:"pinballExcluded";checkable:true;visible:page.showAll;text:"Show excluded";selected:checked;vrOverlayMode:page.vrOverlayMode;targetHeight:page.vrOverlayMode?52:38}
  }
  ListView {id:grid;objectName:"pinballGrid";property int count:page.filtered.length;Layout.fillWidth:true;Layout.fillHeight:true;Layout.topMargin:14;clip:true;spacing:page.vrOverlayMode?18:24;model:page.shelves
   boundsBehavior:Flickable.StopAtBounds;bottomMargin:24;ScrollBar.vertical:ScrollBar{}
   delegate:ShelfRow {required property var modelData;required property int index;width:grid.width;vrOverlayMode:page.vrOverlayMode;tileName:"pinballTile";tileWidth:page.vrOverlayMode?360:300
    heading:modelData.heading;items:modelData.items;tileOf:page.tileOf;focusIndex:index===page.shelfIndex?page.tileIndex:-1
    onHovered:i=>{page.shelfIndex=index;page.tileIndex=i}
    onActivated:i=>{page.shelfIndex=index;page.tileIndex=i;uiController.selectPinball(modelData.items[i].id)}
    // Each table keeps its own two launch buttons under its picture.
    footerHeight:page.vrOverlayMode?48:34
    footer:Component {RowLayout {spacing:8
     PillButton {objectName:"pinballTileDesktop";Layout.fillWidth:true;Layout.preferredWidth:1;targetHeight:page.vrOverlayMode?48:34;vrOverlayMode:page.vrOverlayMode;neon:true;accent:Theme.get("color.state.ready_neon");text:"Launch on desktop";enabled:page.startable(parent.parent.modelData);onClicked:uiController.launchPinball(parent.parent.modelData.id,"desktop")}
     PillButton {objectName:"pinballTileHeadset";Layout.fillWidth:true;Layout.preferredWidth:1;targetHeight:page.vrOverlayMode?48:34;vrOverlayMode:page.vrOverlayMode;text:"Launch in headset";enabled:page.headsetReady(parent.parent.modelData);onClicked:uiController.launchPinball(parent.parent.modelData.id,"headset")}}}
   }
  }
 }
 // A table's own page: the hero with its backglass and the two launches, then everything else about it.
 Loader {anchors.fill:parent;active:!!page.selected.id;sourceComponent:Component{ColumnLayout {spacing:0
  HeroBanner {Layout.fillWidth:true;Layout.preferredHeight:page.height*(page.vrOverlayMode?0.4:0.44);vrOverlayMode:page.vrOverlayMode;system:"pinball";art:page.fileUrl(page.selected.artBackglass)
   eyebrow:("Pinball"+(page.tableLine(page.selected)?"  ·  "+page.tableLine(page.selected):"")).toUpperCase()
   title:page.selected.title||""
   meta:(page.selected.error||page.stateText(page.selected.romState))+(page.selected.authors?" · by "+page.selected.authors:"")+(page.selected.version?" · version "+page.selected.version:"")
   metaColour:page.selected.error||page.selected.romState==="ROM missing"?Theme.get("color.state.needs_neon"):Theme.get("color.text.muted")
   PillButton {objectName:"pinballLaunchDesktop";targetHeight:page.vrOverlayMode?56:48;neon:true;accent:Theme.get("color.state.ready_neon");text:"Launch on desktop";vrOverlayMode:page.vrOverlayMode;enabled:page.startable(page.selected);onClicked:uiController.playPinball("desktop")}
   PillButton {objectName:"pinballLaunchHeadset";targetHeight:page.vrOverlayMode?56:48;text:"Launch in headset";vrOverlayMode:page.vrOverlayMode;enabled:page.headsetReady(page.selected);onClicked:uiController.playPinball("headset")}
   PillButton {targetHeight:page.vrOverlayMode?56:48;text:"‹ Back";vrOverlayMode:page.vrOverlayMode;onClicked:uiController.selectPinball("")}
  }
  Text {objectName:"pinballDetailTitle";visible:false;text:page.selected.title||""}
  ScrollView {id:detail;Layout.fillWidth:true;Layout.fillHeight:true;clip:true;contentWidth:availableWidth
   Column {x:page.side;width:detail.availableWidth-2*page.side;spacing:12;topPadding:4;bottomPadding:24
    Flow {width:parent.width;spacing:8
     PillButton {text:page.selected.favourite?"Unfavourite":"Favourite";vrOverlayMode:page.vrOverlayMode;onClicked:uiController.favouritePinball()}
     PillButton {text:"Mark VR-ready";vrOverlayMode:page.vrOverlayMode;onClicked:uiController.markPinball(true)}
     PillButton {text:"Exclude";vrOverlayMode:page.vrOverlayMode;onClicked:uiController.markPinball(false)}
     PillButton {text:"Locate VPX";visible:!page.vrOverlayMode;vrOverlayMode:page.vrOverlayMode;onClicked:{page.locationKey="pinballExecutable";file.nameFilters=["Program (*.exe)"];file.open()}}
    }
    UiText {objectName:"pinballAccepted";width:parent.width;wrapMode:Text.Wrap;color:Theme.get("color.text.muted_detail");font.pixelSize:page.vrOverlayMode?16:14
     text:{let when=(uiSettings.values,uiSettings.acceptedOn("pinball-"+(page.selected.id||""),page.selected.title||""));return "In the headset: "+(when?"accepted by you ("+when+")":"not yet accepted by you")+"."}}
    UiText {width:parent.width;wrapMode:Text.Wrap;font.pixelSize:page.vrOverlayMode?17:15;text:(page.headsetReason?page.headsetReason+" ":"")+(page.selected.vrReady===true?"":"This table is not marked VR-ready, so it launches on the desktop only. ")+"On the desktop VR stays off and SteamVR is not needed. In the headset, VR controllers: X opens the in-game menu; hold Y for 3 seconds to close the table (with the Hub open as the SteamVR panel), or press Y together with the left stick click."}
    UiText {width:parent.width;wrapMode:Text.Wrap;color:Theme.get("color.text.muted_detail");font.pixelSize:page.vrOverlayMode?15:13;text:Array.from(page.selected.evidence||[]).join(", ")+(page.selected.features&&page.selected.features.length?"  ·  "+Array.from(page.selected.features).join(", "):"")}
    // The gamepad layout the Hub sets for an Xbox-style controller, for both launches.
    Text {text:"Gamepad";color:Theme.get("color.text.primary");font.family:Theme.get("font.family");font.weight:700;font.pixelSize:page.vrOverlayMode?22:19;topPadding:6}
    Grid {objectName:"pinballGamepad";columns:2;columnSpacing:24;rowSpacing:6
     // A Grid fills row by row: control, then what it does.
     Repeater {model:{let cells=[];for(let row of uiController.pinballGamepadLayout())cells.push({text:row.control,strong:true},{text:row.action,strong:false});return cells}
      UiText {required property var modelData;text:modelData.text;font.pixelSize:page.vrOverlayMode?16:14;font.weight:modelData.strong?700:500;color:modelData.strong?Theme.get("color.text.primary"):Theme.get("color.text.body")}}
    }
    UiText {width:parent.width;wrapMode:Text.Wrap;text:uiController.pinballControlsStatus;visible:text.length>0}
    Loader {width:parent.width;active:uiController.pinballControls.rows.length>0;visible:active;height:item?item.implicitHeight:0;source:"qrc:/controls/ControlsView.qml"
     onLoaded:{item.width=width;item.controlsModel=uiController.pinballControls;item.textSize=page.vrOverlayMode?18:14}
     onWidthChanged:if(item)item.width=width
    }
    UiText {objectName:"pinballLaunchHelp";width:parent.width;wrapMode:Text.Wrap;color:Theme.get("color.text.muted_detail");font.pixelSize:page.vrOverlayMode?15:13;text:uiController.pinballLaunchHelp}
   }
  }
 }}}
}
