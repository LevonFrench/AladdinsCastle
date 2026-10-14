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
 property var report:uiController.setupReport
 property var systems:uiController.setupSystems
 property var kinds:uiController.setupKinds
 property var steam:uiController.steamVr
 property var chosen:systems.filter(s=>s.chosen)
 property color good:Theme.get("color.state.ready_neon")
 property color warn:Theme.get("color.state.needs_neon")
 property int body:vrOverlayMode?18:15
 property string pendingId:""
 // Leaving "Where it is" forwards accepts the suggested folders and reads them; the report page shows the result.
 function go(to){if(step===3&&to>step){uiController.acceptSetupGuesses();uiController.scanSetup()} if(to>=names.length){uiController.finishSetup();page.finished()}else step=Math.max(0,to)}
 function names_of(list){return Array.from(list||[]).map(e=>e.title).join(",  ")}
 FolderDialog {id:folder;title:"Choose the folder holding your own files";onAccepted:uiController.setSetupFolder(page.pendingId,uiController.localPath(selectedFolder))}
 FileDialog {id:program;title:"Choose the program";nameFilters:["Program (*.exe)","All files (*)"];onAccepted:uiController.setSetupProgram(page.pendingId,uiController.localPath(selectedFile))}
 component Tick:PillButton {property bool on:false;text:(on?"● ":"○ ")+label;property string label:"";selected:on;vrOverlayMode:page.vrOverlayMode}
 component Para:UiText {Layout.fillWidth:true;font.pixelSize:page.body}
 ColumnLayout {anchors.fill:parent;spacing:12
  RowLayout {spacing:8
   UiText {text:"Set-up";font.pixelSize:24}
   Repeater {model:page.names
    PillButton {required property int index;required property string modelData;objectName:"setupStep"+index;text:(index+1)+"  "+modelData;selected:index===page.step;vrOverlayMode:page.vrOverlayMode;onClicked:page.go(index)}
   }
   Item {Layout.fillWidth:true}
  }
  ScrollView {id:scroll;Layout.fillWidth:true;Layout.fillHeight:true;clip:true;contentWidth:availableWidth
   StackLayout {width:scroll.availableWidth;currentIndex:page.step
    ColumnLayout {spacing:10
     UiText {objectName:"setupWelcome";Layout.fillWidth:true;text:"Welcome to "+uiController.appName;font.pixelSize:22;color:Theme.get("color.text.primary")}
     Para {text:"The Hub starts arcade, pinball and console games in your headset, from one list."}
     Para {text:"It comes with no games. You show it where your own game files are."}
     Para {text:"Your files stay where they are. The Hub only reads those folders: it never moves, renames or changes anything in them."}
     Para {text:"The next steps ask whether to add the Hub to SteamVR, what you have, and where it is. You can skip any step and come back later from ··· then Set-up."}
    }
    ColumnLayout {spacing:10
     UiText {Layout.fillWidth:true;text:page.steam.state==="added"?"The Hub is already added to SteamVR.":"Add the Hub to SteamVR so it appears in your dashboard?";font.pixelSize:20;color:page.steam.state==="added"?page.good:Theme.get("color.text.primary")}
     Para {visible:page.steam.state==="no-steam";color:page.warn;text:"Steam was not found on this PC, so there is nothing to add the Hub to yet."}
     RowLayout {spacing:8;visible:page.steam.state==="absent"
      PillButton {objectName:"setupSteamYes";text:"Yes, add it";neon:true;accent:page.good;vrOverlayMode:page.vrOverlayMode;onClicked:{uiSettings.set("setupSteam","yes");uiController.addToSteamVr()}}
      PillButton {objectName:"setupSteamLater";text:"Not now";vrOverlayMode:page.vrOverlayMode;onClicked:{uiSettings.set("setupSteam","later");page.go(2)}}
     }
     Para {objectName:"setupSteamError";visible:!!page.steam.error;color:page.warn;text:"SteamVR did not add it: "+(page.steam.error||"")+" Start SteamVR first, then choose Yes again."}
     Para {text:"Adding it only tells SteamVR where the Hub is. It changes nothing else in Steam, and Not now leaves everything as it is."}
     UiText {Layout.fillWidth:true;Layout.topMargin:8;text:"To use it in the headset";font.pixelSize:18;color:Theme.get("color.text.primary")}
     Para {text:"1.  Start SteamVR and put the headset on."}
     Para {text:"2.  Press the system button on a controller to open the dashboard."}
     Para {text:"3.  In the row of buttons along the bottom of the dashboard, choose "+uiController.appName+"."}
     Para {text:"4.  To have it start with SteamVR every time: in SteamVR open Settings, Startup / Shutdown, Choose Startup Overlay Apps, and turn "+uiController.appName+" on."}
     Para {text:"If it does not show: start the Hub once more while SteamVR is running, or restart SteamVR."}
    }
    ColumnLayout {spacing:10
     UiText {Layout.fillWidth:true;text:"What do you have?";font.pixelSize:20;color:Theme.get("color.text.primary")}
     Para {text:"Tick what you own game files for. Under each are the systems the Hub can start today."}
     Repeater {model:[{id:"arcade",label:"Arcade"},{id:"pinball",label:"Pinball"},{id:"consoles",label:"Consoles"}]
      ColumnLayout {id:kind;required property var modelData;Layout.fillWidth:true;spacing:6
       Tick {objectName:"setupKind-"+kind.modelData.id;label:kind.modelData.label;on:page.kinds[kind.modelData.id]===true;onClicked:uiController.setSetupKind(kind.modelData.id,!on)}
       Repeater {model:page.systems.filter(s=>s.kind===kind.modelData.id)
        RowLayout {id:row;required property var modelData;Layout.fillWidth:true;Layout.leftMargin:36;spacing:10;opacity:page.kinds[kind.modelData.id]===true?1:0.45
         Tick {objectName:"setupSystem-"+row.modelData.id;label:row.modelData.title;on:row.modelData.chosen===true;enabled:page.kinds[kind.modelData.id]===true;onClicked:uiController.setSetupSystem(row.modelData.id,!on)}
         UiText {Layout.fillWidth:true;text:row.modelData.detail;font.pixelSize:page.body-2;color:Theme.get("color.text.muted_detail")}
        }
       }
      }
     }
    }
    ColumnLayout {spacing:10
     UiText {Layout.fillWidth:true;text:"Where is it?";font.pixelSize:20;color:Theme.get("color.text.primary")}
     Para {text:page.chosen.length?"One folder of your own files for each system, and the program that plays it. Leave any empty to skip it for now."+(page.vrOverlayMode?" Folders and programs are chosen on the desktop.":""):"Nothing is ticked on the page before, so there is nothing to find. Go back and tick what you have."}
     Repeater {model:page.chosen
      Rectangle {id:card;required property var modelData;Layout.fillWidth:true;implicitHeight:inner.implicitHeight+20;radius:Theme.get("size.card.radius");color:Theme.get("color.surface.card");border.color:Theme.get("color.line.strong")
       ColumnLayout {id:inner;anchors.fill:parent;anchors.margins:10;spacing:6
        UiText {text:card.modelData.title;font.pixelSize:17;color:Theme.get("color.text.primary")}
        RowLayout {spacing:8
         UiText {Layout.fillWidth:true;objectName:"setupFolder-"+card.modelData.id;font.pixelSize:page.body-1;elide:Text.ElideMiddle;wrapMode:Text.NoWrap
          text:"Your files:  "+(card.modelData.folder||(card.modelData.guess?card.modelData.guess+"   (found, used unless you change it)":"not chosen"))
          color:card.modelData.folder?page.good:card.modelData.guess?Theme.get("color.text.body"):page.warn}
         PillButton {text:"Choose folder";visible:!page.vrOverlayMode;onClicked:{page.pendingId=card.modelData.id;folder.open()}}
         PillButton {text:"Skip";visible:!!card.modelData.folder||!!card.modelData.guess;vrOverlayMode:page.vrOverlayMode;onClicked:{uiController.setSetupFolder(card.modelData.id,"");uiController.setSetupSystem(card.modelData.id,false)}}
        }
        RowLayout {spacing:8
         UiText {Layout.fillWidth:true;font.pixelSize:page.body-1;elide:Text.ElideMiddle;wrapMode:Text.NoWrap
          text:card.modelData.programLabel+":  "+(card.modelData.program||"not chosen")
          color:card.modelData.program?page.good:page.warn}
         PillButton {text:"Locate program";visible:!page.vrOverlayMode;onClicked:{page.pendingId=card.modelData.id;program.open()}}
        }
       }
      }
     }
    }
    ColumnLayout {spacing:10
     RowLayout {spacing:10
      UiText {Layout.fillWidth:true;text:"What you have";font.pixelSize:20;color:Theme.get("color.text.primary")}
      UiText {text:page.report.scanned?"Read on "+page.report.scanned:"";font.pixelSize:page.body-2;color:Theme.get("color.text.muted_detail")}
      PillButton {objectName:"setupRescan";text:uiController.setupScanning?"Reading…":"Rescan";enabled:!uiController.setupScanning;vrOverlayMode:page.vrOverlayMode;onClicked:uiController.scanSetup()}
     }
     Para {objectName:"setupProgress";visible:uiController.setupScanning;color:page.warn;text:"Reading your folders. Nothing in them is changed.  "+uiController.setupProgress}
     Para {visible:!uiController.setupScanning&&!(page.report.systems||[]).length;text:"Nothing has been read yet. Tick what you have, say where it is, then use Rescan."}
     Para {visible:(page.report.systems||[]).length>0;text:"Ready games can be started. Not right means the file was found but cannot be used as it is. Missing means the Hub knows the game but did not find it; only the name it looks for is shown."}
     Repeater {model:uiController.setupScanning?[]:(page.report.systems||[])
      Rectangle {id:result;required property var modelData;objectName:"setupResult-"+modelData.id;Layout.fillWidth:true;implicitHeight:lines.implicitHeight+20;radius:Theme.get("size.card.radius");color:Theme.get("color.surface.card");border.color:Theme.get("color.line.strong")
       ColumnLayout {id:lines;anchors.fill:parent;anchors.margins:10;spacing:6
        RowLayout {spacing:14
         UiText {text:result.modelData.title;font.pixelSize:17;color:Theme.get("color.text.primary")}
         UiText {text:"Ready "+result.modelData.ready.length;font.pixelSize:page.body;color:page.good}
         UiText {text:"Not right "+result.modelData.notRight.length;font.pixelSize:page.body;color:result.modelData.notRight.length?page.warn:Theme.get("color.text.muted_detail")}
         UiText {text:"Missing "+result.modelData.missing.length;font.pixelSize:page.body;color:Theme.get("color.text.muted_detail")}
         Item {Layout.fillWidth:true}
        }
        Para {visible:!!result.modelData.note;text:result.modelData.note||"";color:Theme.get("color.text.muted_detail")}
        Para {visible:result.modelData.ready.length>0;text:"Ready:  "+page.names_of(result.modelData.ready);color:page.good;font.pixelSize:page.body-1}
        Repeater {model:result.modelData.notRight
         Para {required property var modelData;text:"Not right:  "+modelData.title+".  "+modelData.reason;color:page.warn;font.pixelSize:page.body-1}
        }
        Para {visible:result.modelData.missing.length>0;font.pixelSize:page.body-1;color:Theme.get("color.text.muted_detail")
         text:"Missing:  "+Array.from(result.modelData.missing).map(e=>e.title+" ("+e.expected+")").join(",  ")}
       }
      }
     }
    }
   }
  }
  RowLayout {spacing:8
   PillButton {objectName:"setupBack";text:"Back";enabled:page.step>0;vrOverlayMode:page.vrOverlayMode;onClicked:page.go(page.step-1)}
   PillButton {objectName:"setupNext";text:page.step===page.names.length-1?"Finish":"Next";neon:true;accent:page.good;vrOverlayMode:page.vrOverlayMode;onClicked:page.go(page.step+1)}
   PillButton {objectName:"setupSkipAll";text:"Skip set-up";visible:page.step<page.names.length-1;vrOverlayMode:page.vrOverlayMode;onClicked:{uiController.finishSetup();page.finished()}}
   Item {Layout.fillWidth:true}
  }
 }
}
