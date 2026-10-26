// SPDX-License-Identifier: GPL-3.0-only
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Dialogs
import AladdinsCastle.Hub
Item {
 id:page
 property bool vrOverlayMode:false
 property bool reduceMotion:false
 property var scanData:uiController.pinballData
 property var selected:uiController.pinballSelection
 property var rows:scanData.tables||[]
 // Every table in the folder is listed; one that cannot play says why on its tile. Ready to play and
 // VR room only narrow the list when asked. Every readable table plays in the headset unless you exclude it.
 // B in the headset: a table's page closes first, then every search and filter clears at once; false at the plain list.
 function handleBack(){
  if(page.selected.id){uiController.selectPinball("");return true}
  if(query.text.length||ready.checked||vrOnly.checked||favourites.checked||recent.checked||maker.currentIndex>0||decade.currentIndex>0){
   query.text="";ready.checked=false;vrOnly.checked=false;favourites.checked=false;recent.checked=false;maker.currentIndex=0;decade.currentIndex=0;return true}
  return false
 }
 function playable(r){return !r.error&&(r.romState==="ready"||r.romState==="needs no ROM")}
 readonly property var counts:{let ready=0,headset=0;for(const r of rows)if(playable(r)){++ready;if(r.vrReady===true)++headset}return {ready:ready,headset:headset}}
 function fileUrl(path){return path?"file:///"+String(path).replace(/\\/g,"/"):""}
 function stateText(state){return {"ready":"Ready to play","needs no ROM":"Ready to play","ROM missing":"ROM missing","unknown":"ROM not checked"}[state]||state||""}
 function makerOptions(){let result=["All makers"];for(let row of rows)if(row.maker&&result.indexOf(row.maker)<0)result.push(row.maker);return result}
 property var filtered:rows.filter(function(r){
  return (!vrOnly.checked||r.vrRoom===true)&&(!query.text||r.title.toLowerCase().indexOf(query.text.toLowerCase())>=0)
   &&(maker.currentIndex===0||r.maker===maker.currentText)
   &&(decade.currentText==="All decades"||Math.floor(Number(r.year)/10)*10===Number(decade.currentText))
   &&(type.currentText==="All types"||r.type===type.currentText)
   &&(!ready.checked||page.playable(r))
   &&(!favourites.checked||r.favourite===true)&&(!recent.checked||Number(r.lastPlayed)>0)
 }).sort(function(a,b){return sort.currentText==="Recent"?Number(b.lastPlayed||0)-Number(a.lastPlayed||0):String(a.title).localeCompare(String(b.title))})
 property string locationKey:"pinballFolder"
 // Two launches per table, each exactly what it says. The headset one waits for SteamVR; the desktop one never needs it.
 property bool idle:Object.keys(uiController.nowPlaying).length===0
 function startable(row){return !!row&&!!row.id&&!row.error&&row.romState!=="ROM missing"&&page.idle}
 // SteamVR off is no reason to refuse: Launch in headset starts it first, then the table.
 function headsetReady(row){return page.startable(row)&&row.vrReady===true&&!uiController.steamVrStarting}
 readonly property string headsetReason:uiController.steamVrRunning?"":"SteamVR is off: Launch in headset starts it first, then the table."
 // Direction C: rows of tables. Recently played and favourites first, then one row per era.
 property var shelves:{
  let list=[],recentRows=filtered.filter(r=>Number(r.lastPlayed)>0).sort((a,b)=>Number(b.lastPlayed)-Number(a.lastPlayed)).slice(0,12),favouriteRows=filtered.filter(r=>r.favourite===true)
  if(recentRows.length&&!recent.checked)list.push({heading:"Recently played  ·  "+recentRows.length,items:recentRows})
  if(favouriteRows.length&&!favourites.checked)list.push({heading:"Favourites  ·  "+favouriteRows.length,items:favouriteRows})
  // The tables that start in the headset now, first, unless the filters already show only those.
  const headsetRows=filtered.filter(r=>page.playable(r)&&r.vrReady===true)
  if(headsetRows.length&&headsetRows.length<filtered.length)list.push({heading:"Ready in the headset  ·  "+headsetRows.length,items:headsetRows})
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
 // The tile says in two words why a table cannot play, or plays on the desktop only.
 function tileOf(row){const ok=playable(row);return {title:row.title,system:"pinball",art:row.artThumb?fileUrl(row.artThumb):(row.wheelUrl||""),
  badge:row.error?"Can't read":!ok?(row.note&&row.romState==="unknown"?"Locked script":stateText(row.romState)):row.vrReady!==true?"Excluded":row.favourite?"Favourite":row.vrRoom===true?"VR room":"",
  badgeColour:row.error||row.romState==="ROM missing"?Theme.get("color.state.needs_neon"):ok&&row.vrReady===true?Theme.get("color.text.primary"):Theme.get("color.text.muted")}}
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
 // The list of tables. On the desktop the hero and the filters stay above the rows. On the SteamVR panel they
 // are the list's header, so the whole page scrolls as one surface under the stick.
 Item {id:listPage;anchors.fill:parent;visible:!page.selected.id
 ColumnLayout {id:topBlock;objectName:"pinballTop";parent:page.vrOverlayMode&&grid.headerItem?grid.headerItem:listPage;width:listPage.width;spacing:0
  HeroBanner {objectName:"pinballHero";Layout.fillWidth:true;Layout.preferredHeight:page.height*(page.vrOverlayMode?0.38:0.42);vrOverlayMode:page.vrOverlayMode;system:"pinball"
   art:page.fileUrl(page.focusedTable.artBackglass)
   eyebrow:("Pinball"+(page.tableLine(page.focusedTable)?"  ·  "+page.tableLine(page.focusedTable):"")).toUpperCase()
   title:page.focusedTable.title||"Pinball"
   meta:page.focusedTable.id?(page.focusedTable.error||page.focusedTable.note||page.stateText(page.focusedTable.romState))+(page.focusedTable.vrReady===true?"":" · Excluded by you: desktop only"):"Choose your table folder, then Scan."
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
   UiText {objectName:"pinballCount";text:(page.filtered.length===page.rows.length?page.rows.length+" tables":page.filtered.length+" of "+page.rows.length+" tables")+"  ·  "+page.counts.ready+" ready to play  ·  "+page.counts.headset+" ready in the headset";color:Theme.get("color.text.muted");font.pixelSize:page.vrOverlayMode?16:13;wrapMode:Text.NoWrap}
   UiText {objectName:"pinballHeadsetReason";visible:text.length>0;text:page.headsetReason?"SteamVR starts first for Launch in headset":"";color:Theme.get("color.state.needs_neon");font.pixelSize:page.vrOverlayMode?16:13;Layout.fillWidth:true;elide:Text.ElideRight;wrapMode:Text.NoWrap}
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
   PillButton {id:ready;objectName:"pinballReady";checkable:true;text:"Ready to play";selected:checked;vrOverlayMode:page.vrOverlayMode;targetHeight:page.vrOverlayMode?52:38}
   PillButton {id:vrOnly;objectName:"pinballVrOnly";checkable:true;text:"VR room only";selected:checked;vrOverlayMode:page.vrOverlayMode;targetHeight:page.vrOverlayMode?52:38}
  }
 }
 ListView {id:grid;objectName:"pinballGrid";property int count:page.filtered.length;clip:true;spacing:page.vrOverlayMode?18:24;model:page.shelves
   // Placed by number, not anchored to the hero block: on the panel that block lives inside the list.
   x:0;width:listPage.width;y:page.vrOverlayMode?0:topBlock.implicitHeight+14;height:Math.max(0,listPage.height-y)
   header:page.vrOverlayMode?topSpace:null
   // The stick moves it on the panel (the wheel catcher below), so a laser press on a tile is never a drag.
   interactive:!page.vrOverlayMode
   boundsBehavior:Flickable.StopAtBounds;bottomMargin:24;ScrollBar.vertical:ScrollBar{width:page.vrOverlayMode?18:implicitWidth;policy:page.vrOverlayMode&&grid.contentHeight>grid.height?ScrollBar.AlwaysOn:ScrollBar.AsNeeded}
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
 Component {id:topSpace;Item {width:grid.width;height:topBlock.implicitHeight+14}}
 // Catches the stick over the panel's list; presses and hover pass through to the tiles.
 MouseArea {objectName:"pinballWheel";anchors.fill:grid;enabled:page.vrOverlayMode;acceptedButtons:Qt.NoButton;z:2
  onWheel:wheel=>{wheel.accepted=true;listScroller.wheel(wheel.angleDelta.y,Date.now())}}
 // Rests: the top, the first row of tables, then each row (every row of tables is the same height).
 PanelScroller {id:listScroller;objectName:"pinballScroller";flick:grid;reduceMotion:page.reduceMotion
  // Measured, not estimated: the list's first row sits at 0 with its header above it, and a row's pitch is
  // the first row's real height plus the gap. The list's own content height is only an estimate.
  readonly property real headerHeight:grid.headerItem?Math.max(0,-grid.originY):0
  rowHeight:{const first=grid.count>0&&grid.contentHeight>0?grid.itemAtIndex(0):null
   return first&&first.height>0?first.height+grid.spacing:Math.max(1,(grid.contentHeight-headerHeight+grid.spacing)/Math.max(1,page.shelves.length))}
  stops:{const list=[0];for(let i=0;i<page.shelves.length;++i)list.push(headerHeight+i*rowHeight);return list}}
 }
 // A table's own page: the hero with its backglass and the two launches, then everything else about it.
 // One surface: the hero scrolls away with the rest, on the panel and on the desktop.
 Loader {anchors.fill:parent;active:!!page.selected.id;sourceComponent:Component{Item {
  Flickable {id:detail;objectName:"pinballDetailScroll";anchors.fill:parent;clip:true;contentHeight:tableColumn.implicitHeight;boundsBehavior:Flickable.StopAtBounds;interactive:!page.vrOverlayMode
   ScrollBar.vertical:ScrollBar{width:page.vrOverlayMode?18:implicitWidth;policy:detail.contentHeight>detail.height?ScrollBar.AlwaysOn:ScrollBar.AsNeeded}
  ColumnLayout {id:tableColumn;width:detail.width;spacing:0
  HeroBanner {Layout.fillWidth:true;Layout.preferredHeight:page.height*(page.vrOverlayMode?0.4:0.44);vrOverlayMode:page.vrOverlayMode;system:"pinball";art:page.fileUrl(page.selected.artBackglass)
   eyebrow:("Pinball"+(page.tableLine(page.selected)?"  ·  "+page.tableLine(page.selected):"")).toUpperCase()
   title:page.selected.title||""
   meta:(page.selected.error||page.selected.note||page.stateText(page.selected.romState))+(page.selected.authors?" · by "+page.selected.authors:"")+(page.selected.version?" · version "+page.selected.version:"")
   metaColour:page.selected.error||page.selected.romState==="ROM missing"?Theme.get("color.state.needs_neon"):Theme.get("color.text.muted")
   PillButton {objectName:"pinballLaunchDesktop";targetHeight:page.vrOverlayMode?56:48;neon:true;accent:Theme.get("color.state.ready_neon");text:"Launch on desktop";vrOverlayMode:page.vrOverlayMode;enabled:page.startable(page.selected);onClicked:uiController.playPinball("desktop")}
   PillButton {objectName:"pinballLaunchHeadset";targetHeight:page.vrOverlayMode?56:48;text:"Launch in headset";vrOverlayMode:page.vrOverlayMode;enabled:page.headsetReady(page.selected);onClicked:uiController.playPinball("headset")}
   PillButton {targetHeight:page.vrOverlayMode?56:48;text:"‹ Back";vrOverlayMode:page.vrOverlayMode;onClicked:uiController.selectPinball("")}
  }
  Text {objectName:"pinballDetailTitle";visible:false;text:page.selected.title||""}
   Column {Layout.leftMargin:page.side;Layout.preferredWidth:detail.width-2*page.side;spacing:12;topPadding:4;bottomPadding:24
    Flow {width:parent.width;spacing:8
     PillButton {text:page.selected.favourite?"Unfavourite":"Favourite";vrOverlayMode:page.vrOverlayMode;onClicked:uiController.favouritePinball()}
     PillButton {text:"Include in headset";vrOverlayMode:page.vrOverlayMode;onClicked:uiController.markPinball(true)}
     PillButton {text:"Exclude";vrOverlayMode:page.vrOverlayMode;onClicked:uiController.markPinball(false)}
     PillButton {objectName:"pinballVpsEntry";text:"VPS entry";visible:!!page.selected.vpsUrl;vrOverlayMode:page.vrOverlayMode;onClicked:Qt.openUrlExternally(page.selected.vpsUrl)}
     PillButton {text:"Locate VPX";visible:!page.vrOverlayMode;vrOverlayMode:page.vrOverlayMode;onClicked:{page.locationKey="pinballExecutable";file.nameFilters=["Program (*.exe)"];file.open()}}
    }
    UiText {objectName:"pinballAccepted";width:parent.width;wrapMode:Text.Wrap;color:Theme.get("color.text.muted_detail");font.pixelSize:page.vrOverlayMode?16:14
     text:{let when=(uiSettings.values,uiSettings.acceptedOn("pinball-"+(page.selected.id||""),page.selected.title||""));return "In the headset: "+(when?"accepted by you ("+when+")":"not yet accepted by you")+"."}}
    UiText {width:parent.width;wrapMode:Text.Wrap;font.pixelSize:page.vrOverlayMode?17:15;text:(page.headsetReason?page.headsetReason+" ":"")+(page.selected.vrReady===true?"":"You excluded this table from the headset, so it launches on the desktop only. ")+"On the desktop VR stays off and SteamVR is not needed. In the headset, VR controllers: left stick click is Start, right stick click is Coin, A is the plunger button, X opens the in-game menu, and clicking both sticks aligns the table to your hands (keep them held and move your hands to scale and turn it). To leave a table, press the left Menu button, open the Hub panel and choose Exit game."}
    UiText {width:parent.width;wrapMode:Text.Wrap;color:Theme.get("color.text.muted_detail");font.pixelSize:page.vrOverlayMode?15:13;text:Array.from(page.selected.evidence||[]).join(", ")+(page.selected.features&&page.selected.features.length?"  ·  "+Array.from(page.selected.features).join(", "):"")}
    UiText {width:parent.width;wrapMode:Text.Wrap;color:Theme.get("color.text.muted_detail");visible:!!page.selected.romReason;font.pixelSize:page.vrOverlayMode?15:13;text:page.selected.romReason||""}
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
  }
  MouseArea {objectName:"pinballDetailWheel";anchors.fill:detail;enabled:page.vrOverlayMode;acceptedButtons:Qt.NoButton;z:2
   onWheel:wheel=>{wheel.accepted=true;detailScroller.wheel(wheel.angleDelta.y,Date.now())}}
  PanelScroller {id:detailScroller;objectName:"pinballDetailScroller";flick:detail;rowHeight:120;reduceMotion:page.reduceMotion}
 }}}
}
