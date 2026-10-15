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
 Component.onCompleted:uiSettings.reloadAccepted()
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
 ColumnLayout {anchors.fill:parent;spacing:8;visible:!page.selectedId
  RowLayout {spacing:8
   UiText {text:"VR games";font.pixelSize:24;Layout.fillWidth:true}
   PillButton {objectName:"model3SetupButton";text:"Set-up";vrOverlayMode:page.vrOverlayMode;onClicked:page.setupRequested()}
   PillButton {text:"Check again";vrOverlayMode:page.vrOverlayMode;onClicked:{uiSettings.reloadAccepted();uiController.refreshModel3()}}
  }
  UiText {Layout.fillWidth:true;text:page.vrOverlayMode?"Pick a game to choose how you see it.":"Your own emulator builds and your own game files, in the headset. Start SteamVR with the headset connected first, then pick a game to choose how you see it. Systems are added in Set-up."}
  UiText {objectName:"model3Setup";Layout.fillWidth:true;visible:text.length>0;text:page.setupProblem;font.pixelSize:16;color:page.needsColour}
  GridView {id:grid;objectName:"model3Grid";Layout.fillWidth:true;Layout.fillHeight:true;clip:true;model:page.rows;boundsBehavior:Flickable.StopAtBounds
   cellWidth:Math.floor(width/Math.max(1,Math.floor(width/(Theme.get("size.card.base_w")*page.factor+8))))
   cellHeight:page.vrOverlayMode?Math.max(184,Math.floor(height/Math.max(1,Math.floor(height/192)))):Theme.get("size.card.base_h")*page.factor+8
   ScrollBar.vertical:ScrollBar{}
   delegate:Item {id:cell;required property var modelData;width:grid.cellWidth;height:grid.cellHeight
    GameCard {objectName:"model3Card";width:parent.width-8;height:parent.height-8;game:page.card(cell.modelData);sizeValue:page.sizeValue;vrOverlayMode:page.vrOverlayMode
     effectsEnabled:!(uiSettings.values,uiSettings.get("cheapEffects",true))
     primaryText:cell.modelData.problem?"Not ready":cell.modelData.warning?"Read first":cell.modelData.system==="catalog"?"Start":"Start · "+cell.modelData.viewLabel
     // Acceptance is the owner's word only; finding a game is not the same thing.
     footText:cell.modelData.accepted?"✓ Accepted in the headset":" "
     onDetailRequested:if(cell.modelData.system==="catalog")page.detailRequested(cell.modelData.id);else page.selectedId=cell.modelData.id
     // A game that is not ready, or starts only behind a warning, opens its page instead of starting.
     onActionRequested:if(cell.modelData.problem||cell.modelData.warning||!page.idle)page.selectedId=cell.modelData.id;else page.play(cell.modelData)
    }
   }
  }
  RowLayout {spacing:8;visible:page.vrOverlayMode
   property real step:Math.max(1,Math.floor(grid.height/grid.cellHeight))*grid.cellHeight
   PillButton {objectName:"model3PreviousPage";text:"Previous page";vrOverlayMode:true;enabled:grid.contentY>0;onClicked:grid.contentY=Math.max(0,grid.contentY-parent.step)}
   PillButton {objectName:"model3NextPage";text:"Next page";vrOverlayMode:true;enabled:grid.contentY+grid.height<grid.contentHeight-1;onClicked:grid.contentY=Math.min(Math.max(0,grid.contentHeight-grid.height),grid.contentY+parent.step)}
   UiText {text:grid.count?"Page "+(Math.round(grid.contentY/Math.max(1,parent.step))+1)+" of "+Math.max(1,Math.ceil(grid.contentHeight/Math.max(1,parent.step))):""}
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
