// SPDX-License-Identifier: GPL-3.0-only
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import AladdinsCastle.Hub
Item {
 id:page
 property bool vrOverlayMode:false
 property string sizeValue:"S"
 // Model 3 first, then the games of the other systems; a row with "system" belongs to one of those.
 property var model3Rows:uiController.model3Games
 property var rows:model3Rows.concat(uiController.otherGames,uiController.readyCatalogGames)
 signal detailRequested(string id)
 property var views:selected.views||uiController.model3Views
 property string selectedId:""
 // The same set-up problem on every card is said once, in full, above the cards.
 property string setupProblem:{let first=model3Rows.length?model3Rows[0].problem:"";for(let row of model3Rows)if(row.problem!==first)return "";return first}
 property var selected:{for(let row of rows)if(row.id===selectedId)return row;return ({})}
 property bool idle:Object.keys(uiController.nowPlaying).length===0
 property real factor:Theme.get("size.card.scale."+sizeValue)
 // The owner may have edited the accepted list since the tab was last open.
 Component.onCompleted:{uiSettings.reloadAccepted();page.forceActiveFocus()}
 property color readyColour:Theme.get("color.state.ready_neon")
 property color needsColour:Theme.get("color.state.needs_neon")
 function card(row){
  let c=Object.assign({},row.card||{})
  c.gameId=row.id;c.title=row.title;c.year=row.year
  // A ready catalog game keeps its own catalog card.
  if(row.system==="catalog"){c.blurb="Set up and ready on this PC.";c.stateColour=readyColour;return c}
  c.pill=row.pill||"MODEL 3"
  // Arcade boards have one maker; console games do not, so none is claimed for them.
  c.manufacturerShort=row.system==="system22"?"Namco":(!row.system||row.system==="model2")?"Sega":""
  c.hardwareLabel={dreamcast:"Dreamcast",system22:"System 22",model2:"Model 2",ps2:"PlayStation 2"}[row.system]||"Model 3";c.developerShown=false;c.isWip=false
  let trouble=row.problem||row.warning
  c.blurb=trouble?trouble.split(". ")[0].replace(/\.$/,"")+".":"Found on this PC.";c.stateColour=trouble?needsColour:readyColour
  return c
 }
 function play(row){if(row.system==="catalog")uiController.primary(row.id);else if(row.system)uiController.playOtherGame(row.id);else uiController.playModel3(row.id)}
 function choose(row,view){if(row.system)uiController.chooseGameView(row.id,view);else uiController.chooseModel3View(row.id,view)}
 signal setupRequested()
 // One picture per view, seen from above: you, what you look at, and how much of your view it fills.
 component ViewPicture:Canvas {
  property string kind:"screen"
  property color ink:Theme.get("color.text.body")
  property color accent:page.readyColour
  onKindChanged:requestPaint();onWidthChanged:requestPaint();onHeightChanged:requestPaint();onAccentChanged:requestPaint()
  onPaint:{
   let c=getContext("2d");c.reset();let w=width,h=height,x=w/2,y=h*0.82,far=h*0.2
   let half=kind==="wide"?w*0.36:w*0.22
   c.lineWidth=2
   if(kind==="inside"){
    c.strokeStyle=accent;c.fillStyle=Theme.alpha(accent,0.16);c.beginPath();c.arc(x,h*0.52,Math.min(w,h)*0.44,0,2*Math.PI);c.fill();c.stroke()
    y=h*0.52;c.lineWidth=4;c.strokeStyle=ink;c.beginPath();c.moveTo(x-w*0.07,y-h*0.2);c.lineTo(x+w*0.07,y-h*0.2);c.stroke()
   }else{
    c.fillStyle=Theme.alpha(accent,0.16);c.beginPath();c.moveTo(x,y);c.lineTo(x-half,far);c.lineTo(x+half,far);c.closePath();c.fill()
    c.lineWidth=5;c.strokeStyle=accent;c.beginPath();c.moveTo(x-half,far);c.lineTo(x+half,far);c.stroke()
   }
   c.fillStyle=ink;c.beginPath();c.arc(x,y,6,0,2*Math.PI);c.fill()
  }
 }
 // Direction C: the focused game as a hero across the top, then one row of tiles per system.
 // Left and right move along a row, up and down between rows; laser hover moves the focus too.
 readonly property var shelfOrder:["model3","model2","system22","dreamcast","ps2","catalog"]
 readonly property var shelfNames:({model3:"Sega Model 3",model2:"Sega Model 2",system22:"Namco System 22",dreamcast:"Sega Dreamcast",ps2:"PlayStation 2",catalog:"Set up on this PC"})
 property var shelves:{let list=[];for(let key of shelfOrder){let items=rows.filter(r=>(r.system||"model3")===key);if(items.length)list.push({key:key,heading:shelfNames[key]+"  ·  "+items.length,items:items})}return list}
 property int shelfIndex:0
 property int tileIndex:0
 readonly property var focusedRow:{const shelf=shelves[Math.min(shelfIndex,shelves.length-1)];return shelf?shelf.items[Math.min(tileIndex,shelf.items.length-1)]||({}):({})}
 function hardwareLabel(row){return {dreamcast:"Sega Dreamcast",system22:"Namco System 22",model2:"Sega Model 2",ps2:"PlayStation 2",catalog:"On this PC"}[row.system]||"Sega Model 3"}
 function badgeOf(row){return row.problem?"Not ready":row.warning?"Read first":row.accepted?"Accepted in the headset":""}
 function tileOf(row){const art=row.system==="catalog"&&row.card?(row.card.artBanner||row.card.artCard||""):"";return {title:row.title,system:row.system||"model3",art:art,badge:badgeOf(row),badgeColour:row.accepted&&!row.problem&&!row.warning?readyColour:needsColour}}
 function moveFocus(dShelf,dTile){
  if(!shelves.length)return
  shelfIndex=Math.max(0,Math.min(shelves.length-1,shelfIndex+dShelf))
  tileIndex=Math.max(0,Math.min(shelves[shelfIndex].items.length-1,tileIndex+dTile))
  shelfList.positionViewAtIndex(shelfIndex,ListView.Contain)
 }
 function openRow(row){if(!row||!row.id)return;if(row.system==="catalog")page.detailRequested(row.id);else page.selectedId=row.id}
 // A game that is not ready, or starts only behind a warning, opens its page instead of starting.
 function primaryAction(row){if(!row||!row.id)return;if(row.problem||row.warning||!page.idle||row.system==="catalog")openRow(row);else page.play(row)}
 focus:!selectedId
 Keys.onLeftPressed:moveFocus(0,-1)
 Keys.onRightPressed:moveFocus(0,1)
 Keys.onUpPressed:moveFocus(-1,0)
 Keys.onDownPressed:moveFocus(1,0)
 Keys.onReturnPressed:openRow(focusedRow)
 Keys.onEnterPressed:openRow(focusedRow)
 Keys.onSpacePressed:primaryAction(focusedRow)
 ColumnLayout {anchors.fill:parent;spacing:0;visible:!page.selectedId
  HeroBanner {id:hero;objectName:"model3Hero";Layout.fillWidth:true;Layout.preferredHeight:page.height*(page.vrOverlayMode?0.42:0.46);vrOverlayMode:page.vrOverlayMode
   system:page.focusedRow.system||"model3";art:page.tileOf(page.focusedRow).art
   eyebrow:(page.hardwareLabel(page.focusedRow)+(page.focusedRow.year?"  ·  "+page.focusedRow.year:"")).toUpperCase()
   title:page.focusedRow.title||"VR games"
   meta:page.setupProblem||page.focusedRow.problem||page.focusedRow.warning||(page.focusedRow.system==="catalog"?"Set up and ready on this PC.":page.focusedRow.title?"Found on this PC. Choose how you see it, then start.":"Add your systems in Set-up.")
   metaColour:page.setupProblem||page.focusedRow.problem||page.focusedRow.warning?page.needsColour:Theme.get("color.text.muted")
   PillButton {objectName:"model3HeroStart";visible:!!page.focusedRow.id;targetHeight:page.vrOverlayMode?56:48;vrOverlayMode:page.vrOverlayMode;neon:true;accent:page.focusedRow.problem||page.focusedRow.warning?page.needsColour:page.readyColour
    text:page.focusedRow.problem?"Not ready":page.focusedRow.warning?"Read first":page.focusedRow.system==="catalog"?"Start":"Start · "+(page.focusedRow.viewLabel||"");onClicked:page.primaryAction(page.focusedRow)}
   PillButton {objectName:"model3HeroDetails";visible:!!page.focusedRow.id;targetHeight:page.vrOverlayMode?56:48;vrOverlayMode:page.vrOverlayMode;text:"Details";onClicked:page.openRow(page.focusedRow)}
   extra:[
    PillButton {objectName:"model3SetupButton";text:"Set-up";vrOverlayMode:page.vrOverlayMode;onClicked:page.setupRequested()},
    PillButton {text:"Check again";vrOverlayMode:page.vrOverlayMode;onClicked:{uiSettings.reloadAccepted();uiController.refreshModel3()}}]
  }
  UiText {objectName:"model3Setup";visible:false;text:page.setupProblem}
  ListView {id:shelfList;objectName:"model3Grid";property int count:page.rows.length;Layout.fillWidth:true;Layout.fillHeight:true;clip:true;spacing:page.vrOverlayMode?18:24;model:page.shelves
   boundsBehavior:Flickable.StopAtBounds;topMargin:4;bottomMargin:24
   delegate:ShelfRow {required property var modelData;required property int index;width:shelfList.width;vrOverlayMode:page.vrOverlayMode;tileName:"model3Card"
    heading:modelData.heading;items:modelData.items;tileOf:page.tileOf;focusIndex:index===page.shelfIndex?page.tileIndex:-1
    onHovered:i=>{page.shelfIndex=index;page.tileIndex=i}
    onActivated:i=>{page.shelfIndex=index;page.tileIndex=i;page.openRow(modelData.items[i])}}
  }
 }
 Loader {anchors.fill:parent;active:!!page.selectedId;sourceComponent:Component{ColumnLayout {spacing:12
  RowLayout {spacing:12
   PillButton {objectName:"model3Back";text:"Back to games";vrOverlayMode:page.vrOverlayMode;onClicked:page.selectedId=""}
   UiText {objectName:"model3Title";text:page.selected.title||"";font.pixelSize:24;Layout.fillWidth:true}
  }
  UiText {Layout.fillWidth:true;text:"Choose how you see it. The choice is remembered for this game."}
  RowLayout {Layout.fillWidth:true;Layout.fillHeight:true;spacing:12
   Repeater {model:page.views
    Button {id:option;objectName:"model3View";required property var modelData;property bool chosen:page.selected.view===modelData.id
     Layout.fillWidth:true;Layout.fillHeight:true;Layout.preferredWidth:1;Layout.maximumHeight:340;padding:14
     Accessible.name:modelData.label;Accessible.checked:chosen
     onClicked:page.choose(page.selected,modelData.id)
     background:Rectangle {radius:Theme.get("size.card.radius");color:Theme.mix(Theme.get("color.surface.card"),page.readyColour,option.chosen?0.12:0.03);border.width:option.chosen||option.activeFocus?2:1;border.color:option.chosen?page.readyColour:option.hovered?Theme.get("color.line.button"):Theme.get("color.line.strong")}
     contentItem:ColumnLayout {spacing:8
      ViewPicture {Layout.fillWidth:true;Layout.fillHeight:true;Layout.minimumHeight:70;kind:option.modelData.id==="headset"||option.modelData.id==="vr"?"inside":option.modelData.id==="monitor"?"screen":option.modelData.id;accent:option.chosen?page.readyColour:Theme.get("color.text.muted_detail")}
      UiText {Layout.fillWidth:true;text:(option.chosen?"● ":"○ ")+option.modelData.label;font.pixelSize:page.vrOverlayMode?22:18;color:option.chosen?page.readyColour:Theme.get("color.text.primary")}
      UiText {Layout.fillWidth:true;text:option.modelData.help;font.pixelSize:page.vrOverlayMode?16:13}
     }
    }
   }
  }
  // The four facts kept apart: listed, how to start it, found here, accepted by the owner.
  UiText {objectName:"model3Status";Layout.fillWidth:true;font.pixelSize:14;color:Theme.get("color.text.muted_detail")
   text:"In the Hub's list: yes.   How to start it: known.   On this PC: "+(page.selected.found?"found":"not ready")+".   In the headset: "+(page.selected.accepted?"accepted by you ("+page.selected.accepted+")":"not yet accepted by you")+"."}
  UiText {objectName:"model3Problem";Layout.fillWidth:true;visible:!!page.selected.problem;text:page.selected.problem||"";font.pixelSize:16;color:page.needsColour}
  UiText {objectName:"model3Warning";Layout.fillWidth:true;visible:!!page.selected.warning;text:"Before you start: "+(page.selected.warning||"");font.pixelSize:16;color:page.needsColour}
  RowLayout {spacing:12
   PillButton {objectName:"model3Start";text:"Start "+(page.selected.title||"")+" · "+(page.selected.viewLabel||"");neon:true;accent:page.selected.problem?page.needsColour:page.readyColour;vrOverlayMode:page.vrOverlayMode;enabled:!page.selected.problem&&page.idle;onClicked:page.play(page.selected)}
   UiText {Layout.fillWidth:true;visible:!page.selected.system;text:"In the game: Home = set straight ahead, 1 = start, 3 = coin, Esc = quit."}
  }
 }}}
}
