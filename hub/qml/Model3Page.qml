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
  c.hardwareLabel={wii:"Wii",dreamcast:"Dreamcast",system22:"System 22",model2:"Model 2",ps2:"PlayStation 2",ps1:"PlayStation",n64:"Nintendo 64",saturn:"Saturn",mamegun:"Arcade (MAME)",atomiswave:"Atomiswave",gamecube:"GameCube"}[row.system]||"Model 3";c.developerShown=false;c.isWip=false
  let trouble=row.problem||row.warning
  c.blurb=trouble?trouble.split(". ")[0].replace(/\.$/,"")+".":"Found on this PC.";c.stateColour=trouble?needsColour:readyColour
  return c
 }
 // Every game says where it plays: in the headset (Model 3's screen, wide screen or inside the game are
 // choices under that), and on the monitor where the system has a monitor mode.
 function headsetViews(row){return (row.views||uiController.model3Views).filter(v=>v.id!=="monitor")}
 function hasMonitor(row){return (row.views||[]).some(v=>v.id==="monitor")}
 function headsetView(row){const list=headsetViews(row);for(const v of list)if(v.id===row.view)return v;return list.length?list[0]:({})}
 function headsetLabel(row){const list=headsetViews(row);return "Play in headset"+(list.length>1?" · "+headsetView(row).label:"")}
 // Which view Play uses, and whether it is the user's choice or the Hub's default, said plainly.
 function viewNote(row,where){
  const list=headsetViews(row);if(list.length<2)return ""
  const others=list.filter(v=>v.id!==headsetView(row).id).map(v=>v.label).join(" or ")
  return "Plays as "+headsetView(row).label+(row.viewChosen?", your choice.":", the default.")+" "+others+" can be chosen "+where+"; the choice is kept for this game."
 }
 function playIn(row,where){if(!row||!row.id)return;uiController.playGame(row.id,where==="monitor"?"monitor":headsetView(row).id)}
 function play(row){if(row.system==="catalog")uiController.primary(row.id);else playIn(row,"headset")}
 readonly property bool canStart:idle&&!uiController.steamVrStarting
 readonly property string headsetNote:uiController.steamVrRunning?"":"SteamVR is off: Play in headset starts it first, then the game."
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
 readonly property var shelfOrder:["model3","model2","system22","mamegun","wii","gamecube","dreamcast","atomiswave","ps2","ps1","n64","saturn","catalog"]
 readonly property var shelfNames:({model3:"Sega Model 3",model2:"Sega Model 2",system22:"Namco System 22",mamegun:"Arcade guns and motion (MAME)",wii:"Nintendo Wii",gamecube:"Nintendo GameCube",dreamcast:"Sega Dreamcast",atomiswave:"Sammy Atomiswave",ps2:"PlayStation 2",ps1:"PlayStation",n64:"Nintendo 64",saturn:"Sega Saturn",catalog:"Set up on this PC"})
 property var shelves:{let list=[];for(let key of shelfOrder){let items=rows.filter(r=>(r.system||"model3")===key);if(items.length)list.push({key:key,heading:shelfNames[key]+"  ·  "+items.length,items:items})}return list}
 property int shelfIndex:0
 property int tileIndex:0
 readonly property var focusedRow:{const shelf=shelves[Math.min(shelfIndex,shelves.length-1)];return shelf?shelf.items[Math.min(tileIndex,shelf.items.length-1)]||({}):({})}
 function hardwareLabel(row){return {wii:"Nintendo Wii",dreamcast:"Sega Dreamcast",system22:"Namco System 22",model2:"Sega Model 2",ps2:"PlayStation 2",ps1:"PlayStation",n64:"Nintendo 64",saturn:"Sega Saturn",catalog:"On this PC"}[row.system]||"Sega Model 3"}
 function badgeOf(row){return row.problem?"Not ready":row.warning?"Read first":row.accepted?"Accepted in the headset":""}
 function tileOf(row){const art=row.art||(row.system==="catalog"&&row.card?(row.card.artBanner||row.card.artCard||""):"");return {title:row.title,system:row.system||"model3",art:art,badge:badgeOf(row),badgeColour:row.accepted&&!row.problem&&!row.warning?readyColour:needsColour}}
 function moveFocus(dShelf,dTile){
  if(!shelves.length)return
  shelfIndex=Math.max(0,Math.min(shelves.length-1,shelfIndex+dShelf))
  tileIndex=Math.max(0,Math.min(shelves[shelfIndex].items.length-1,tileIndex+dTile))
  shelfList.positionViewAtIndex(shelfIndex,ListView.Contain)
 }
 function openRow(row){if(!row||!row.id)return;if(row.system==="catalog")page.detailRequested(row.id);else page.selectedId=row.id}
 // A game that is not ready, or starts only behind a warning, opens its page instead of starting.
 function primaryAction(row){if(!row||!row.id)return;if(row.problem||row.warning||!page.canStart||row.system==="catalog")openRow(row);else page.play(row)}
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
   system:page.focusedRow.system||"model3";art:page.focusedRow.heroArt||page.tileOf(page.focusedRow).art;artBlurred:!!page.focusedRow.heroBlur
   eyebrow:(page.hardwareLabel(page.focusedRow)+(page.focusedRow.year?"  ·  "+page.focusedRow.year:"")).toUpperCase()
   title:page.focusedRow.title||"VR games"
   meta:page.setupProblem||page.focusedRow.problem||page.focusedRow.warning||(page.focusedRow.system==="catalog"?"Set up and ready on this PC.":page.focusedRow.title?(page.viewNote(page.focusedRow,"in Details")||"Found on this PC. Choose how you see it, then start."):"Add your systems in Set-up.")
   metaColour:page.setupProblem||page.focusedRow.problem||page.focusedRow.warning?page.needsColour:Theme.get("color.text.muted")
   PillButton {objectName:"model3HeroStart";visible:!!page.focusedRow.id;targetHeight:page.vrOverlayMode?56:48;vrOverlayMode:page.vrOverlayMode;neon:true;accent:page.focusedRow.problem||page.focusedRow.warning?page.needsColour:page.readyColour
    text:page.focusedRow.problem?"Not ready":page.focusedRow.warning?"Read first":page.focusedRow.system==="catalog"?"Start":page.headsetLabel(page.focusedRow);onClicked:page.primaryAction(page.focusedRow)}
   PillButton {objectName:"model3HeroMonitor";visible:!!page.focusedRow.id&&page.hasMonitor(page.focusedRow)&&!page.focusedRow.problem&&!page.focusedRow.warning;targetHeight:page.vrOverlayMode?56:48;vrOverlayMode:page.vrOverlayMode
    text:"Play on monitor";enabled:page.canStart;onClicked:page.playIn(page.focusedRow,"monitor")}
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
 // A game's own page: the hero with Start and Back, then how to see it, then what it needs.
 Loader {anchors.fill:parent;active:!!page.selectedId;sourceComponent:Component{ColumnLayout {spacing:0
  HeroBanner {Layout.fillWidth:true;Layout.preferredHeight:page.height*(page.vrOverlayMode?0.4:0.44);vrOverlayMode:page.vrOverlayMode
   system:page.selected.system||"model3";art:page.selected.heroArt||"";artBlurred:!!page.selected.heroBlur
   eyebrow:(page.hardwareLabel(page.selected)+(page.selected.year?"  ·  "+page.selected.year:"")).toUpperCase()
   title:page.selected.title||""
   meta:page.selected.problem||(page.selected.warning?"Before you start: "+page.selected.warning:(page.viewNote(page.selected,"below")||"Found on this PC."))
   metaColour:page.selected.problem||page.selected.warning?page.needsColour:Theme.get("color.text.muted")
   PillButton {objectName:"model3Start";targetHeight:page.vrOverlayMode?56:48;text:page.headsetLabel(page.selected);neon:true;accent:page.selected.problem?page.needsColour:page.readyColour;vrOverlayMode:page.vrOverlayMode;enabled:!page.selected.problem&&page.canStart;onClicked:page.playIn(page.selected,"headset")}
   PillButton {objectName:"model3PlayMonitor";visible:page.hasMonitor(page.selected);targetHeight:page.vrOverlayMode?56:48;text:"Play on monitor";vrOverlayMode:page.vrOverlayMode;enabled:!page.selected.problem&&page.canStart;onClicked:page.playIn(page.selected,"monitor")}
   PillButton {objectName:"model3Back";targetHeight:page.vrOverlayMode?56:48;text:"‹ Back";vrOverlayMode:page.vrOverlayMode;onClicked:page.selectedId=""}
   // Only with the artwork setting on; otherwise it says where to turn it on, and asks nothing.
   extra:[PillButton {objectName:"model3GetArt";text:uiController.artworkBusy?"Getting artwork…":"Get artwork";vrOverlayMode:page.vrOverlayMode;enabled:!uiController.artworkBusy
    onClicked:uiController.getArtwork(page.selected.artId||page.selected.id)}]
  }
  Text {objectName:"model3Title";visible:false;text:page.selected.title||""}
  // Inside the game, the cabinet's own wheel or handlebars can be drawn in front of you.
  SettingRow {objectName:"model3CabinetControl";Layout.fillWidth:true;Layout.leftMargin:page.vrOverlayMode?28:40;Layout.rightMargin:page.vrOverlayMode?28:40;Layout.topMargin:8;vrOverlayMode:page.vrOverlayMode
   visible:!page.selected.system&&!!page.selected.cabinetControl&&page.selected.view==="inside"
   label:"Show the cabinet's "+(page.selected.cabinetControl||"wheel")+" in front of you";on:!!page.selected.showControl
   detail:"Inside the game only. It turns with the steering the game receives, from the controllers, a gamepad or the keyboard."
   onClicked:uiController.setModel3Control(page.selected.id,!on)}
  // Several copies of this game in the library: which one is used, and the rule that chose it.
  ColumnLayout {id:copiesBox;objectName:"gameCopies";Layout.fillWidth:true;Layout.leftMargin:page.vrOverlayMode?28:40;Layout.rightMargin:page.vrOverlayMode?28:40;spacing:6
   readonly property string system:page.selected.system||"model3"
   readonly property string game:page.selected.system?String(page.selected.id||"").substring(page.selected.system.length+1):(page.selected.id||"")
   readonly property var copies:(uiController.library,uiSettings.values,uiController.copiesOf(system,game))
   readonly property string inUse:(uiController.library,uiSettings.values,uiController.copyInUse(system,game))
   visible:copies.length>1
   Text {text:"Copy used ("+copiesBox.copies.length+" found)";color:Theme.get("color.text.primary");font.family:Theme.get("font.family");font.weight:700;font.pixelSize:page.vrOverlayMode?20:17}
   UiText {Layout.fillWidth:true;wrapMode:Text.Wrap;text:"Chosen by this rule unless you pick one: "+uiController.libraryRule();font.pixelSize:page.vrOverlayMode?15:13;color:Theme.get("color.text.muted")}
   Flow {Layout.fillWidth:true;spacing:8
    Repeater {model:copiesBox.copies
     PillButton {required property var modelData;objectName:"gameCopy";vrOverlayMode:page.vrOverlayMode
      readonly property string file:modelData.file||""
      neon:file===copiesBox.inUse;accent:Theme.get("color.text.primary")
      text:file.split("/").pop()+(modelData.region?"  ·  "+modelData.region:"")+(modelData.identity?"  ·  "+modelData.identity:"")
      onClicked:uiController.chooseCopy(copiesBox.system,copiesBox.game,file)}}}
  }
  UiText {objectName:"model3HeadsetNote";Layout.leftMargin:page.vrOverlayMode?28:40;Layout.bottomMargin:10;visible:text.length>0;text:page.headsetNote;font.pixelSize:page.vrOverlayMode?16:14;color:Theme.get("color.text.muted")}
  Text {Layout.leftMargin:page.vrOverlayMode?28:40;Layout.topMargin:4;text:"How you see it in the headset";color:Theme.get("color.text.primary");font.family:Theme.get("font.family");font.weight:700;font.pixelSize:page.vrOverlayMode?22:19}
  RowLayout {Layout.fillWidth:true;Layout.leftMargin:page.vrOverlayMode?28:40;Layout.rightMargin:page.vrOverlayMode?28:40;Layout.topMargin:12;spacing:16
   Repeater {model:page.headsetViews(page.selected)
    Button {id:option;objectName:"model3View";required property var modelData;property bool chosen:page.selected.view===modelData.id
     Layout.fillWidth:true;Layout.preferredWidth:1;Layout.preferredHeight:page.vrOverlayMode?186:140;padding:16;hoverEnabled:true
     Accessible.name:modelData.label;Accessible.checked:chosen
     onClicked:page.choose(page.selected,modelData.id)
     background:Rectangle {radius:Theme.get("radius.card");color:option.chosen?Theme.get("color.surface.pill"):Theme.get("color.surface.card");border.width:option.chosen||option.activeFocus||option.hovered?3:0;border.color:Theme.get("color.glass.ring")}
     contentItem:RowLayout {spacing:14
      ViewPicture {Layout.preferredWidth:page.vrOverlayMode?90:80;Layout.fillHeight:true;kind:option.modelData.id==="headset"||option.modelData.id==="vr"||option.modelData.id.startsWith("inside")?"inside":option.modelData.id==="wide"||option.modelData.id==="open-frame"?"wide":"screen";accent:option.chosen?page.readyColour:Theme.get("color.text.muted_detail")}
      ColumnLayout {Layout.fillWidth:true;spacing:6
       Text {Layout.fillWidth:true;text:option.modelData.label;color:Theme.get("color.text.primary");font.family:Theme.get("font.family");font.weight:700;font.pixelSize:page.vrOverlayMode?21:18}
       // The view Play uses, marked on its card: the user's choice, or the default until one is made.
       Text {objectName:"model3ViewInUse";Layout.fillWidth:true;visible:option.chosen&&page.headsetViews(page.selected).length>1;text:page.selected.viewChosen?"Used by Play: your choice":"Used by Play: the default"
        color:page.readyColour;font.family:Theme.get("font.family");font.weight:700;font.pixelSize:page.vrOverlayMode?15:13}
       Text {Layout.fillWidth:true;text:option.modelData.help;wrapMode:Text.Wrap;maximumLineCount:6;elide:Text.ElideRight;color:Theme.get("color.text.muted");font.family:Theme.get("font.family");font.weight:500;font.pixelSize:page.vrOverlayMode?16:14}
       Item {Layout.fillHeight:true}
      }
     }
    }
   }
  }
  ColumnLayout {Layout.fillWidth:true;Layout.leftMargin:page.vrOverlayMode?28:40;Layout.rightMargin:page.vrOverlayMode?28:40;Layout.topMargin:18;spacing:8
   UiText {objectName:"model3Problem";Layout.fillWidth:true;visible:!!page.selected.problem;text:page.selected.problem||"";font.pixelSize:page.vrOverlayMode?18:16;color:page.needsColour}
   UiText {objectName:"model3Warning";Layout.fillWidth:true;visible:!!page.selected.warning;text:"Before you start: "+(page.selected.warning||"");font.pixelSize:page.vrOverlayMode?18:16;color:page.needsColour}
   UiText {Layout.fillWidth:true;visible:!page.selected.system;text:"In the game: Home = set straight ahead · 1 = start · 3 = coin · Esc = quit";font.pixelSize:page.vrOverlayMode?18:16;color:Theme.get("color.text.body")}
   // The four facts kept apart: listed, how to start it, found here, accepted by the owner.
   UiText {objectName:"model3Status";Layout.fillWidth:true;font.pixelSize:page.vrOverlayMode?16:14;color:Theme.get("color.text.muted_detail")
    text:"In the Hub's list: yes   ·   How to start it: known   ·   On this PC: "+(page.selected.found?"found":"not ready")+"   ·   In the headset: "+(page.selected.accepted?"accepted by you ("+page.selected.accepted+")":"not yet accepted by you")}
  }
  Item {Layout.fillHeight:true}
 }}}
}
