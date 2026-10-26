// SPDX-License-Identifier: GPL-3.0-only
import QtQuick
import QtQuick.Controls
import QtQuick.Dialogs
import QtQuick.Layouts
import AladdinsCastle.Hub
// The Game library in Set-up: big folders of your own games, subfolders and all. The Hub looks through
// them for every game it plays and never changes anything in them; a folder chosen for one system
// below still comes first.
Rectangle {
 id:card
 property bool vrOverlayMode:false
 // Replaced only when the folders change: any other setting leaves the list, and its rows, alone.
 property var roots:[]
 function keepRoots(){const next=Array.from(uiSettings.get("libraryRoots",[])||[]);if(JSON.stringify(next)!==JSON.stringify(roots))roots=next}
 Connections {target:uiSettings;function onChanged(){card.keepRoots()}}
 Component.onCompleted:keepRoots()
 readonly property var result:uiController.library||({})
 readonly property var names:({model3:"Sega Model 3",model2:"Sega Model 2",ps1:"PlayStation",ps2:"PlayStation 2",dreamcast:"Sega Dreamcast",wii:"Nintendo Wii",gamecube:"Nintendo GameCube"})
 readonly property string summary:{
  const systems=result.systems||{};let parts=[]
  for(const key of ["model3","model2","wii","gamecube","dreamcast","ps2","ps1"]){const s=systems[key];if(s)parts.push(names[key]+": "+s.games+(s.other?" (and "+s.other+" unknown "+(s.other===1?"ID":"IDs")+")":""))}
  const tables=(result.pinballFolders||[]);if(tables.length)parts.push("Visual Pinball: "+tables[0].tables+" tables in "+tables[0].folder)
  const u=result.unrecognised||{};if(result.files!==undefined)parts.push("Unknown: "+(u.discs||0)+" disc images, "+(u.archives||0)+" archives")
  return parts.join("\n")
 }
 property bool showUnknown:false
 readonly property int unknownTotal:{const u=result.unrecognised||{};return u.total!==undefined?u.total:(u.files||[]).length}
 readonly property int twice:{let n=0;const systems=result.systems||{};for(const key of Object.keys(systems))n+=systems[key].duplicateGames||0;return n}
 // A read that finished and found no game for any system and no table folder.
 readonly property bool foundNothing:result.files!==undefined&&Object.keys(result.systems||{}).length===0&&(result.pinballFolders||[]).length===0
 implicitHeight:column.implicitHeight+(vrOverlayMode?40:32)
 radius:Theme.get("size.card.radius");color:Theme.get("color.surface.card")
 FolderDialog {id:dialog;title:"Choose a folder of your games";onAccepted:{const path=uiController.localPath(selectedFolder);if(card.roots.indexOf(path)<0)uiSettings.set("libraryRoots",card.roots.concat([path]))}}
 ColumnLayout {id:column;anchors.left:parent.left;anchors.right:parent.right;anchors.top:parent.top;anchors.margins:card.vrOverlayMode?20:16;spacing:8
  Text {text:"Game library";color:Theme.get("color.text.primary");font.family:Theme.get("font.family");font.weight:700;font.pixelSize:card.vrOverlayMode?21:18}
  UiText {Layout.fillWidth:true;wrapMode:Text.Wrap;font.pixelSize:card.vrOverlayMode?17:14;color:Theme.get("color.text.muted")
   text:"Point to one or more big folders of your own games, subfolders and all. The Hub identifies discs from their own IDs and never changes them. Set-up below lists the games it can start. A folder chosen for a system below still comes first."}
  // In a Column, not straight in the layout: this card can sit on a hidden Set-up page while its rows change.
  Column {Layout.fillWidth:true;spacing:8;visible:card.roots.length>0
   Repeater {model:card.roots
    RowLayout {required property string modelData;width:parent.width;spacing:8
     UiText {Layout.fillWidth:true;text:modelData;elide:Text.ElideMiddle;wrapMode:Text.NoWrap;font.pixelSize:card.vrOverlayMode?16:13}
     PillButton {objectName:"libraryRemove";text:"Remove";vrOverlayMode:card.vrOverlayMode;onClicked:uiSettings.set("libraryRoots",card.roots.filter(r=>r!==modelData))}}}}
  RowLayout {spacing:8
   PillButton {objectName:"libraryAdd";visible:!card.vrOverlayMode;vrOverlayMode:card.vrOverlayMode;text:"Add a folder…";onClicked:dialog.open()}
   PillButton {objectName:"libraryScan";visible:card.roots.length>0;neon:true;accent:Theme.get("color.state.ready_neon");vrOverlayMode:card.vrOverlayMode;enabled:!uiController.libraryScanning
    text:uiController.libraryScanning?"Looking…":card.result.files!==undefined?"Look again":"Find my games";onClicked:uiController.scanLibrary()}
   PillButton {objectName:"libraryStop";visible:uiController.libraryScanning;vrOverlayMode:card.vrOverlayMode;text:"Stop";onClicked:uiController.cancelLibraryScan()}
   UiText {visible:card.vrOverlayMode&&card.roots.length===0;text:"Add folders on the desktop.";font.pixelSize:15}
  }
  // Empty and nothing-found states, each with the one thing to do. Optional: a Hub with no big games folder can use the systems below instead.
  UiText {objectName:"libraryEmpty";Layout.fillWidth:true;visible:!card.vrOverlayMode&&card.roots.length===0;wrapMode:Text.Wrap;font.pixelSize:card.vrOverlayMode?16:13;color:Theme.get("color.text.muted")
   text:"No folder yet. Choose Add a folder, then Find my games. Or skip this and choose a folder for each system below."}
  UiText {objectName:"libraryNothing";Layout.fillWidth:true;visible:!uiController.libraryScanning&&card.foundNothing;wrapMode:Text.Wrap;font.pixelSize:card.vrOverlayMode?16:13;color:Theme.get("color.state.needs_neon")
   text:"No games were recognised in these folders. Check that you chose the folder that holds your discs or game files, then choose Look again."}
  UiText {objectName:"libraryProgress";visible:uiController.libraryScanning;text:uiController.libraryProgress;font.pixelSize:card.vrOverlayMode?16:13}
  UiText {objectName:"librarySummary";Layout.fillWidth:true;visible:!uiController.libraryScanning&&card.summary.length>0;wrapMode:Text.Wrap;font.pixelSize:card.vrOverlayMode?16:13;color:Theme.get("color.text.body")
   text:card.summary+(card.result.seconds!==undefined?"\nLooked through "+card.result.files+" files in "+Number(card.result.seconds).toFixed(1)+" seconds"+(card.result.scanned?", "+card.result.scanned:"")+".":"")}
  // Files the search read but did not recognise, each with why (lane L3's list, library.unrecognised.files), and games found
  // in more than one file. Shown only when the search result carries them; nothing here changes a file.
  UiText {objectName:"libraryTwice";Layout.fillWidth:true;visible:!uiController.libraryScanning&&card.twice>0;wrapMode:Text.Wrap;font.pixelSize:card.vrOverlayMode?16:13;color:Theme.get("color.text.muted")
   text:card.twice+(card.twice===1?" game was":" games were")+" found in more than one file. The Hub uses one copy of each; the Set-up report below says which and why."}
  PillButton {objectName:"libraryUnrecognisedToggle";visible:!uiController.libraryScanning&&card.unknownTotal>0;vrOverlayMode:card.vrOverlayMode
   text:(card.showUnknown?"Hide the ":"Show the ")+card.unknownTotal+(card.unknownTotal===1?" file":" files")+" not recognised";onClicked:card.showUnknown=!card.showUnknown}
  Column {objectName:"libraryUnrecognised";Layout.fillWidth:true;spacing:6;visible:card.showUnknown&&children.length>1
   Repeater {model:card.showUnknown?Array.from((card.result.unrecognised||{}).files||[]).slice(0,12):[]
    ColumnLayout {id:unknown;required property var modelData;width:parent.width;spacing:0
     UiText {Layout.fillWidth:true;text:unknown.modelData.name||"";elide:Text.ElideMiddle;wrapMode:Text.NoWrap;font.pixelSize:card.vrOverlayMode?16:13;color:Theme.get("color.text.body")}
     UiText {Layout.fillWidth:true;text:unknown.modelData.reason||"";wrapMode:Text.Wrap;font.pixelSize:card.vrOverlayMode?15:12;color:Theme.get("color.text.muted")}
    }
   }
   UiText {width:parent.width;visible:card.unknownTotal>12;wrapMode:Text.Wrap;font.pixelSize:card.vrOverlayMode?15:12;color:Theme.get("color.text.muted");text:"and "+(card.unknownTotal-12)+" more"}
  }
  // Pinball tables are played from one folder: the library can offer the one with the most tables.
  PillButton {objectName:"libraryUsePinball";vrOverlayMode:card.vrOverlayMode
   readonly property var best:(card.result.pinballFolders||[])[0]
   visible:!!best&&!(uiSettings.values,uiSettings.get("pinballFolder",""));text:"Use this folder for Visual Pinball";onClicked:uiController.setSetupFolder("pinball",best.folder)}
 }
}
