// SPDX-License-Identifier: GPL-3.0-only
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import AladdinsCastle.Hub
// The SteamVR panel's library, after Steam's: one slim bar of system tabs, search and filter, then one
// grid of every game that scrolls as a single surface. A tile opens the game's own page.
// The stick scrolls whole rows (PanelScroller); B steps back one level (handleBack, called by HubRoot).
FocusScope {
 id:page
 objectName:"panelLibrary"
 property bool vrOverlayMode:true
 property bool reduceMotion:false
 // Every game set up on this PC, in the same order and with the same rows as the VR games page.
 property var rows:uiController.model3Games.concat(uiController.otherGames,uiController.readyCatalogGames)
 property string tab:"all"
 property string query:""
 property bool readyOnly:false
 property string sortKey:"system"
 property string selectedId:""
 property bool searchOpen:false
 property int currentIndex:0
 signal detailRequested(string id)
 signal setupRequested()
 signal pinballRequested()
 signal helpRequested()
 // A new page is on show (the game page opened or closed), for the click guard and B.
 signal pageChanged()
 onSelectedIdChanged:pageChanged()
 Component.onCompleted:forceActiveFocus()
 readonly property real side:28
 readonly property color readyColour:Theme.get("color.state.ready_neon")
 readonly property color needsColour:Theme.get("color.state.needs_neon")
 // System tabs, made from the games themselves: a system the Hub learns later gets its own tab with no
 // change here (its label from the names below, else from its pill). Catalog games set up on this PC are
 // sorted in by their hardware. Pinball is a page of its own, at the end.
 readonly property var labels:({all:"All",model3:"Model 3",model2:"Model 2",system22:"System 22",ps1:"PS1",ps2:"PS2",n64:"N64",
  saturn:"Saturn",dreamcast:"Dreamcast",atomiswave:"Atomiswave",wii:"Wii",gamecube:"GameCube",mamegun:"Arcade guns",other:"Other",pinball:"Pinball"})
 readonly property var systemOrder:["ps1","model2","model3","system22","mamegun","n64","saturn","dreamcast","atomiswave","ps2","wii","gamecube"]
 function tabOf(row){
  const system=row.system||"model3"
  if(system!=="catalog")return system
  const card=row.card||({});const hw=String(card.hardwareId||"")
  if(hw==="sony-ps1")return "ps1"
  if(hw==="sony-ps2")return "ps2"
  if(hw.startsWith("sega-model-2"))return "model2"
  if(hw.startsWith("sega-model-3"))return "model3"
  if(hw==="nintendo-64")return "n64"
  if(hw==="sega-saturn")return "saturn"
  if(hw==="sega-dreamcast"||hw.startsWith("sega-naomi"))return "dreamcast"
  if(hw==="sammy-atomiswave")return "atomiswave"
  if(hw==="nintendo-wii")return "wii"
  if(hw==="nintendo-gamecube")return "gamecube"
  if(hw.indexOf("system-22")>=0)return "system22"
  if(card.genreId==="gun")return "mamegun" // The same tab as the arcade gun launcher's games.
  return "other"
 }
 function rank(id){const i=systemOrder.indexOf(id);return i<0?(id==="other"?1000:500):i}
 function labelOf(id,row){if(labels[id])return labels[id];const pill=String((row||{}).pill||id);return pill.charAt(0)+pill.slice(1).toLowerCase()}
 readonly property var tabDefs:{
  const seen={};const list=[]
  for(const row of rows){const id=tabOf(row);if(!seen[id]){seen[id]=true;list.push({id:id,label:labelOf(id,row)})}}
  list.sort((a,b)=>rank(a.id)-rank(b.id)||a.id.localeCompare(b.id))
  return [{id:"all",label:"All"}].concat(list,[{id:"pinball",label:"Pinball"}])
 }
 function ready(row){return !row.problem}
 // The core modules put a game's controls in "warning", except where the scan found a disc that starts
 // behind a note (Wii, Dreamcast): there it is what to read first. A row may also carry "controls" itself.
 function warningIsControls(row){return !!row.system&&row.system!=="catalog"&&["wii","dreamcast"].indexOf(row.system)<0}
 readonly property var counts:{const c={all:rows.length};for(const row of rows){const t=tabOf(row);c[t]=(c[t]||0)+1}return c}
 readonly property var visibleTabs:tabDefs
 readonly property var shown:{
  const q=query.trim().toLowerCase()
  let list=rows.filter(r=>(tab==="all"||tabOf(r)===tab)&&(!readyOnly||ready(r))&&(!q||String(r.title||"").toLowerCase().indexOf(q)>=0))
  const order=r=>rank(tabOf(r))
  if(sortKey==="title")list=list.slice().sort((a,b)=>String(a.title).localeCompare(String(b.title)))
  else if(sortKey==="year")list=list.slice().sort((a,b)=>(a.year||9999)-(b.year||9999)||String(a.title).localeCompare(String(b.title)))
  else list=list.map((r,i)=>({r:r,i:i})).sort((a,b)=>order(a.r)-order(b.r)||a.i-b.i).map(x=>x.r)
  return list
 }
 readonly property bool filtered:tab!=="all"||query.length>0||readyOnly
 readonly property var selected:{for(const row of rows)if(row.id===selectedId)return row;return ({})}
 // B, one level at a time: the filter menu, then the game page, then any tab, search or filter.
 readonly property bool canGoBack:filterMenu.opened||!!selectedId||filtered||searchOpen
 function handleBack(){
  if(filterMenu.opened){filterMenu.close();return true}
  if(selectedId){selectedId="";page.forceActiveFocus();return true}
  if(filtered||searchOpen){tab="all";query="";readyOnly=false;searchOpen=false;currentIndex=0;scroller.scrollToRow(0);page.forceActiveFocus();return true}
  return false
 }
 function openAt(i){const row=shown[i];if(!row||!row.id)return;currentIndex=i;selectedId=row.id}
 function chooseTab(id){if(id==="pinball"){page.pinballRequested();return}tab=id;currentIndex=0;scroller.scrollToRow(0)}
 onTabChanged:Qt.callLater(()=>{for(const child of tabRow.children)if(child.objectName==="panelTab-"+tab){tabStrip.contentX=Math.max(0,Math.min(tabStrip.contentWidth-tabStrip.width,child.x-40));return}})
 function badgeOf(row){return row.problem?"Not ready":row.warning&&!warningIsControls(row)?"Read first":row.accepted?"Accepted in the headset":""}
 function tileOf(row){const art=row.art||(row.system==="catalog"&&row.card?(row.card.artBanner||row.card.artCard||""):"");return {title:row.title,system:row.system||"model3",art:art,badge:badgeOf(row),badgeColour:row.accepted&&!row.problem?readyColour:needsColour}}
 function moveCurrent(delta){
  if(!shown.length)return
  currentIndex=Math.max(0,Math.min(shown.length-1,currentIndex+delta))
  scroller.ensureRowVisible(Math.floor(currentIndex/grid.columns))
 }
 Keys.onLeftPressed:moveCurrent(-1)
 Keys.onRightPressed:moveCurrent(1)
 Keys.onUpPressed:moveCurrent(-grid.columns)
 Keys.onDownPressed:moveCurrent(grid.columns)
 Keys.onReturnPressed:if(!selectedId)openAt(currentIndex)
 Keys.onEnterPressed:if(!selectedId)openAt(currentIndex)
 Item {
  id:library;anchors.fill:parent;visible:!page.selectedId
  // The slim bar: tabs on the left, search, filter and the menu on the right.
  Rectangle {
   id:bar;objectName:"panelTopBar";anchors.left:parent.left;anchors.right:parent.right;height:76;color:Theme.get("color.surface.header")
   Flickable {
    id:tabStrip;objectName:"panelTabs";anchors.left:parent.left;anchors.leftMargin:page.side;anchors.right:tools.left;anchors.rightMargin:12;anchors.verticalCenter:parent.verticalCenter
    height:56;contentWidth:tabRow.implicitWidth;clip:true;flickableDirection:Flickable.HorizontalFlick;interactive:contentWidth>width;boundsBehavior:Flickable.StopAtBounds
    Row {id:tabRow;spacing:8
     Repeater {model:page.visibleTabs
      PillButton {required property var modelData;objectName:"panelTab-"+modelData.id;vrOverlayMode:page.vrOverlayMode
       text:modelData.label+(modelData.id==="pinball"?"  ›":"");Accessible.description:modelData.id==="pinball"?"Opens the pinball tables":(page.counts[modelData.id]||0)+" games"
       neon:page.tab===modelData.id;accent:Theme.get("color.text.primary");onClicked:page.chooseTab(modelData.id)}}
    }
   }
   Row {
    id:tools;anchors.right:parent.right;anchors.rightMargin:page.side;anchors.verticalCenter:parent.verticalCenter;spacing:8
    // Search is a pill until used: the bar keeps its room for the tabs. In the headset, SteamVR's keyboard opens.
    PillButton {objectName:"panelSearchButton";visible:!search.visible;vrOverlayMode:page.vrOverlayMode;text:"Search";onClicked:{page.searchOpen=true;search.forceActiveFocus()}}
    TextField {
     id:search;objectName:"panelSearch";visible:page.searchOpen||page.query.length>0;text:page.query;width:240;height:56;placeholderText:"Search";color:Theme.get("color.text.primary");placeholderTextColor:Theme.get("color.text.faint")
     font.family:Theme.get("font.family");font.pixelSize:18;Accessible.name:"Search games";verticalAlignment:TextInput.AlignVCenter
     onTextEdited:{page.query=text;page.currentIndex=0;scroller.scrollToRow(0)}
     onActiveFocusChanged:if(activeFocus&&page.vrOverlayMode&&typeof overlayHost!=="undefined"&&overlayHost)overlayHost.requestKeyboard(search)
     Keys.onEscapePressed:event=>{page.query="";page.searchOpen=false;page.forceActiveFocus();event.accepted=true}
     background:Rectangle {radius:Theme.get("radius.pill");color:Theme.get("color.surface.pill");border.width:search.activeFocus?2:0;border.color:Theme.get("color.glass.ring")}
    }
    PillButton {objectName:"panelFilter";vrOverlayMode:page.vrOverlayMode;text:"Filter"+(page.readyOnly?" · Ready":"")+" ▾";selected:page.readyOnly||page.sortKey!=="system";onClicked:filterMenu.open()}
    PillButton {objectName:"panelMenu";vrOverlayMode:page.vrOverlayMode;text:"···";Accessible.name:"Help and settings";onClicked:page.helpRequested()}
   }
  }
  GridView {
   id:grid;objectName:"panelGrid"
   anchors.top:bar.bottom;anchors.left:parent.left;anchors.right:parent.right;anchors.bottom:parent.bottom
   readonly property int columns:width>=1100?4:width>=760?3:2
   readonly property real tileWidth:Math.floor((width-2*page.side)/columns)-18
   leftMargin:page.side;rightMargin:page.side;topMargin:18;bottomMargin:24
   cellWidth:(width-2*page.side)/columns;cellHeight:Math.round(tileWidth*9/16)+20
   clip:true;model:page.shown;cacheBuffer:cellHeight*2
   // The stick moves it, a row at a time (the wheel catcher above); dragging the bar moves it too.
   interactive:false;keyNavigationEnabled:false;boundsBehavior:Flickable.StopAtBounds
   ScrollBar.vertical:ScrollBar {objectName:"panelScrollBar";width:18;policy:grid.contentHeight>grid.height?ScrollBar.AlwaysOn:ScrollBar.AlwaysOff}
   delegate:Item {
    id:cell;required property var modelData;required property int index
    width:grid.cellWidth;height:grid.cellHeight
    GameTile {
     objectName:"panelTile";anchors.centerIn:parent;width:grid.tileWidth;height:Math.round(grid.tileWidth*9/16);vrOverlayMode:page.vrOverlayMode
     readonly property var view:page.tileOf(cell.modelData)
     title:view.title||"";system:view.system;art:view.art||"";badge:view.badge;badgeColour:view.badgeColour
     focused:cell.index===page.currentIndex;focusPolicy:Qt.NoFocus
     onHoveredChanged:if(hovered)page.currentIndex=cell.index
     onClicked:page.openAt(cell.index)
    }
   }
  }
  // Catches the stick (wheel packets) over the whole grid; presses and hover pass through to the tiles.
  MouseArea {
   objectName:"panelWheel";anchors.fill:grid;acceptedButtons:Qt.NoButton;z:2
   onWheel:wheel=>{wheel.accepted=true;scroller.wheel(wheel.angleDelta.y,Date.now())}
  }
  PanelScroller {id:scroller;objectName:"panelScroller";flick:grid;rowHeight:grid.cellHeight;reduceMotion:page.reduceMotion}
  Column {
   anchors.centerIn:grid;spacing:16;visible:page.shown.length===0;width:Math.min(parent.width-80,640)
   UiText {objectName:"panelEmpty";width:parent.width;horizontalAlignment:Text.AlignHCenter;font.pixelSize:22;color:Theme.get("color.text.muted")
    text:page.rows.length===0?"No games set up on this PC yet. Add your systems in Set-up.":"No games match. Press B to show every game."}
   PillButton {anchors.horizontalCenter:parent.horizontalCenter;visible:page.rows.length===0;vrOverlayMode:page.vrOverlayMode;neon:true;accent:page.readyColour;text:"Set-up";onClicked:page.setupRequested()}
  }
 }
 Loader {
  id:gamePage;objectName:"panelGameLoader";anchors.fill:parent;active:!!page.selectedId
  sourceComponent:Component {PanelGamePage {
   row:page.selected;vrOverlayMode:page.vrOverlayMode;reduceMotion:page.reduceMotion
   systemLabel:page.selected.system==="catalog"?((page.selected.card||{}).hardwareLabel||"On this PC"):page.labelOf(page.tabOf(page.selected),page.selected)
   warningIsControls:page.warningIsControls(page.selected)
   onBackRequested:page.handleBack()
   onDetailRequested:id=>page.detailRequested(id)
   onSetupRequested:page.setupRequested()}}
 }
 // Filter and sort: a small menu over the grid; B or Done closes it.
 Popup {
  id:filterMenu;objectName:"panelFilterMenu";parent:page;x:page.width-width-page.side;y:84;width:380;padding:16;modal:true;focus:true
  closePolicy:Popup.CloseOnEscape|Popup.CloseOnPressOutside
  background:Rectangle {color:Theme.get("color.surface.panel");radius:Theme.get("radius.hero");border.color:Theme.get("color.line.strong")}
  Column {width:parent.width;spacing:8
   UiText {text:"SHOW";font.pixelSize:15;font.weight:700;color:Theme.get("color.text.muted")}
   PillButton {objectName:"panelFilterAll";width:parent.width;vrOverlayMode:page.vrOverlayMode;text:"Every game";selected:!page.readyOnly;onClicked:page.readyOnly=false}
   PillButton {objectName:"panelFilterReady";width:parent.width;vrOverlayMode:page.vrOverlayMode;text:"Ready to play only";selected:page.readyOnly;onClicked:page.readyOnly=true}
   UiText {text:"SORT";font.pixelSize:15;font.weight:700;color:Theme.get("color.text.muted")}
   Repeater {model:[{id:"system",label:"By system"},{id:"title",label:"Title · A–Z"},{id:"year",label:"Year · oldest first"}]
    PillButton {required property var modelData;width:parent.width;vrOverlayMode:page.vrOverlayMode;text:modelData.label;selected:page.sortKey===modelData.id;onClicked:{page.sortKey=modelData.id;scroller.scrollToRow(0)}}}
   PillButton {objectName:"panelFilterDone";width:parent.width;vrOverlayMode:page.vrOverlayMode;neon:true;accent:Theme.get("color.text.primary");text:"Done";onClicked:filterMenu.close()}
  }
 }
}
