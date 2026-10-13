// SPDX-License-Identifier: GPL-3.0-only
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Dialogs
import AladdinsCastle.Hub
Item {
 id:page
 property bool vrOverlayMode:false
 property string sizeValue:"S"
 property var rows:uiController.model3Games
 property var views:uiController.model3Views
 property string selectedId:""
 // The same set-up problem on every card is said once, in full, above the cards.
 property string setupProblem:{let first=rows.length?rows[0].problem:"";for(let row of rows)if(row.problem!==first)return "";return first}
 property var selected:{for(let row of rows)if(row.id===selectedId)return row;return ({})}
 property bool idle:Object.keys(uiController.nowPlaying).length===0
 property real factor:Theme.get("size.card.scale."+sizeValue)
 property color readyColour:Theme.get("color.state.ready_neon")
 property color needsColour:Theme.get("color.state.needs_neon")
 function card(row){
  let c=Object.assign({},row.card||{})
  c.gameId=row.id;c.title=row.title;c.year=row.year;c.pill="MODEL 3";c.manufacturerShort="Sega";c.hardwareLabel="Model 3";c.developerShown=false;c.isWip=false
  c.blurb=row.problem?row.problem.split(". ")[0].replace(/\.$/,"")+".":"Your own file, real 3D in VR.";c.stateColour=row.problem?needsColour:readyColour
  return c
 }
 FileDialog {id:file;title:"Choose your Model 3 emulator program";nameFilters:["Program (*.exe)","All files (*)"];onAccepted:uiSettings.set("model3Executable",uiController.localPath(selectedFile))}
 FolderDialog {id:folder;title:"Choose the folder holding your own Model 3 game files";onAccepted:uiSettings.set("model3GameFolder",uiController.localPath(selectedFolder))}
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
   UiText {text:"Sega Model 3";font.pixelSize:24;Layout.fillWidth:true}
   PillButton {objectName:"model3Locate";text:"Locate emulator";visible:!page.vrOverlayMode;onClicked:file.open()}
   PillButton {objectName:"model3Folder";text:"Game folder";visible:!page.vrOverlayMode;onClicked:folder.open()}
   PillButton {text:"Check again";vrOverlayMode:page.vrOverlayMode;onClicked:uiController.refreshModel3()}
  }
  UiText {Layout.fillWidth:true;text:"Your own emulator build and your own game files, drawn in real 3D for the headset. Start SteamVR with the headset connected first, then pick a game to choose how you see it."+(page.vrOverlayMode?" The emulator and the game folder are chosen on the desktop.":"")}
  UiText {objectName:"model3Setup";Layout.fillWidth:true;visible:text.length>0;text:page.setupProblem;font.pixelSize:16;color:page.needsColour}
  GridView {id:grid;objectName:"model3Grid";Layout.fillWidth:true;Layout.fillHeight:true;clip:true;model:page.rows;boundsBehavior:Flickable.StopAtBounds
   cellWidth:Math.floor(width/Math.max(1,Math.floor(width/(Theme.get("size.card.base_w")*page.factor+8))))
   cellHeight:Math.max(Theme.get("size.card.base_h")*page.factor,page.vrOverlayMode?248:0)+8
   ScrollBar.vertical:ScrollBar{}
   delegate:Item {id:cell;required property var modelData;width:grid.cellWidth;height:grid.cellHeight
    GameCard {objectName:"model3Card";width:parent.width-8;height:parent.height-8;game:page.card(cell.modelData);sizeValue:page.sizeValue;vrOverlayMode:page.vrOverlayMode
     effectsEnabled:!(uiSettings.values,uiSettings.get("cheapEffects",true))
     primaryText:cell.modelData.problem?"Not ready":"Start · "+cell.modelData.viewLabel
     footText:"TRUE 3D · 3 views"
     onDetailRequested:page.selectedId=cell.modelData.id
     onActionRequested:if(cell.modelData.problem||!page.idle)page.selectedId=cell.modelData.id;else uiController.playModel3(cell.modelData.id)
    }
   }
  }
  RowLayout {spacing:8;visible:page.vrOverlayMode
   PillButton {text:"Previous page";vrOverlayMode:true;onClicked:grid.contentY=Math.max(0,grid.contentY-grid.height)}
   PillButton {text:"Next page";vrOverlayMode:true;onClicked:grid.contentY=Math.min(Math.max(0,grid.contentHeight-grid.height),grid.contentY+grid.height)}
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
     onClicked:uiController.chooseModel3View(page.selectedId,modelData.id)
     background:Rectangle {radius:Theme.get("size.card.radius");color:Theme.mix(Theme.get("color.surface.card"),page.readyColour,option.chosen?0.12:0.03);border.width:option.chosen||option.activeFocus?2:1;border.color:option.chosen?page.readyColour:option.hovered?Theme.get("color.line.button"):Theme.get("color.line.strong")}
     contentItem:ColumnLayout {spacing:8
      ViewPicture {Layout.fillWidth:true;Layout.fillHeight:true;Layout.minimumHeight:70;kind:option.modelData.id;accent:option.chosen?page.readyColour:Theme.get("color.text.muted_detail")}
      UiText {Layout.fillWidth:true;text:(option.chosen?"● ":"○ ")+option.modelData.label;font.pixelSize:page.vrOverlayMode?22:18;color:option.chosen?page.readyColour:Theme.get("color.text.primary")}
      UiText {Layout.fillWidth:true;text:option.modelData.help;font.pixelSize:page.vrOverlayMode?16:13}
     }
    }
   }
  }
  UiText {objectName:"model3Problem";Layout.fillWidth:true;visible:!!page.selected.problem;text:page.selected.problem||"";font.pixelSize:16;color:page.needsColour}
  RowLayout {spacing:12
   PillButton {objectName:"model3Start";text:"Start "+(page.selected.title||"")+" · "+(page.selected.viewLabel||"");neon:true;accent:page.selected.problem?page.needsColour:page.readyColour;vrOverlayMode:page.vrOverlayMode;enabled:!page.selected.problem&&page.idle;onClicked:uiController.playModel3(page.selectedId)}
   UiText {Layout.fillWidth:true;text:"In the game: Home = set straight ahead, 1 = start, 3 = coin, Esc = quit."}
  }
 }}}
}
