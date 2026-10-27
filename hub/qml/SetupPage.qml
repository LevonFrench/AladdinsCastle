// SPDX-License-Identifier: GPL-3.0-only
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Dialogs
import AladdinsCastle.Hub
Item {
 id:page
 property bool vrOverlayMode:false
 property int step:0
 signal finished()
 property var names:["Welcome","SteamVR","What you have","Where it is","Report"]
 // Rows a Repeater makes are kept in a Column, never straight in a layout of a page: a layout on a hidden
 // page does not take note of rows destroyed meanwhile, and showing the page then sized it from rows that
 // were already gone (a crash on Next).
 // Every change to the settings sends the systems and the report again. The lists below are replaced only
 // when what they show changes, and the system cards are keyed by id, so a click never rebuilds the card it
 // came from while the layout is still sizing it.
 property var report:({})
 function keepReport(){const next=uiController.setupReport||{};if(JSON.stringify(next)!==JSON.stringify(report))report=next}
 Connections {target:uiController;function onSetupChanged(){page.keepReport()}}
 Component.onCompleted:{keepReport();keepIds()}
 property var systems:uiController.setupSystems
 readonly property var byId:{const map={};for(const s of systems)map[s.id]=s;return map}
 property var kindIds:({})
 property var chosenIds:[]
 function keepIds(){
  const kinds={};for(const s of systems)(kinds[s.kind]=kinds[s.kind]||[]).push(s.id)
  if(JSON.stringify(kinds)!==JSON.stringify(kindIds))kindIds=kinds
  const ids=systems.filter(s=>s.chosen).map(s=>s.id);if(JSON.stringify(ids)!==JSON.stringify(chosenIds))chosenIds=ids
 }
 onSystemsChanged:keepIds()
 // A plain flag: it tells only when reading starts or ends, so the report cards are not rebuilt between.
 readonly property bool scanning:uiController.setupScanning
 property var kinds:uiController.setupKinds
 property var steam:uiController.steamVr
 property var chosen:systems.filter(s=>s.chosen)
 property color good:Theme.get("color.state.ready_neon")
 property color warn:Theme.get("color.state.needs_neon")
 property int body:vrOverlayMode?19:16
 property int heading:vrOverlayMode?30:26
 readonly property real side:vrOverlayMode?28:40
 property string pendingId:""
 // The order a first run follows: SteamVR and the headset, what you have, where it is (folders and programs), then the read.
 // What each system still needs, in plain words: state is "folder", "program" or "ready"; next is the one thing to do.
 // A system with no folder key keeps its game files beside its program, so only the program is asked for.
 function systemState(info){
  if(!info||!info.id)return {state:"",label:"",next:"",ok:false}
  const desktop=page.vrOverlayMode?" Do this in the Hub window on the desktop.":""
  if(info.folderKey&&!info.folder&&!info.guess)return {state:"folder",label:"Needs a folder",next:"Next: choose the folder that holds your game files, or Skip this system."+desktop,ok:false}
  if(!info.program)return {state:"program",label:"Needs its program",next:"Next: locate "+info.programLabel+"."+desktop,ok:false}
  return {state:"ready",label:"Ready to read",next:"Next: nothing here. The last page reads your files.",ok:true}
 }
 readonly property var needs:chosen.map(s=>page.systemState(s))
 readonly property int readyCount:needs.filter(s=>s.ok).length
 readonly property bool steamDone:steam.state==="added"
 readonly property bool steamSkipped:!steamDone&&(uiSettings.values,uiSettings.get("setupSteam","")==="later")
 readonly property var reported:report.systems||[]
 readonly property int readyGames:reported.reduce((n,s)=>n+(s.ready||[]).length,0)
 // Which pages are finished, for the tick on their step.
 readonly property var stepDone:[false,steamDone||steamSkipped,chosen.length>0,chosen.length>0&&readyCount===chosen.length,reported.length>0]
 // The four jobs of a first run, each with where it stands. Shown on the Welcome page, each with a way in.
 function road(i){
  if(i===0)return {title:"SteamVR and headset",done:steamDone,go:1,status:steam.state==="added"?"The Hub is in SteamVR. Start SteamVR and put the headset on to play.":steam.state==="no-steam"?"Steam was not found on this PC. You can set up the rest and add the Hub later.":steamSkipped?"You chose Not now. You can add it from this step whenever you like.":"Not added yet. Start SteamVR, then add the Hub."}
  if(i===1)return {title:"What you have",done:chosen.length>0,go:2,status:chosen.length?"Ticked: "+names_of(chosen):"Nothing ticked yet. Tick the systems you have game files for."}
  if(i===2)return {title:"Where the files and programs are",done:chosen.length>0&&readyCount===chosen.length,go:3,status:chosen.length===0?"Needs the step before.":readyCount+" of "+chosen.length+" have a folder and a program."}
  return {title:"Read my files",done:reported.length>0,go:4,status:report.scanned?"Last read on "+report.scanned+".":"Not read yet. This comes last."}
 }
 // One line under the steps: where you are and the one thing to do now.
 readonly property string guide:{
  const total=" of "+names.length+": "
  if(step===0)return "Step 1"+total+"start here. Choose Next when you are ready."
  if(step===1)return "Step 2"+total+(steam.state==="added"?"SteamVR is done. Choose Next.":steam.state==="no-steam"?"Steam was not found. You can carry on and add the Hub later.":"add the Hub to SteamVR, or choose Not now.")
  if(step===2)return "Step 3"+total+(chosen.length?chosen.length+(chosen.length===1?" system":" systems")+" ticked. Choose Next to say where the files are.":"tick the systems you have game files for.")
  if(step===3)return "Step 4"+total+(chosen.length===0?"nothing is ticked. Go back one step and tick a system.":readyCount===chosen.length?"every system has what it needs. Choose Read my files.":(chosen.length-readyCount)+" of "+chosen.length+" still need something. Each card says what to do next.")
  return "Step 5"+total+(scanning?"reading your folders.":reported.length?(readyGames>0?readyGames+(readyGames===1?" game":" games")+" can start. Choose Finish.":"no game can start yet. Each system says what to do next."):"nothing has been read yet. Choose Rescan.")
 }
 // Folders that may hold a system's files, from lane L3's folder checks: [{folder,detail}]. Taken from the report entry, the
 // system row, or the map uiController.setupSuggestions (id -> list) when the Hub has one; each item is a path or {folder,detail}.
 // Absent everywhere, the list is empty and nothing shows.
 function suggestionsFor(id,entry){
  const maps=uiController.setupSuggestions||({})
  const raw=Array.from((entry&&entry.suggestions)||(byId[id]||{}).suggestions||maps[id]||[])
  return raw.map(e=>typeof e==="string"?{folder:e,detail:""}:{folder:e.folder||"",detail:e.detail||""}).filter(e=>e.folder.length>0).slice(0,3)
 }
 // What the folder chosen for a system holds, in words, from uiController.setupFolderChecks (id -> {matched,checked,detail}); empty when unknown.
 function folderNote(id){
  const check=(uiController.setupFolderChecks||({}))[id]
  return check&&check.detail&&check.matched===0?check.detail:""
 }
 readonly property bool canSuggest:typeof uiController.suggestSetupFolders==="function"
 // A small size for the files a list names.
 function sizeText(bytes){const n=Number(bytes)||0;return n<=0?"":n>=1073741824?(n/1073741824).toFixed(1)+" GB":n>=1048576?Math.round(n/1048576)+" MB":Math.max(1,Math.round(n/1024))+" KB"}
 // Whether a newer program is known for a system (a flag lane L2 supplies on the system row or on its report entry).
 function hasUpdate(id,entry){return (entry&&entry.updateAvailable===true)||((byId[id]||{}).updateAvailable===true)}
 // Leaving "Where it is" forwards accepts the suggested folders and reads them; the report page shows the result.
 function go(to){if(step===3&&to>step){uiController.acceptSetupGuesses();uiController.scanSetup()} if(to>=names.length){uiController.finishSetup();page.finished()}else step=Math.max(0,to)}
 function names_of(list){return Array.from(list||[]).map(e=>e.title).join(",  ")}
 FolderDialog {id:folder;title:"Choose the folder holding your own files";onAccepted:uiController.setSetupFolder(page.pendingId,uiController.localPath(selectedFolder))}
 // The few settings a system needs beside its folder and program, each said in plain words.
 property string pendingKey:""
 readonly property var extras:({
  ps2:[{key:"ps2Python",label:"Python that runs your launcher",kind:"file",help:"The python.exe your PS2 launcher script needs."},
       {key:"ps2StopFile",label:"Stop file your launcher watches",kind:"save",help:"The Hub writes this file to ask a PS2 game to quit, so it saves."}],
  model2:[{key:"model2RuntimePath",label:"Runtime libraries of the build (optional)",kind:"folder",help:"A folder of DLLs the Model 2 build needs, when they are not beside it."}],
  "3ds":[{key:"threedsCore",label:"Nintendo 3DS core (Azahar's azahar_libretro.dll)",kind:"file",help:"The core acvr-ps1 loads to play 3DS games."},
         {key:"threedsStarFoxFile",label:"Star Fox 64 3D game file",kind:"file",help:"Choose this game's .3ds, .cci, .cxi or .3dsx file. Content identity is not verified."},
         {key:"threedsMarioKartFile",label:"Mario Kart 7 game file",kind:"file",help:"Choose this game's .3ds, .cci, .cxi or .3dsx file. The Hub does not identify games by filename."},
         {key:"libretroProfiles",label:"Profile folder (optional)",kind:"folder",help:"The game profiles, setups/retro-ps1-vr/profiles. Left empty, the Hub looks for it above its own folder."}]})
 FileDialog {id:extraFile;title:"Choose the file";onAccepted:uiSettings.set(page.pendingKey,uiController.localPath(selectedFile))}
 FileDialog {id:extraSave;title:"Choose where the stop file goes";fileMode:FileDialog.SaveFile;onAccepted:uiSettings.set(page.pendingKey,uiController.localPath(selectedFile))}
 FolderDialog {id:extraFolder;title:"Choose the folder";onAccepted:uiSettings.set(page.pendingKey,uiController.localPath(selectedFolder))}
 FileDialog {id:program;title:"Choose the program";nameFilters:["Program (*.exe)","All files (*)"];onAccepted:uiController.setSetupProgram(page.pendingId,uiController.localPath(selectedFile))}
 component Tick:Button {id:tick;property bool on:false;property string label:"";readonly property bool selected:on;hoverEnabled:true;padding:10;leftPadding:12;rightPadding:16
  implicitHeight:page.vrOverlayMode?56:44;Accessible.name:label;Accessible.checked:on
  background:Rectangle {radius:Theme.get("radius.pill");color:tick.on?Theme.get("color.surface.pill"):Theme.get("color.surface.card");border.width:tick.activeFocus||(tick.hovered&&tick.enabled)?2:0;border.color:Theme.get("color.glass.ring")}
  contentItem:Row {spacing:12
   Rectangle {anchors.verticalCenter:parent.verticalCenter;width:page.vrOverlayMode?26:22;height:width;radius:6;color:tick.on?Theme.get("color.state.ready_neon"):"transparent";border.width:2;border.color:tick.on?Theme.get("color.state.ready_neon"):Theme.get("color.text.dim")
    Canvas {anchors.fill:parent;visible:tick.on;onPaint:{const c=getContext("2d");c.reset();c.strokeStyle=Theme.get("color.surface.window");c.lineWidth=3;c.beginPath();c.moveTo(width*0.24,height*0.52);c.lineTo(width*0.43,height*0.72);c.lineTo(width*0.78,height*0.3);c.stroke()}}}
   Text {anchors.verticalCenter:parent.verticalCenter;text:tick.label;color:tick.enabled?Theme.get("color.text.primary"):Theme.get("color.text.faint");font.family:Theme.get("font.family");font.weight:700;font.pixelSize:page.vrOverlayMode?18:15}}}
 component Heading:Text {Layout.fillWidth:true;wrapMode:Text.Wrap;color:Theme.get("color.text.primary");font.family:Theme.get("font.family");font.weight:800;font.pixelSize:page.heading}
 component Para:UiText {Layout.fillWidth:true;font.pixelSize:page.body}
 ColumnLayout {anchors.fill:parent;anchors.leftMargin:page.side;anchors.rightMargin:page.side;anchors.topMargin:page.vrOverlayMode?16:24;anchors.bottomMargin:16;spacing:14
  RowLayout {spacing:8
   Text {text:"Set-up";color:Theme.get("color.text.primary");font.family:Theme.get("font.family");font.weight:900;font.pixelSize:page.vrOverlayMode?40:36;Layout.rightMargin:16}
   Repeater {model:page.names
    PillButton {required property int index;required property string modelData;objectName:"setupStep"+index;text:(index+1)+"  "+modelData+(page.stepDone[index]&&index!==page.step?"  ✓":"");neon:index===page.step;accent:Theme.get("color.text.primary");vrOverlayMode:page.vrOverlayMode;onClicked:page.go(index)}
   }
   Item {Layout.fillWidth:true}
  }
  UiText {objectName:"setupGuide";Layout.fillWidth:true;text:page.guide;font.pixelSize:page.body;font.weight:700;color:Theme.get("color.text.primary")}
  ScrollView {id:scroll;Layout.fillWidth:true;Layout.fillHeight:true;clip:true;contentWidth:availableWidth
   StackLayout {width:scroll.availableWidth;currentIndex:page.step
    ColumnLayout {spacing:10
     Heading {objectName:"setupWelcome";text:"Welcome to "+uiController.appName}
     Para {text:"The Hub starts arcade, pinball and console games in your headset, from one list."}
     Para {text:"It comes with no games. You show it where your own game files are."}
     Para {text:"Your files stay where they are. The Hub only reads those folders: it never moves, renames or changes anything in them."}
     Heading {Layout.topMargin:10;text:"Four jobs, in this order";font.pixelSize:page.heading-6}
     // In a Column, not straight in the layout, like every list on this page.
     Column {Layout.fillWidth:true;spacing:8
      Repeater {model:4
       RowLayout {id:job;required property int index;readonly property var info:page.road(index);width:parent.width;spacing:12
        Text {Layout.alignment:Qt.AlignTop;text:(job.index+1)+".";color:job.info.done?page.good:Theme.get("color.text.primary");font.family:Theme.get("font.family");font.weight:800;font.pixelSize:page.body+2}
        ColumnLayout {Layout.fillWidth:true;spacing:2
         Text {text:job.info.title+(job.info.done?"  ✓":"");color:job.info.done?page.good:Theme.get("color.text.primary");font.family:Theme.get("font.family");font.weight:700;font.pixelSize:page.body+1}
         UiText {objectName:"setupRoadStatus"+job.index;Layout.fillWidth:true;text:job.info.status;font.pixelSize:page.body-2;color:Theme.get("color.text.muted_detail")}
        }
        PillButton {objectName:"setupRoad"+job.index;text:job.info.done?"Review":"Open";vrOverlayMode:page.vrOverlayMode;onClicked:page.go(job.info.go)}
       }
      }
     }
     Para {text:"You can skip any step and come back later from ··· then Set-up. Nothing here is a dead end: Next and Skip set-up always work."}
    }
    ColumnLayout {spacing:10
     Heading {objectName:"setupSteamHeading";text:page.steam.state==="added"?"The Hub is already added to SteamVR.":page.steam.state==="no-steam"?"Steam was not found on this PC.":"Add the Hub to SteamVR so it appears in your dashboard?";color:page.steam.state==="added"?page.good:Theme.get("color.text.primary")}
     Para {objectName:"setupSteamChecking";visible:!page.steam.state;text:"Looking for Steam on this PC…"}
     Para {objectName:"setupSteamNone";visible:page.steam.state==="no-steam";color:page.warn;text:"There is nothing to add the Hub to yet. To use the headset, install Steam and SteamVR, then open Set-up again from ··· then Set-up. You can carry on now: the rest of set-up does not need it."}
     RowLayout {spacing:8;visible:page.steam.state==="absent"
      PillButton {objectName:"setupSteamYes";text:"Yes, add it";neon:true;accent:page.good;vrOverlayMode:page.vrOverlayMode;onClicked:{uiSettings.set("setupSteam","yes");uiController.addToSteamVr()}}
      PillButton {objectName:"setupSteamLater";text:"Not now";vrOverlayMode:page.vrOverlayMode;onClicked:{uiSettings.set("setupSteam","later");page.go(2)}}
     }
     Para {objectName:"setupSteamError";visible:!!page.steam.error;color:page.warn;text:"SteamVR did not add it: "+(page.steam.error||"")+" Start SteamVR first, then choose Yes again."}
     Para {text:"Adding it only tells SteamVR where the Hub is. It changes nothing else in Steam, and Not now leaves everything as it is."}
     Heading {Layout.topMargin:10;text:"To use it in the headset";font.pixelSize:page.heading-6}
     Para {objectName:"setupHeadset";text:"Headset first: connect it and check that SteamVR shows it ready. When you start a headset game, the Hub can start SteamVR for you; Settings has a switch for that."}
     Para {text:"1.  Start SteamVR and put the headset on."}
     Para {text:"2.  Press the system button on a controller to open the dashboard."}
     Para {text:"3.  In the row of buttons along the bottom of the dashboard, choose "+uiController.appName+"."}
     Para {text:"4.  To have it start with SteamVR every time: in SteamVR open Settings, Startup / Shutdown, Choose Startup Overlay Apps, and turn "+uiController.appName+" on."}
     Para {text:"If it does not show: start the Hub once more while SteamVR is running, or restart SteamVR."}
     Heading {Layout.topMargin:10;text:"In a game";font.pixelSize:page.heading-6}
     Para {objectName:"setupControls";text:uiController.universalControls+" "+uiController.universalMenuNote}
    }
    ColumnLayout {spacing:10
     Heading {text:"What do you have?"}
     Para {text:"Tick what you own game files for. Under each are the systems the Hub can start today."}
     Para {objectName:"setupNothingTicked";visible:page.chosen.length===0;color:page.warn;text:"Nothing is ticked yet. Tick a kind below, then the systems under it. Not sure? Choose Skip set-up and come back from ··· then Set-up."}
     Repeater {model:[{id:"arcade",label:"Arcade"},{id:"pinball",label:"Pinball"},{id:"consoles",label:"Consoles"}]
      ColumnLayout {id:kind;required property var modelData;Layout.fillWidth:true;spacing:6
       Tick {objectName:"setupKind-"+kind.modelData.id;label:kind.modelData.label;on:page.kinds[kind.modelData.id]===true;onClicked:uiController.setSetupKind(kind.modelData.id,!on)}
       Column {Layout.fillWidth:true;Layout.leftMargin:36;spacing:6
        Repeater {model:page.kindIds[kind.modelData.id]||[]
         RowLayout {id:row;required property string modelData;readonly property var info:page.byId[modelData]||({});width:parent.width;spacing:10;opacity:page.kinds[kind.modelData.id]===true?1:0.45
          Tick {objectName:"setupSystem-"+row.modelData;label:row.info.title;on:row.info.chosen===true;enabled:page.kinds[kind.modelData.id]===true;onClicked:uiController.setSetupSystem(row.modelData,!on)}
          UiText {Layout.fillWidth:true;text:row.info.detail;font.pixelSize:page.body-2;color:Theme.get("color.text.muted_detail")}
         }
        }
       }
      }
     }
    }
    ColumnLayout {spacing:10
     Heading {text:"Where is it?"}
     GameLibraryCard {objectName:"gameLibrary";Layout.fillWidth:true;vrOverlayMode:page.vrOverlayMode}
     Para {text:page.chosen.length?"One folder of your own files for each system, and the program that plays it. Leave any empty to skip it for now."+(page.vrOverlayMode?" Folders and programs are chosen on the desktop.":""):"Nothing is ticked on the page before, so there is nothing to find. Go back and tick what you have."}
     PillButton {objectName:"setupGoSystems";visible:page.chosen.length===0;text:"Go back and tick what you have";vrOverlayMode:page.vrOverlayMode;onClicked:page.go(2)}
     Column {Layout.fillWidth:true;spacing:10;visible:children.length>1
      Repeater {model:page.chosenIds
       Rectangle {id:card;required property string modelData;readonly property var info:page.byId[modelData]||({});readonly property var need:page.systemState(info);width:parent.width;implicitHeight:inner.implicitHeight+32;radius:Theme.get("size.card.radius");color:Theme.get("color.surface.card");border.width:0
        ColumnLayout {id:inner;anchors.fill:parent;anchors.margins:16;spacing:8
         RowLayout {spacing:14
          Text {text:card.info.title;color:Theme.get("color.text.primary");font.family:Theme.get("font.family");font.weight:700;font.pixelSize:page.vrOverlayMode?21:18}
          UiText {objectName:"setupState-"+card.modelData;text:card.need.label;font.pixelSize:page.body;font.weight:700;color:card.need.ok?page.good:page.warn}
          // SLOT for lane L2: "update available" for this system's program. Shown when the system row carries updateAvailable=true.
          UiText {objectName:"setupUpdate-"+card.modelData;visible:page.hasUpdate(card.modelData,null);text:"Update available";font.pixelSize:page.body;font.weight:700;color:page.warn}
          Item {Layout.fillWidth:true}
         }
         Para {objectName:"setupAction-"+card.modelData;text:card.need.next;font.pixelSize:page.body-1;color:card.need.ok?Theme.get("color.text.muted_detail"):Theme.get("color.text.body")}
         Para {visible:!card.info.folderKey;text:card.modelData==="3ds"?"Choose each game file below. The Hub does not identify games by filename.":"This program keeps its game files in a folder beside itself, so there is no folder to choose.";font.pixelSize:page.body-1;color:Theme.get("color.text.muted_detail")}
         RowLayout {spacing:8;visible:!!card.info.folderKey
          UiText {Layout.fillWidth:true;objectName:"setupFolder-"+card.modelData;font.pixelSize:page.body-1;elide:Text.ElideMiddle;wrapMode:Text.NoWrap
           text:"Your files:  "+(card.info.folder||(card.info.guess?card.info.guess+"   (found, used unless you change it)":"not chosen"))
           color:card.info.folder?page.good:card.info.guess?Theme.get("color.text.body"):page.warn}
          PillButton {text:"Choose folder";visible:!page.vrOverlayMode;onClicked:{page.pendingId=card.modelData;folder.open()}}
          PillButton {objectName:"setupSkip-"+card.modelData;text:"Skip";vrOverlayMode:page.vrOverlayMode;onClicked:{uiController.setSetupFolder(card.modelData,"");uiController.setSetupSystem(card.modelData,false)}}
         }
         // Lane L3's folder checks: what the chosen folder holds when it fits nothing, and folders that may hold the files,
         // found by reading a few files in them. Shown only when the Hub supplies them; one click uses a folder.
         Para {objectName:"setupFolderNote-"+card.modelData;visible:text.length>0&&!!card.info.folder;text:page.folderNote(card.modelData);color:page.warn;font.pixelSize:page.body-1}
         Column {objectName:"setupSuggestions-"+card.modelData;Layout.fillWidth:true;spacing:6;visible:children.length>1&&(!card.info.folder||page.folderNote(card.modelData).length>0)
          Repeater {model:page.suggestionsFor(card.modelData,null)
           ColumnLayout {id:suggestion;required property var modelData;width:parent.width;spacing:2
            RowLayout {spacing:8
             UiText {Layout.fillWidth:true;text:"May hold your files:  "+suggestion.modelData.folder;elide:Text.ElideMiddle;wrapMode:Text.NoWrap;font.pixelSize:page.body-1}
             PillButton {text:"Use this folder";vrOverlayMode:page.vrOverlayMode;onClicked:uiController.setSetupFolder(card.modelData,suggestion.modelData.folder)}
            }
            UiText {Layout.fillWidth:true;visible:text.length>0;text:suggestion.modelData.detail;font.pixelSize:page.body-2;color:Theme.get("color.text.muted_detail")}
           }
          }
         }
         PillButton {objectName:"setupFindFolder-"+card.modelData;visible:page.canSuggest&&!!card.info.folderKey&&!page.vrOverlayMode&&page.suggestionsFor(card.modelData,null).length===0&&(!card.info.folder||page.folderNote(card.modelData).length>0)
          text:"Look for the folder";onClicked:uiController.suggestSetupFolders(card.modelData)}
         RowLayout {spacing:8
          UiText {Layout.fillWidth:true;font.pixelSize:page.body-1;elide:Text.ElideMiddle;wrapMode:Text.NoWrap
           text:card.info.programLabel+":  "+(card.info.program||"not chosen")
           color:card.info.program?page.good:page.warn}
          PillButton {text:"Locate program";visible:!page.vrOverlayMode;onClicked:{page.pendingId=card.modelData;program.open()}}
         }
         Repeater {model:page.extras[card.modelData]||[]
          RowLayout {required property var modelData;Layout.fillWidth:true;spacing:8
           readonly property string value:(uiSettings.values,uiSettings.get(modelData.key,""))
           UiText {Layout.fillWidth:true;objectName:"setupExtra-"+modelData.key;font.pixelSize:page.body-1;elide:Text.ElideMiddle;wrapMode:Text.NoWrap
            text:modelData.label+":  "+(parent.value||"not chosen");color:parent.value?page.good:(modelData.kind==="folder"?Theme.get("color.text.muted_detail"):page.warn)}
           PillButton {text:"Choose";visible:!page.vrOverlayMode;onClicked:{page.pendingKey=modelData.key;(modelData.kind==="folder"?extraFolder:modelData.kind==="save"?extraSave:extraFile).open()}}
           PillButton {text:"Clear";visible:!!parent.value;vrOverlayMode:page.vrOverlayMode;onClicked:uiSettings.set(modelData.key,"")}
          }
         }
        }
       }
      }
     }
    }
    ColumnLayout {spacing:10
     RowLayout {spacing:10
      Heading {text:"What you have"}
      UiText {text:page.report.scanned?"Read on "+page.report.scanned:"";font.pixelSize:page.body-2;color:Theme.get("color.text.muted_detail")}
      PillButton {objectName:"setupRescan";text:uiController.setupScanning?"Reading…":"Rescan";enabled:!uiController.setupScanning;vrOverlayMode:page.vrOverlayMode;onClicked:uiController.scanSetup()}
     }
     // Running a program the Hub found is the user's call, asked once, plainly.
     Rectangle {objectName:"setupQueryEmulators";Layout.fillWidth:true;visible:(uiSettings.values,uiSettings.get("queryEmulators","")==="");implicitHeight:ask.implicitHeight+28;radius:Theme.get("size.card.radius");color:Theme.get("color.surface.card")
      ColumnLayout {id:ask;x:14;y:14;width:parent.width-28;spacing:8
       Para {Layout.fillWidth:true;text:"May the Hub run the MAME builds it found, hidden and once, to read what each game needs? Then the files inside your games are checked too. Without it, games are found by their names only. You can change this in Settings."}
       RowLayout {spacing:10
        PillButton {objectName:"setupQueryYes";text:"Yes, ask them";neon:true;accent:page.good;vrOverlayMode:page.vrOverlayMode;onClicked:{uiSettings.set("queryEmulators",true);uiController.scanSetup()}}
        PillButton {objectName:"setupQueryNo";text:"No, names only";vrOverlayMode:page.vrOverlayMode;onClicked:uiSettings.set("queryEmulators",false)}
       }
      }
     }
     Para {objectName:"setupProgress";visible:uiController.setupScanning;color:page.warn;text:"Reading your folders. Nothing in them is changed.  "+uiController.setupProgress}
     // The read stopped with a problem: say what, and the two ways on.
     Para {objectName:"setupReportError";visible:!!page.report.error&&!page.scanning;color:page.warn;text:"The read did not finish: "+(page.report.error||"")+"  Choose Rescan to try again, or check the folders on the step before."}
     RowLayout {spacing:8;visible:!!page.report.error&&!page.scanning
      PillButton {objectName:"setupErrorFolders";text:"Back to folders and programs";vrOverlayMode:page.vrOverlayMode;onClicked:page.go(3)}
     }
     // Nothing read yet: either nothing is ticked (go back) or the read has not run (run it).
     Para {objectName:"setupEmpty";visible:!page.scanning&&!page.reported.length&&!page.report.error;text:page.chosen.length===0?"Nothing is ticked, so there is nothing to read. Go back and tick what you have.":"Nothing has been read yet. Choose Read now to look in your folders. Nothing in them is changed."}
     RowLayout {spacing:8;visible:!page.scanning&&!page.reported.length&&!page.report.error
      PillButton {objectName:"setupEmptyAction";text:page.chosen.length===0?"Tick what you have":"Read now";neon:true;accent:page.good;vrOverlayMode:page.vrOverlayMode;onClicked:{if(page.chosen.length===0)page.go(2);else uiController.scanSetup()}}
     }
     // The one line to take away from the read.
     Para {objectName:"setupSummary";visible:!page.scanning&&page.reported.length>0;font.weight:700;color:page.readyGames>0?page.good:page.warn
      text:page.readyGames>0?page.readyGames+(page.readyGames===1?" game can":" games can")+" start now. Choose Finish, then pick one from the list. Anything listed below as not found or not right can be fixed later: Set-up reads again whenever you choose Rescan.":"No game can start yet. Each system below says what to do next."}
     RowLayout {spacing:8;visible:!page.scanning&&page.reported.length>0&&page.readyGames===0
      PillButton {objectName:"setupSummaryFolders";text:"Back to folders and programs";vrOverlayMode:page.vrOverlayMode;onClicked:page.go(3)}
     }
     Para {visible:(page.report.systems||[]).length>0;text:"Found, can start: the game's files and its program are on this PC. Found, not right: the file is there but cannot be used as it is. Not found: the Hub knows the game but did not find it; only the name it looks for is shown. None of this says a game works well in the headset: only you can say that, in your list of accepted games."}
     Column {Layout.fillWidth:true;spacing:10;visible:children.length>1
      Repeater {model:page.scanning?[]:(page.report.systems||[])
       Rectangle {id:result;required property var modelData;objectName:"setupResult-"+modelData.id;property bool showFiles:false;property bool showTwice:false;readonly property int unrecognisedTotal:(modelData.unrecognised||[]).length+(modelData.unrecognisedMore||0);width:parent.width;implicitHeight:lines.implicitHeight+32;radius:Theme.get("size.card.radius");color:Theme.get("color.surface.card");border.width:0
        ColumnLayout {id:lines;anchors.fill:parent;anchors.margins:16;spacing:8
         RowLayout {spacing:14
          Text {text:result.modelData.title;color:Theme.get("color.text.primary");font.family:Theme.get("font.family");font.weight:700;font.pixelSize:page.vrOverlayMode?21:18}
          UiText {text:"Found, can start "+result.modelData.ready.length;font.pixelSize:page.body;color:page.good}
          UiText {text:"Found, not right "+result.modelData.notRight.length;font.pixelSize:page.body;color:result.modelData.notRight.length?page.warn:Theme.get("color.text.muted_detail")}
          UiText {text:"Not found "+result.modelData.missing.length;font.pixelSize:page.body;color:Theme.get("color.text.muted_detail")}
          // SLOT for lane L2: "update available", from this report entry's updateAvailable or the system row's.
          UiText {objectName:"setupReportUpdate-"+result.modelData.id;visible:page.hasUpdate(result.modelData.id,result.modelData);text:"Update available";font.pixelSize:page.body;font.weight:700;color:page.warn}
          Item {Layout.fillWidth:true}
         }
         Para {visible:!!result.modelData.note;text:result.modelData.note||"";color:Theme.get("color.text.muted_detail")}
         Para {visible:result.modelData.ready.length>0;text:"Found, can start:  "+page.names_of(result.modelData.ready);color:page.good;font.pixelSize:page.body-1}
         Repeater {model:result.modelData.notRight
          Para {required property var modelData;text:"Found, not right:  "+modelData.title+".  "+modelData.reason;color:page.warn;font.pixelSize:page.body-1}
         }
         Para {visible:result.modelData.missing.length>0;font.pixelSize:page.body-1;color:Theme.get("color.text.muted_detail")
          text:"Not found:  "+Array.from(result.modelData.missing).map(e=>e.title+" ("+e.expected+")").join(",  ")}
         Para {objectName:"setupOther-"+result.modelData.id;visible:(result.modelData.other||[]).length>0;font.pixelSize:page.body-2;color:Theme.get("color.text.muted_detail")
          text:{let all=Array.from(result.modelData.other||[]);return "Found, not in the Hub's list ("+all.length+"):  "+all.slice(0,6).map(e=>e.title+" ("+e.detail+")").join(",  ")+(all.length>6?",  and "+(all.length-6)+" more":"")}}
         // Lane L3's finds, each shown only when the report entry carries it and never changing a file.
         // Files in the folder that matched nothing, with why in plain words: a count that opens into a short list.
         Para {objectName:"setupUnrecognisedUnchecked-"+result.modelData.id;visible:result.modelData.unrecognisedChecked===false;font.pixelSize:page.body-2;color:Theme.get("color.text.muted_detail")
          text:"Files the Hub did not recognise are not listed for this folder yet: your Game library has not looked through it. Add the folder to the Game library on the step before and choose Find my games."}
         PillButton {objectName:"setupUnrecognisedToggle-"+result.modelData.id;visible:result.unrecognisedTotal>0;vrOverlayMode:page.vrOverlayMode
          text:(result.showFiles?"Hide the ":"Show the ")+result.unrecognisedTotal+(result.unrecognisedTotal===1?" file":" files")+" not recognised";onClicked:result.showFiles=!result.showFiles}
         Column {objectName:"setupUnrecognised-"+result.modelData.id;Layout.fillWidth:true;spacing:6;visible:result.showFiles&&children.length>1
          Repeater {model:result.showFiles?Array.from(result.modelData.unrecognised||[]).slice(0,12):[]
           ColumnLayout {id:unknown;required property var modelData;width:parent.width;spacing:0
            UiText {Layout.fillWidth:true;text:(unknown.modelData.name||"")+(page.sizeText(unknown.modelData.size)?"   ("+page.sizeText(unknown.modelData.size)+")":"");elide:Text.ElideMiddle;wrapMode:Text.NoWrap;font.pixelSize:page.body-2;color:Theme.get("color.text.body")}
            UiText {Layout.fillWidth:true;text:(unknown.modelData.kind==="other"?"Wrong folder?  ":"")+(unknown.modelData.reason||"");font.pixelSize:page.body-2;color:Theme.get("color.text.muted_detail")}
           }
          }
          Para {width:parent.width;visible:result.unrecognisedTotal>12;font.pixelSize:page.body-2;color:Theme.get("color.text.muted_detail");text:"and "+(result.unrecognisedTotal-12)+" more"}
         }
         // The same game found in more than one file, which copy the Hub uses and why the others lose.
         PillButton {objectName:"setupTwiceToggle-"+result.modelData.id;visible:(result.modelData.duplicates||[]).length>0;vrOverlayMode:page.vrOverlayMode
          text:(result.showTwice?"Hide the ":"Show the ")+(result.modelData.duplicates||[]).length+((result.modelData.duplicates||[]).length===1?" game":" games")+" found more than once";onClicked:result.showTwice=!result.showTwice}
         Column {objectName:"setupTwice-"+result.modelData.id;Layout.fillWidth:true;spacing:8;visible:result.showTwice&&children.length>1
          Repeater {model:result.showTwice?Array.from(result.modelData.duplicates||[]).slice(0,6):[]
           ColumnLayout {id:twice;required property var modelData;width:parent.width;spacing:0
            UiText {Layout.fillWidth:true;font.weight:700;font.pixelSize:page.body-2;color:Theme.get("color.text.body");elide:Text.ElideRight;wrapMode:Text.NoWrap
             text:(twice.modelData.title||"")+(twice.modelData.sameDisc===false?"   (different releases)":"")}
            Repeater {model:Array.from(twice.modelData.copies||[]).slice(0,5)
             UiText {required property var modelData;Layout.fillWidth:true;font.pixelSize:page.body-2;color:modelData.kept?page.good:Theme.get("color.text.muted_detail");elide:Text.ElideMiddle;wrapMode:Text.NoWrap
              text:modelData.kept?"Used:  "+modelData.name:"Not used:  "+modelData.name+(modelData.why?"  -  "+modelData.why:"")}
            }
           }
          }
          Para {width:parent.width;visible:!!result.modelData.duplicateRule;font.pixelSize:page.body-2;color:Theme.get("color.text.muted_detail");text:result.modelData.duplicateRule||""}
         }
         // What the folder holds when it fits nothing, and folders that may hold the files; one click uses one and reads again.
         Para {objectName:"setupReportFolderNote-"+result.modelData.id;visible:result.modelData.ready.length===0&&text.length>0;text:page.folderNote(result.modelData.id);color:page.warn;font.pixelSize:page.body-1}
         Column {objectName:"setupReportSuggestions-"+result.modelData.id;Layout.fillWidth:true;spacing:6;visible:result.modelData.ready.length===0&&children.length>1
          Repeater {model:page.suggestionsFor(result.modelData.id,result.modelData)
           ColumnLayout {id:suggested;required property var modelData;width:parent.width;spacing:2
            RowLayout {spacing:8
             UiText {Layout.fillWidth:true;text:"May hold your files:  "+suggested.modelData.folder;elide:Text.ElideMiddle;wrapMode:Text.NoWrap;font.pixelSize:page.body-1}
             PillButton {text:"Use this folder";vrOverlayMode:page.vrOverlayMode;onClicked:{uiController.setSetupFolder(result.modelData.id,suggested.modelData.folder);uiController.scanSetup()}}
            }
            UiText {Layout.fillWidth:true;visible:text.length>0;text:suggested.modelData.detail;font.pixelSize:page.body-2;color:Theme.get("color.text.muted_detail")}
           }
          }
         }
         PillButton {objectName:"setupReportFindFolder-"+result.modelData.id;vrOverlayMode:page.vrOverlayMode;visible:page.canSuggest&&result.modelData.ready.length===0&&page.suggestionsFor(result.modelData.id,result.modelData).length===0
          text:"Look for the folder";onClicked:uiController.suggestSetupFolders(result.modelData.id)}
         // The one thing to do about this system.
         Para {objectName:"setupAdvice-"+result.modelData.id;font.pixelSize:page.body-1;font.weight:700;color:result.modelData.ready.length>0&&!result.modelData.notRight.length?Theme.get("color.text.muted_detail"):Theme.get("color.text.body")
          text:{const m=result.modelData,r=m.ready.length,n=m.notRight.length,x=m.missing.length
           if(n>0)return (r>0?"Some games can start. ":"")+"Next: fix what is listed under Found, not right, then choose Rescan."
           if(r>0)return "Next: nothing here. These start from the Hub's list."
           if(x>0)return "Next: check that this system's folder holds your game files, then choose Rescan."
           return "Nothing was found. Next: check that you chose the folder that holds your files for this system, then choose Rescan."}}
        }
       }
      }
     }
    }
   }
  }
  RowLayout {spacing:8
   PillButton {objectName:"setupBack";targetHeight:page.vrOverlayMode?56:48;text:"‹ Back";enabled:page.step>0;vrOverlayMode:page.vrOverlayMode;onClicked:page.go(page.step-1)}
   PillButton {objectName:"setupNext";targetHeight:page.vrOverlayMode?56:48;text:page.step===page.names.length-1?"Finish":page.step===3&&page.chosen.length>0?"Read my files":"Next";neon:true;accent:page.good;vrOverlayMode:page.vrOverlayMode;onClicked:page.go(page.step+1)}
   PillButton {objectName:"setupSkipAll";targetHeight:page.vrOverlayMode?56:48;text:"Skip set-up";visible:page.step<page.names.length-1;vrOverlayMode:page.vrOverlayMode;onClicked:{uiController.finishSetup();page.finished()}}
   Item {Layout.fillWidth:true}
  }
 }
}
