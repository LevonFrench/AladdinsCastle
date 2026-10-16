// SPDX-License-Identifier: GPL-3.0-only
import QtQuick
import QtQuick.Controls
import QtQuick.Dialogs
import QtQuick.Layouts
import AladdinsCastle.Hub
ScrollView{
 id:page
 property bool vrOverlayMode:false
 readonly property real side:vrOverlayMode?28:40
 readonly property bool catalogOn:(uiSettings.values,uiSettings.get("showAllGames",false))===true
 readonly property bool artOn:(uiSettings.values,uiSettings.get("artDownload",false))===true
 readonly property var artFolders:(uiSettings.values,uiSettings.get("artFolders",[]))||[]
 FolderDialog {id:artFolderDialog;title:"Choose a folder of artwork";onAccepted:{const path=uiController.localPath(selectedFolder);if(page.artFolders.indexOf(path)<0)uiSettings.set("artFolders",page.artFolders.concat([path]))}}
 readonly property var artSystems:[{key:"model3",label:"Sega Model 3"},{key:"model2",label:"Sega Model 2"},{key:"system22",label:"Namco System 22"},{key:"wii",label:"Nintendo Wii"},{key:"dreamcast",label:"Sega Dreamcast"},{key:"ps2",label:"PlayStation 2"}]
 clip:true
 contentWidth:availableWidth
 Column{x:page.side;width:Math.min(page.availableWidth-2*page.side,880);spacing:12;topPadding:page.vrOverlayMode?20:28;bottomPadding:28
  Text{text:"Settings";color:Theme.get("color.text.primary");font.family:Theme.get("font.family");font.weight:900;font.pixelSize:page.vrOverlayMode?40:36}
  Text{width:parent.width;wrapMode:Text.Wrap;bottomPadding:8;text:"Settings stay in the Hub's own folder on this PC. Pictures come from this PC unless you turn on artwork downloads below.";color:Theme.get("color.text.muted");font.family:Theme.get("font.family");font.weight:500;font.pixelSize:page.vrOverlayMode?18:16}
  SettingRow{objectName:"settingShowAll";width:parent.width;vrOverlayMode:page.vrOverlayMode;on:page.catalogOn;label:"Show games without a setup"
   detail:"Off: only games that can be played on this PC are listed. On: the whole catalog comes back, with its filters, search and order.";onClicked:uiSettings.set("showAllGames",!on)}
  SettingRow{objectName:"settingArtDownload";width:parent.width;vrOverlayMode:page.vrOverlayMode;on:page.artOn;label:"Download artwork from the internet"
   detail:"Off: the Hub asks the internet for nothing and shows only pictures on this PC. On: Get artwork, on a game's page or for a whole system below, fetches box art, title screens and screenshots from the libretro thumbnail server, then from SteamGridDB if you add your own key. Nothing downloads by itself. Pictures are kept in the Hub's own folder, and your own pictures always come first.";onClicked:uiSettings.set("artDownload",!on)}
  Column{objectName:"artDownloadOptions";visible:page.artOn;width:parent.width;spacing:10;leftPadding:page.vrOverlayMode?20:16
   Text{text:"SteamGridDB key (optional)";color:Theme.get("color.text.primary");font.family:Theme.get("font.family");font.weight:700;font.pixelSize:page.vrOverlayMode?18:15}
   TextField{id:steamGridKey;objectName:"settingSteamGridKey";width:Math.min(parent.width-parent.leftPadding,520);height:page.vrOverlayMode?52:40;echoMode:TextInput.Password;selectByMouse:true
    placeholderText:"Your own key, from SteamGridDB: Preferences, API";Accessible.name:"SteamGridDB API key";text:(uiSettings.values,uiSettings.get("steamGridDbKey",""))
    color:Theme.get("color.text.primary");placeholderTextColor:Theme.get("color.text.faint");font.family:Theme.get("font.family");font.pixelSize:page.vrOverlayMode?16:14
    onEditingFinished:if(text.trim()!==uiSettings.get("steamGridDbKey",""))uiSettings.set("steamGridDbKey",text.trim())
    leftPadding:16;rightPadding:16
    background:Rectangle {radius:Theme.get("radius.pill");color:Theme.get("color.surface.pill");border.width:steamGridKey.activeFocus?2:0;border.color:Theme.get("color.glass.ring")}
    onActiveFocusChanged:if(activeFocus&&page.vrOverlayMode&&typeof overlayHost!=="undefined")overlayHost.requestKeyboard(steamGridKey)}
   Text{width:parent.width-parent.leftPadding;wrapMode:Text.Wrap;text:"Kept in the Hub's settings on this PC and sent only to SteamGridDB. Without a key, only libretro is asked.";color:Theme.get("color.text.muted");font.family:Theme.get("font.family");font.weight:500;font.pixelSize:page.vrOverlayMode?15:13}
   Flow{width:parent.width-parent.leftPadding;spacing:10;topPadding:4
    Repeater{model:page.artSystems
     PillButton{required property var modelData;objectName:"artGet_"+modelData.key;text:"Get artwork · "+modelData.label;vrOverlayMode:page.vrOverlayMode;enabled:!uiController.artworkBusy;onClicked:uiController.getArtwork("system:"+modelData.key)}}}
   UiText{objectName:"artworkStatus";width:parent.width-parent.leftPadding;visible:text.length>0;text:uiController.artworkStatus;font.pixelSize:page.vrOverlayMode?15:13}
  }
  // What went wrong lately: each crash the watchdog saw, newest first, kept in the Hub's own folder.
  Rectangle{objectName:"crashRecords";width:parent.width;radius:Theme.get("radius.card");color:Theme.get("color.surface.card");height:crashColumn.implicitHeight+(page.vrOverlayMode?40:32)
   Column{id:crashColumn;x:page.vrOverlayMode?20:16;y:page.vrOverlayMode?20:16;width:parent.width-2*x;spacing:10
    Text{text:"Crash records";color:Theme.get("color.text.primary");font.family:Theme.get("font.family");font.weight:700;font.pixelSize:page.vrOverlayMode?20:17}
    Text{width:parent.width;wrapMode:Text.Wrap;color:Theme.get("color.text.muted");font.family:Theme.get("font.family");font.weight:500;font.pixelSize:page.vrOverlayMode?16:14
     text:uiController.crashRecords.length?"The last "+Math.min(uiController.crashRecords.length,10)+" of the crashes the Hub saw, with the end of the emulator's own log where it keeps one. Kept in the Hub's folder on this PC.":"No crashes recorded. When a game the Hub started crashes, it is said here: when, which game, and why in words."}
    Repeater{model:uiController.crashRecords.slice(0,10)
     Column{required property var modelData;objectName:"crashRecord";width:crashColumn.width;spacing:4
      Text{width:parent.width;wrapMode:Text.Wrap;text:modelData.title+"  ·  "+String(modelData.time||"").replace("T"," ")+"  ·  "+modelData.meaning;color:Theme.get("color.state.needs_neon");font.family:Theme.get("font.family");font.weight:700;font.pixelSize:page.vrOverlayMode?16:14}
      Text{width:parent.width;visible:!!modelData.log;wrapMode:Text.WrapAnywhere;maximumLineCount:6;elide:Text.ElideRight;text:"From "+modelData.logFrom+":\n"+String(modelData.log||"").trim().split("\n").slice(-5).join("\n");color:Theme.get("color.text.muted");font.family:"Consolas";font.pixelSize:page.vrOverlayMode?14:12}
     }}
   }
  }
  // Pictures already on this PC: read where they are, never copied, and always before downloads.
  Rectangle{objectName:"artFolders";width:parent.width;radius:Theme.get("radius.card");color:Theme.get("color.surface.card");height:artFolderColumn.implicitHeight+(page.vrOverlayMode?40:32)
   Column{id:artFolderColumn;x:page.vrOverlayMode?20:16;y:page.vrOverlayMode?20:16;width:parent.width-2*x;spacing:8
    Text{text:"Folders of artwork you already have";color:Theme.get("color.text.primary");font.family:Theme.get("font.family");font.weight:700;font.pixelSize:page.vrOverlayMode?20:17}
    Text{width:parent.width;wrapMode:Text.Wrap;color:Theme.get("color.text.muted");font.family:Theme.get("font.family");font.weight:500;font.pixelSize:page.vrOverlayMode?16:14
     text:"Marquee, flyer or box-art packs on this PC, matched by set name or disc name. The Hub only reads them where they are: nothing is copied and nothing leaves this PC. They always come before downloaded pictures."+(page.artFolders.length?"  "+uiController.localArtFiles+(uiController.localArtFiles===1?" picture found.":" pictures found."):"")}
    Repeater{model:page.artFolders
     RowLayout{required property string modelData;width:artFolderColumn.width;spacing:10
      UiText{Layout.fillWidth:true;text:modelData;elide:Text.ElideMiddle;font.pixelSize:page.vrOverlayMode?15:13}
      PillButton{objectName:"artFolderRemove";text:"Remove";vrOverlayMode:page.vrOverlayMode;onClicked:uiSettings.set("artFolders",page.artFolders.filter(f=>f!==modelData))}}}
    PillButton{objectName:"artFolderAdd";visible:!page.vrOverlayMode;vrOverlayMode:page.vrOverlayMode;text:"Add a folder…";onClicked:artFolderDialog.open()}
    UiText{visible:page.vrOverlayMode;text:"Add folders on the desktop.";font.pixelSize:15}
   }
  }
  Repeater{model:[{key:"reduceMotion",label:"Reduce motion",detail:"Rows and pictures change without moving."},{key:"cheapEffects",label:"Use cheaper card effects",detail:"Plainer cards, lighter on the graphics card."},{key:"scanOnStartup",label:"Scan remembered folders on startup",catalog:true},{key:"bannerListDisabled",label:"Hide list banner",catalog:true},{key:"bannerLibDisabled",label:"Hide library banner",catalog:true},{key:"recentlyPlayedHidden",label:"Hide recently played",catalog:true}].filter(m=>!m.catalog||page.catalogOn)
   SettingRow{required property var modelData;width:parent.width;vrOverlayMode:page.vrOverlayMode;label:modelData.label;detail:modelData.detail||""
    on:Object.keys(uiSettings.values).length>=0&&(uiSettings.values,uiSettings.get(modelData.key,modelData.key==="cheapEffects"))===true;onClicked:uiSettings.set(modelData.key,!on)}}
  Text{visible:page.catalogOn;width:parent.width;topPadding:12;text:"Catalog warnings ("+gameModel.warnings.length+")";color:Theme.get("color.text.primary");font.family:Theme.get("font.family");font.weight:700;font.pixelSize:page.vrOverlayMode?20:17}
  UiText{visible:page.catalogOn;width:parent.width;text:gameModel.warnings.join("\n");font.pixelSize:12}
 }
}
