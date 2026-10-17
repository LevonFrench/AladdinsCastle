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
 readonly property var names:({model3:"Sega Model 3",model2:"Sega Model 2",ps2:"PlayStation 2",dreamcast:"Sega Dreamcast",wii:"Nintendo Wii"})
 readonly property string summary:{
  const systems=result.systems||{};let parts=[]
  for(const key of ["model3","model2","wii","dreamcast","ps2"]){const s=systems[key];if(s)parts.push(names[key]+": "+s.games+(s.other?" (and "+s.other+" not in the Hub's list)":""))}
  const tables=(result.pinballFolders||[]);if(tables.length)parts.push("Visual Pinball: "+tables[0].tables+" tables in "+tables[0].folder)
  const u=result.unrecognised||{};if(result.files!==undefined)parts.push("Not games the Hub plays: "+(u.discs||0)+" disc images, "+(u.archives||0)+" archives")
  return parts.join("\n")
 }
 implicitHeight:column.implicitHeight+(vrOverlayMode?40:32)
 radius:Theme.get("size.card.radius");color:Theme.get("color.surface.card")
 FolderDialog {id:dialog;title:"Choose a folder of your games";onAccepted:{const path=uiController.localPath(selectedFolder);if(card.roots.indexOf(path)<0)uiSettings.set("libraryRoots",card.roots.concat([path]))}}
 ColumnLayout {id:column;anchors.left:parent.left;anchors.right:parent.right;anchors.top:parent.top;anchors.margins:card.vrOverlayMode?20:16;spacing:8
  Text {text:"Game library";color:Theme.get("color.text.primary");font.family:Theme.get("font.family");font.weight:700;font.pixelSize:card.vrOverlayMode?21:18}
  UiText {Layout.fillWidth:true;wrapMode:Text.Wrap;font.pixelSize:card.vrOverlayMode?17:14;color:Theme.get("color.text.muted")
   text:"Point to one or more big folders of your own games, subfolders and all. The Hub looks through them for every game it plays, by what is inside each file where it can, and never changes anything in them. A folder chosen for a system below still comes first."}
  Repeater {model:card.roots
   RowLayout {required property string modelData;Layout.fillWidth:true;spacing:8
    UiText {Layout.fillWidth:true;text:modelData;elide:Text.ElideMiddle;wrapMode:Text.NoWrap;font.pixelSize:card.vrOverlayMode?16:13}
    PillButton {objectName:"libraryRemove";text:"Remove";vrOverlayMode:card.vrOverlayMode;onClicked:uiSettings.set("libraryRoots",card.roots.filter(r=>r!==modelData))}}}
  RowLayout {spacing:8
   PillButton {objectName:"libraryAdd";visible:!card.vrOverlayMode;vrOverlayMode:card.vrOverlayMode;text:"Add a folder…";onClicked:dialog.open()}
   PillButton {objectName:"libraryScan";visible:card.roots.length>0;neon:true;accent:Theme.get("color.state.ready_neon");vrOverlayMode:card.vrOverlayMode;enabled:!uiController.libraryScanning
    text:uiController.libraryScanning?"Looking…":card.result.files!==undefined?"Look again":"Find my games";onClicked:uiController.scanLibrary()}
   UiText {visible:card.vrOverlayMode&&card.roots.length===0;text:"Add folders on the desktop.";font.pixelSize:15}
  }
  UiText {objectName:"libraryProgress";visible:uiController.libraryScanning;text:uiController.libraryProgress;font.pixelSize:card.vrOverlayMode?16:13}
  UiText {objectName:"librarySummary";Layout.fillWidth:true;visible:!uiController.libraryScanning&&card.summary.length>0;wrapMode:Text.Wrap;font.pixelSize:card.vrOverlayMode?16:13;color:Theme.get("color.text.body")
   text:card.summary+(card.result.seconds!==undefined?"\nLooked through "+card.result.files+" files in "+Number(card.result.seconds).toFixed(1)+" seconds"+(card.result.scanned?", "+card.result.scanned:"")+".":"")}
  // Pinball tables are played from one folder: the library can offer the one with the most tables.
  PillButton {objectName:"libraryUsePinball";vrOverlayMode:card.vrOverlayMode
   readonly property var best:(card.result.pinballFolders||[])[0]
   visible:!!best&&!(uiSettings.values,uiSettings.get("pinballFolder",""));text:"Use this folder for Visual Pinball";onClicked:uiController.setSetupFolder("pinball",best.folder)}
 }
}
