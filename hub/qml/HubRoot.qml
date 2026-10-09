// SPDX-License-Identifier: GPL-3.0-only
import QtQuick
import QtQuick.Controls
import QtQuick.Dialogs
import QtQuick.Layouts
import AladdinsCastle.Hub
FocusScope {
 id:root
 objectName:"hubRoot"
 property string view:"List"
 property var history:[]
 property bool vrOverlayMode:false
 property alias overlayPresentation: root.vrOverlayMode
 property bool reduceMotion:(uiSettings.values,uiSettings.get("reduceMotion",false))
 property string sizeValue:Object.keys(uiSettings.values).length>=0 ? (uiSettings.values,uiSettings.get("size"+view,vrOverlayMode?"L":"S")) : "S"
 property var previewGame:({})
 function navigate(page,id){history=history.concat([view]);view=page;if(id)uiController.openDetail(id);preview.visible=false}
 function back(){if(history.length){view=history[history.length-1];history=history.slice(0,-1)}else view="List";preview.visible=false}
 function closeTop(){if(filters.opened)filters.close();else if(sort.opened)sort.close();else if(help.opened)help.close();else if(gameFilter.query.length)gameFilter.query="";else back()}
 function showDetail(id){navigate("Detail",id)}
 Rectangle{anchors.fill:parent;color:Theme.get("color.surface.window")}
 Canvas{anchors.fill:parent;layer.enabled:true;onPaint:{let c=getContext("2d");c.fillStyle=Theme.get("color.surface.dot_grid");for(let x=0;x<width;x+=24)for(let y=0;y<height;y+=24){c.beginPath();c.arc(x,y,0.9,0,2*Math.PI);c.fill()}} onWidthChanged:requestPaint();onHeightChanged:requestPaint()}
 ColumnLayout{anchors.fill:parent;spacing:0
  Header{id:header;Layout.fillWidth:true;Layout.fillHeight:false;Layout.preferredHeight:implicitHeight;Layout.maximumHeight:implicitHeight;vrOverlayMode:root.vrOverlayMode;onLibraryRequested:root.navigate(root.view==="Library"?"List":"Library");onHelpRequested:{sort.close();filters.close();help.open()} onOrderRequested:{help.close();filters.close();sort.open()} onQueryEdited:text=>{gameFilter.query=text;if(root.view==="Detail")root.view="List"}}
  FilterBar{Layout.fillWidth:true;Layout.fillHeight:false;Layout.preferredHeight:implicitHeight;Layout.maximumHeight:implicitHeight;vrOverlayMode:root.vrOverlayMode;sizeValue:root.sizeValue;onBackPressed:root.back();onFiltersRequested:{sort.close();help.close();filters.open()} onScanRequested:if(uiController.scanning)uiController.scan([]);else scanDialog.open();onSizeChosen:value=>uiSettings.set("size"+root.view,value)}
  Item{id:content;Layout.fillWidth:true;Layout.fillHeight:true;Layout.minimumHeight:0;Layout.preferredHeight:0;clip:true
   GameGrid{id:grid;anchors.fill:parent;visible:root.view==="List"||root.view==="Library";view:root.view;sizeValue:root.sizeValue;vrOverlayMode:root.vrOverlayMode;reduceMotion:root.reduceMotion;effectsEnabled:!(uiSettings.values,uiSettings.get("cheapEffects",true));onDetailRequested:id=>root.showDetail(id);onActionRequested:id=>{root.showDetail(id);uiController.primary(id)} onExploreRequested:root.navigate("Explore");onPreviewRequested:(game,item)=>{if(root.vrOverlayMode)return;root.previewGame=game;let p=item.mapToItem(content,0,0);preview.x=Math.max(0,Math.min(content.width-preview.width,p.x-(preview.width-item.width)/2));preview.y=Math.max(0,Math.min(content.height-preview.height,p.y-(preview.height-item.height)/2));preview.visible=true} onPreviewClosed:preview.visible=false}
   Loader{anchors.fill:parent;anchors.margins:24;active:root.view==="Detail";sourceComponent:Component{DetailPage{vrOverlayMode:root.vrOverlayMode;sizeValue:root.sizeValue;onDetailRequested:id=>uiController.openDetail(id)}}}
   Loader{anchors.fill:parent;anchors.margins:20;active:root.view==="Explore";sourceComponent:Component{ExplorePage{sizeValue:root.sizeValue;vrOverlayMode:root.vrOverlayMode;onDetailRequested:id=>root.showDetail(id)}}}
   Loader{anchors.fill:parent;anchors.margins:24;active:root.view==="Settings";sourceComponent:Component{SettingsPage{vrOverlayMode:root.vrOverlayMode}}}
   Item{id:preview;objectName:"previewHost";visible:false;width:previewCard.implicitWidth*Theme.get("motion.preview_scale");height:previewCard.implicitHeight*Theme.get("motion.preview_scale");z:1000
    GameCard{id:previewCard;game:root.previewGame;preview:true;sizeValue:root.sizeValue;scale:Theme.get("motion.preview_scale");transformOrigin:Item.TopLeft;onDetailRequested:id=>root.showDetail(id);onActionRequested:id=>{root.showDetail(id);uiController.primary(id)}}
    HoverHandler{id:previewHover;onHoveredChanged:if(!hovered)preview.visible=false}
   }
  }
  PillButton{objectName:"globalStop";text:typeof hubServices!=="undefined"&&hubServices.preparing?"Stop preparing launch":"Stop game";visible:typeof hubServices!=="undefined"&&hubServices.launchBusy;vrOverlayMode:root.vrOverlayMode;onClicked:uiController.stopLaunch()}
  UiText{Layout.fillWidth:true;Layout.leftMargin:14;Layout.bottomMargin:6;text:uiController.status||gameFilter.visibleCount+" / "+catalogGameCount+" games · Local art only";font.pixelSize:11;color:Theme.get("color.text.muted_detail")}
 }
 FiltersDrawer{id:filters;parent:root;x:Math.max(0,Math.min(root.width-width,160));y:Math.min(140,root.height-height);vrOverlayMode:root.vrOverlayMode}
 SortMenu{id:sort;parent:root;x:Math.max(0,root.width-width-180);y:60;vrOverlayMode:root.vrOverlayMode}
 HelpPanel{id:help;parent:root;x:Math.max(0,root.width-width-200);y:60;onSettingsRequested:root.navigate("Settings")}
 Dialog{id:scanDialog;parent:root;anchors.centerIn:parent;width:Math.min(root.width-32,520);modal:true;title:"Scan my files";standardButtons:Dialog.Ok|Dialog.Cancel
  Column{width:parent.width;spacing:10;UiText{width:parent.width;text:"Choose folders containing your own game files. The scanner identifies media locally; no game content is downloaded."} TextField{id:roots;objectName:"scanRoots";width:parent.width;placeholderText:"Folders separated by ;";text:(uiSettings.values,uiSettings.get("scanRoots",""));color:Theme.get("color.text.primary");background:Rectangle{color:Theme.get("color.surface.pill");border.color:Theme.get("color.line.button");radius:4}} PillButton{text:"Browse folder";onClicked:folder.open()}}
  onAccepted:{uiSettings.set("scanRoots",roots.text);uiController.scan(roots.text.split(/\n|;/).filter(p=>p.trim().length>0))}
  background:Rectangle{color:Theme.get("color.surface.panel");border.color:Theme.get("color.line.button");radius:8}
 }
 Connections {target:uiController;function onLocationRequested(kind){if(kind==="tool")toolDialog.open();else if(kind==="media")scanDialog.open();else if(kind==="steam-shortcut"&&typeof hubServices!=="undefined"){hubServices.beginSteam(uiController.detail.gameId,uiController.detail.variantId);steamDialog.open()}}}
 Dialog {id:toolDialog;parent:root;anchors.centerIn:parent;width:Math.min(root.width-32,600);modal:true;title:"Emulators";standardButtons:Dialog.Close
  Column {width:parent.width;spacing:12
   UiText {width:parent.width;text:"Locate your existing MAME or PCSX2 folder with Find my files. Supermodel can be installed from its pinned official release.";wrapMode:Text.Wrap}
   PillButton {text:"Find my files / emulator folders";onClicked:{toolDialog.close();scanDialog.open()}}
   ScrollView {width:parent.width;height:180;TextArea {text:typeof hubServices!=="undefined"?hubServices.toolPlan:"";readOnly:true;wrapMode:Text.Wrap;color:Theme.get("color.text.primary");font.pixelSize:12}}
   PillButton {text:"Install Supermodel";enabled:typeof hubServices!=="undefined"&&!uiController.installing;onClicked:{hubServices.installSupermodel();toolDialog.close()}}
   PillButton {text:"Preview Supermodel removal";enabled:typeof hubServices!=="undefined"&&!uiController.installing;onClicked:{hubServices.previewSupermodelRemoval();toolDialog.close()}}
  }
 }
 Dialog {id:steamDialog;parent:root;anchors.centerIn:parent;width:Math.min(root.width-32,680);modal:true;title:typeof hubServices!=="undefined"&&hubServices.steamRemoving?"Review Steam shortcut removal":"Review Steam shortcut changes";standardButtons:Dialog.Close
  Column {width:parent.width;spacing:12
   UiText {width:parent.width;wrapMode:Text.Wrap;text:"Choose the account folder and review the exact files below. Save authorizes this displayed change. Steam must be fully closed; custom art is preserved."}
   ComboBox {id:steamAccount;model:typeof hubServices!=="undefined"?hubServices.steamAccounts:[];width:parent.width}
   PillButton {text:"Preview this account";enabled:steamAccount.currentIndex>=0;onClicked:hubServices.previewSteam(steamAccount.currentText)}
   ScrollView {width:parent.width;height:260;TextArea {text:typeof hubServices!=="undefined"?hubServices.steamPreview:"";readOnly:true;wrapMode:Text.Wrap;color:Theme.get("color.text.primary");font.pixelSize:11}}
   PillButton {text:typeof hubServices!=="undefined"&&hubServices.steamRemoving?"Approve the reviewed Steam removal":"Save the reviewed Steam shortcut";enabled:typeof hubServices!=="undefined"&&hubServices.steamWriteReady;onClicked:hubServices.approveSteamWrite()}
  }
 }
 Dialog {id:removeDialog;parent:root;anchors.centerIn:parent;width:Math.min(root.width-32,600);modal:true;title:"Remove owned installation files?";standardButtons:Dialog.Ok|Dialog.Cancel
  onOpened:removeSteam.selected=false
  Column {width:parent.width;spacing:10
  ScrollView {width:parent.width;height:240;TextArea {text:typeof hubServices!=="undefined"?hubServices.removalPlan:"";readOnly:true;wrapMode:Text.Wrap;color:Theme.get("color.text.primary")}}
  PillButton {id:removeSteam;objectName:"removeSteamOption";text:"Also review Steam entry removal (separate approval)";visible:typeof hubServices!=="undefined"&&hubServices.canRemoveSteam;selected:false;onClicked:selected=!selected}
  UiText {width:parent.width;wrapMode:Text.Wrap;text:"Steam changes require a separate exact preview and approval. Close Steam yourself before applying them."}
  }
  onAccepted:if(hubServices.confirmRemoval(removeSteam.selected))steamDialog.open()
 }
 Connections {target:typeof hubServices!=="undefined"?hubServices:null;function onRemovalPlanChanged(){removeDialog.open()}}
 FolderDialog{id:folder;title:"Choose a folder to scan";onAccepted:roots.text+=(roots.text.length?";":"")+uiController.localPath(selectedFolder)}
 Shortcut{sequence:"Ctrl+K";onActivated:header.focusSearch()}
 Shortcut{sequence:"/";enabled:!header.searchField.activeFocus;onActivated:header.focusSearch()}
 Shortcut{sequence:"Escape";onActivated:root.closeTop()}
 Shortcut{sequence:"Alt+Left";onActivated:root.back()}
 Connections{target:uiSettings;function onChanged(){root.previewGame=root.previewGame}}
 Component.onCompleted:if((uiSettings.values,uiSettings.get("scanOnStartup",false))&&(uiSettings.values,uiSettings.get("scanRoots","")).length)uiController.scan((uiSettings.values,uiSettings.get("scanRoots","")).split(/\n|;/))
}
