// SPDX-License-Identifier: GPL-3.0-only
import QtQuick
import QtQuick.Controls
import QtQuick.Dialogs
import QtQuick.Layouts
import AladdinsCastle.Hub
FocusScope {
 id:root
 objectName:"hubRoot"
 // Diagnostics and shortcuts may name a first page, and an item on it: "Model3" or "Model3/scud".
 property var startAt:typeof startView!=="undefined"&&startView?startView.split("/"):[]
 // Off by default: only games with a working setup on this PC are shown. The catalog, its filters,
 // search, sort and catalog-only dialogs come back with Settings, "Show games without a setup".
 property bool showAll:(typeof forceShowAll!=="undefined"&&forceShowAll)||(uiSettings.values,uiSettings.get("showAllGames",false))===true
 // The SteamVR panel always comes home to its library; the desktop to the catalog when it is shown.
 property string home:showAll&&!vrOverlayMode?"List":"Model3"
 readonly property var catalogPages:["List","Library","Explore","Detail","GunRoom"]
 onShowAllChanged:Qt.callLater(leaveCatalog)
 function leaveCatalog(){if(!showAll&&catalogPages.indexOf(view)>=0){history=[];view=home}}
 property string view:startAt.length?startAt[0]:((typeof forceShowAll!=="undefined"&&forceShowAll)||uiSettings.get("showAllGames",false)===true?"List":"Model3")
 property var history:[]
 // Drawn as SteamVR's dashboard panel (set by the overlay renderer); the panel look follows it.
 property bool overlayPresentation:false
 property bool vrOverlayMode:overlayPresentation
 property bool captureMarkers:false
 property bool reduceMotion:(uiSettings.values,uiSettings.get("reduceMotion",false))
 property string sizeValue:Object.keys(uiSettings.values).length>=0 ? (uiSettings.values,uiSettings.get("size"+view,vrOverlayMode?"L":"S")) : "S"
 property var previewGame:({})
 Connections {target:header;function onPinballRequested(){root.navigate("Pinball")} function onModel3Requested(){root.navigate("Model3")}}
 onActiveFocusChanged:if(!activeFocus)uiController.clearDetailControlStates()
 // Any page change closes every menu and dialog, so nothing is left open over the new page.
 function closeMenus(){filters.close();sort.close();help.close();scanDialog.close();toolDialog.close();steamDialog.close();removeDialog.close();preview.visible=false}
 function navigate(page,id){closeMenus();history=history.concat([view]);view=page;if(id)uiController.openDetail(id)}
 function back(){closeMenus();if(history.length){view=history[history.length-1];history=history.slice(0,-1)}else view=home}
 function closeTop(){for(const open of [removeDialog,steamDialog,toolDialog,scanDialog,filters,sort,help])if(open.opened){open.close();return}
  if(showAll&&gameFilter.query.length)gameFilter.query="";else back()}
 function showDetail(id){navigate("Detail",id)}
 // The panel's guards against double presses. After any page change, trigger clicks are ignored for a moment,
 // so a double click on a tile cannot also press the Play button that appears under the pointer. B presses
 // inside a page change are dropped, so one burst of presses never goes back two levels at once.
 property real lastPageChange:0
 property int backSteps:0
 readonly property int backQuietMs:250
 // The click guard is for the SteamVR panel's trigger; the desktop mouse is left alone.
 // Presses on the panel before this time land nowhere. A clock, not a Timer: the panel draws only when something
 // changes, and a QML Timer can stall with it, which would keep the guard up and eat the next real click.
 property real clickGuardUntil:0
 readonly property int clickGuardMs:300
 function pageChanged(){lastPageChange=Date.now();if(overlayPresentation)clickGuardUntil=lastPageChange+clickGuardMs}
 onViewChanged:pageChanged()
 // The kept panel library belongs to the panel look only.
 onVrOverlayModeChanged:model3Loader.keep=false
 readonly property var popups:[removeDialog,steamDialog,toolDialog,scanDialog,filters,sort,help]
 readonly property var pageItem:model3Loader.item
 readonly property bool canGoBack:popups.some(p=>p.opened)||history.length>0||view!==home||(view==="Model3"&&!!pageItem&&pageItem.canGoBack===true)
 // B on the controller: back exactly one level. An open menu or dialog closes; then the page's own level
 // (game page to the grid, a tab or search to the whole grid); then the page before. At the top it does
 // nothing, and it never quits. While a game starts or runs its card is the page, and B leaves it alone.
 function panelBack(){
  if(Date.now()-lastPageChange<backQuietMs)return false
  let done=false
  for(const open of popups)if(open.opened){open.close();done=true;break}
  if(!done&&(uiController.nowPlaying.gameId!==undefined||!!uiController.steamVrStarting))return false
  if(!done&&view==="Model3"&&pageItem&&typeof pageItem.handleBack==="function")done=pageItem.handleBack()
  if(!done&&(history.length>0||view!==home)){back();done=true}
  if(done){++backSteps;pageChanged()}
  return done
 }
 Connections {target:typeof overlayHost!=="undefined"?overlayHost:null;ignoreUnknownSignals:true;function onBackRequested(){if(root.overlayPresentation)root.panelBack()}}
 Loader{anchors.fill:parent;z:10000;active:root.captureMarkers;sourceComponent:Component{OverlayCaptureMarkers{}}}
 Rectangle{anchors.fill:parent;color:Theme.get("color.surface.window")}
 Canvas{anchors.fill:parent;layer.enabled:true;onPaint:{let c=getContext("2d");c.fillStyle=Theme.get("color.surface.dot_grid");for(let x=0;x<width;x+=24)for(let y=0;y<height;y+=24){c.beginPath();c.arc(x,y,0.9,0,2*Math.PI);c.fill()}} onWidthChanged:requestPaint();onHeightChanged:requestPaint()}
 ColumnLayout{anchors.fill:parent;spacing:0
  Header{id:header;visible:!(root.vrOverlayMode&&root.view==="Model3");Layout.fillWidth:true;Layout.fillHeight:false;Layout.preferredHeight:visible?implicitHeight:0;Layout.maximumHeight:visible?implicitHeight:0;vrOverlayMode:root.vrOverlayMode;showAll:root.showAll;view:root.view;onLibraryRequested:root.navigate(root.view==="Library"?"List":"Library");onHelpRequested:{sort.close();filters.close();help.open()} onOrderRequested:{help.close();filters.close();sort.open()} onQueryEdited:text=>{gameFilter.query=text;if(root.view==="Detail")root.view="List"}}
  FilterBar{objectName:"filterBar";visible:root.showAll&&["List","Library","Explore","Detail"].indexOf(root.view)>=0;Layout.fillWidth:true;Layout.fillHeight:false;Layout.preferredHeight:visible?implicitHeight:0;Layout.maximumHeight:visible?implicitHeight:0;vrOverlayMode:root.vrOverlayMode;sizeValue:root.sizeValue;onBackPressed:root.back();onFiltersRequested:{sort.close();help.close();filters.open()} onScanRequested:if(uiController.scanning)uiController.cancelScan();else scanDialog.open();onSizeChosen:value=>uiSettings.set("size"+root.view,value)}
  Item{id:content;Layout.fillWidth:true;Layout.fillHeight:true;Layout.minimumHeight:0;Layout.preferredHeight:0;clip:true
   NowPlayingCard {anchors.fill:parent;vrOverlayMode:root.vrOverlayMode;z:900;visible:uiController.nowPlaying.gameId!==undefined}
   SteamVrStartingCard {objectName:"steamVrStartingCard";anchors.fill:parent;vrOverlayMode:root.vrOverlayMode;z:901;focus:visible}
   // How the last game ended, until dismissed or the next one starts: closed, stopped, or why it crashed.
   Rectangle {id:ended;objectName:"lastSession";z:850;anchors.left:parent.left;anchors.right:parent.right;anchors.bottom:parent.bottom;anchors.margins:root.vrOverlayMode?20:24;anchors.leftMargin:root.vrOverlayMode?28:40;anchors.rightMargin:root.vrOverlayMode?28:40
    property var session:uiController.lastSession;property bool trouble:session.outcome==="crashed"||session.outcome==="failed"
    visible:session.text!==undefined&&uiController.nowPlaying.gameId===undefined;height:visible?Math.max(endedText.implicitHeight+32,root.vrOverlayMode?80:68):0;radius:Theme.get("radius.card")
    color:Theme.get("color.surface.pill");border.width:0;clip:true
    // How it ended, at a glance: amber when it went wrong.
    Rectangle {width:6;height:parent.height;color:ended.trouble?Theme.get("color.state.needs_neon"):Theme.get("color.text.dim")}
    RowLayout {anchors.fill:parent;anchors.margins:16;anchors.leftMargin:24;spacing:14
     UiText {id:endedText;objectName:"lastSessionText";Layout.fillWidth:true;font.pixelSize:root.vrOverlayMode?18:14;color:ended.trouble?Theme.get("color.state.needs_neon"):Theme.get("color.text.primary")
      text:(ended.session.text||"")+(ended.session.details?"\n"+ended.session.details:"")}
     PillButton {objectName:"lastSessionDismiss";text:"Dismiss";vrOverlayMode:root.vrOverlayMode;onClicked:uiController.dismissLastSession()}
    }
   }
   Loader {anchors.fill:parent;active:root.view==="Pinball";focus:active;sourceComponent:Component{PinballPage{vrOverlayMode:root.vrOverlayMode;startItem:root.startAt.length>1&&root.startAt[0]==="Pinball"?root.startAt[1]:""}}}
   Loader {anchors.fill:parent;active:root.view==="Setup";sourceComponent:Component{SetupPage{vrOverlayMode:root.vrOverlayMode;step:root.startAt.length>1&&root.startAt[0]==="Setup"?Number(root.startAt[1]):0;onFinished:{root.history=[];root.view=root.home}}}}
   // The SteamVR panel shows its Steam-like library; the desktop keeps the VR games page. The panel's
   // library stays loaded behind other pages, so B comes back to the same tab and scroll position.
   Loader {id:model3Loader;objectName:"model3Loader";anchors.fill:parent;property bool keep:false;active:root.view==="Model3"||keep;visible:root.view==="Model3";focus:root.view==="Model3"
    sourceComponent:root.vrOverlayMode?panelLibrary:model3Page;onLoaded:keep=root.vrOverlayMode}
   Component {id:model3Page;Model3Page{onSetupRequested:root.navigate("Setup");onDetailRequested:id=>root.showDetail(id);vrOverlayMode:root.vrOverlayMode;sizeValue:root.sizeValue;selectedId:root.startAt.length>1&&root.startAt[0]==="Model3"?root.startAt[1]:""}}
   Component {id:panelLibrary;PanelLibrary{vrOverlayMode:root.vrOverlayMode;reduceMotion:root.reduceMotion;selectedId:root.startAt.length>1&&root.startAt[0]==="Model3"?root.startAt[1]:""
    onSetupRequested:root.navigate("Setup");onDetailRequested:id=>root.showDetail(id);onPinballRequested:root.navigate("Pinball");onHelpRequested:{sort.close();filters.close();help.open()} onPageChanged:root.pageChanged()}}
   Loader {anchors.fill:parent;anchors.margins:24;active:root.view==="GunRoom"&&!root.vrOverlayMode;sourceComponent:Component{GunRoomPage{}}}
   GameGrid{id:grid;anchors.fill:parent;visible:root.view==="List"||root.view==="Library";view:root.view;sizeValue:root.sizeValue;vrOverlayMode:root.vrOverlayMode;reduceMotion:root.reduceMotion;effectsEnabled:!(uiSettings.values,uiSettings.get("cheapEffects",true));onDetailRequested:id=>root.showDetail(id);onActionRequested:id=>{root.showDetail(id);uiController.primary(id)} onExploreRequested:root.navigate("Explore");onPreviewRequested:(game,item)=>{if(root.vrOverlayMode)return;root.previewGame=game;let p=item.mapToItem(content,0,0);preview.x=Math.max(0,Math.min(content.width-preview.width,p.x-(preview.width-item.width)/2));preview.y=Math.max(0,Math.min(content.height-preview.height,p.y-(preview.height-item.height)/2));preview.visible=true} onPreviewClosed:preview.visible=false}
   Loader{anchors.fill:parent;anchors.margins:24;active:root.view==="Detail";sourceComponent:Component{DetailPage{vrOverlayMode:root.vrOverlayMode;sizeValue:root.sizeValue;onDetailRequested:id=>uiController.openDetail(id)}}}
   Loader{anchors.fill:parent;anchors.margins:20;active:root.view==="Explore";sourceComponent:Component{ExplorePage{sizeValue:root.sizeValue;vrOverlayMode:root.vrOverlayMode;onDetailRequested:id=>root.showDetail(id)}}}
   Loader{anchors.fill:parent;active:root.view==="Settings";sourceComponent:Component{SettingsPage{vrOverlayMode:root.vrOverlayMode}}}
   Item{id:preview;objectName:"previewHost";visible:false;width:previewCard.implicitWidth*Theme.get("motion.preview_scale");height:previewCard.implicitHeight*Theme.get("motion.preview_scale");z:1000
    GameCard{id:previewCard;vrOverlayMode:root.vrOverlayMode;game:root.previewGame;preview:true;sizeValue:root.sizeValue;scale:Theme.get("motion.preview_scale");transformOrigin:Item.TopLeft;onDetailRequested:id=>root.showDetail(id);onActionRequested:id=>{root.showDetail(id);uiController.primary(id)}}
    HoverHandler{id:previewHover;onHoveredChanged:if(!hovered)preview.visible=false}
   }
  }
  BackgroundStrip{objectName:"backgroundStrip";Layout.fillWidth:true;Layout.preferredHeight:implicitHeight;Layout.bottomMargin:visible?6:0;vrOverlayMode:root.vrOverlayMode}
  PillButton{objectName:"globalStop";Layout.leftMargin:root.vrOverlayMode?28:40;Layout.bottomMargin:6;targetHeight:root.vrOverlayMode?56:44;neon:true;accent:Theme.get("color.state.uninstall");text:typeof hubServices!=="undefined"&&hubServices.gunRoomBusy?"Close gun room":typeof hubServices!=="undefined"&&hubServices.preparing?"Stop preparing launch":"Stop game";visible:typeof hubServices!=="undefined"&&hubServices.launchBusy;vrOverlayMode:root.vrOverlayMode;onClicked:uiController.stopLaunch()}
  RowLayout {Layout.fillWidth:true;Layout.leftMargin:root.vrOverlayMode?28:40;Layout.rightMargin:root.vrOverlayMode?28:40;Layout.bottomMargin:6;spacing:14
   // On every panel page: what B does. It is a button too, for a laser.
   Rectangle {id:backHint;objectName:"panelBackHint";visible:root.vrOverlayMode;Layout.preferredHeight:44;Layout.preferredWidth:hintRow.implicitWidth+28;radius:22
    color:Theme.get("color.surface.pill");opacity:root.canGoBack?1:0.45;Accessible.name:"Back";Accessible.role:Accessible.Button
    Row {id:hintRow;anchors.centerIn:parent;spacing:10
     Rectangle {width:30;height:30;radius:15;anchors.verticalCenter:parent.verticalCenter;color:Theme.get("color.text.primary")
      Text {anchors.centerIn:parent;text:"B";color:Theme.get("color.surface.window");font.family:Theme.get("font.family");font.weight:900;font.pixelSize:17}}
     Text {anchors.verticalCenter:parent.verticalCenter;text:"Back";color:Theme.get("color.text.primary");font.family:Theme.get("font.family");font.weight:700;font.pixelSize:18}
    }
    MouseArea {anchors.fill:parent;onClicked:root.panelBack()}
   }
   UiText{objectName:"statusLine";Layout.fillWidth:true;text:uiController.status||(root.showAll?gameFilter.visibleCount+" / "+catalogGameCount+" games":"");font.pixelSize:root.vrOverlayMode?16:11;elide:Text.ElideRight;maximumLineCount:2;color:Theme.get("color.text.muted_detail")}
  }
 }
 FiltersDrawer{id:filters;parent:root;x:Math.max(0,Math.min(root.width-width,160));y:Math.min(140,root.height-height);vrOverlayMode:root.vrOverlayMode}
 SortMenu{id:sort;parent:root;x:Math.max(0,root.width-width-180);y:60;vrOverlayMode:root.vrOverlayMode}
 HelpPanel{vrOverlayMode:root.vrOverlayMode;showAll:root.showAll;id:help;parent:root;x:Math.max(0,root.width-width-200);y:60;onSettingsRequested:root.navigate("Settings");onSetupRequested:root.navigate("Setup");onGunRoomRequested:root.navigate("GunRoom")}
 Dialog{id:scanDialog;objectName:"scanDialog";parent:root;anchors.centerIn:parent;width:Math.min(root.width-32,520);modal:true;focus:true;title:"Scan my files";standardButtons:Dialog.Ok|Dialog.Cancel
  footer:DialogButtonBox{standardButtons:scanDialog.standardButtons;delegate:PillButton{vrOverlayMode:root.vrOverlayMode}}
  Column{width:parent.width;spacing:10;UiText{width:parent.width;text:"Choose folders containing your own game files. The scanner identifies media locally; no game content is downloaded."} TextField{id:roots;objectName:"scanRoots";width:parent.width;placeholderText:"Folders separated by ;";text:(uiSettings.values,uiSettings.get("scanRoots",""));color:Theme.get("color.text.primary");background:Rectangle{color:Theme.get("color.surface.pill");border.color:Theme.get("color.line.button");radius:4}} PillButton{vrOverlayMode:root.vrOverlayMode;text:"Browse folder";onClicked:folder.open()}}
  onAccepted:{uiSettings.set("scanRoots",roots.text);uiController.startScan(roots.text.split(/\n|;/).filter(p=>p.trim().length>0))}
  background:Rectangle{color:Theme.get("color.surface.panel");border.color:Theme.get("color.line.button");radius:8}
 }
 Connections {target:uiController;function onLocationRequested(kind){if(kind==="tool")toolDialog.open();else if(kind==="media")scanDialog.open();else if(kind==="steam-shortcut"&&typeof hubServices!=="undefined"){hubServices.beginSteam(uiController.detail.gameId,uiController.detail.variantId);steamDialog.open()}}}
 Dialog {id:toolDialog;objectName:"toolDialog";parent:root;anchors.centerIn:parent;width:Math.min(root.width-32,600);modal:true;focus:true;title:"Emulators";standardButtons:Dialog.Close
  footer:DialogButtonBox{standardButtons:toolDialog.standardButtons;delegate:PillButton{vrOverlayMode:root.vrOverlayMode}}
  Column {width:parent.width;spacing:12
   UiText {width:parent.width;text:"Locate your existing MAME or PCSX2 folder with Find my files. Supermodel can be installed from its pinned official release.";wrapMode:Text.Wrap}
   PillButton {vrOverlayMode:root.vrOverlayMode;text:"Find my files / emulator folders";onClicked:{toolDialog.close();scanDialog.open()}}
   ScrollView {width:parent.width;height:180;TextArea {text:typeof hubServices!=="undefined"?hubServices.toolPlan:"";readOnly:true;wrapMode:Text.Wrap;color:Theme.get("color.text.primary");font.pixelSize:12}}
   PillButton {vrOverlayMode:root.vrOverlayMode;text:"Install Supermodel";enabled:typeof hubServices!=="undefined"&&!uiController.installing;onClicked:{hubServices.installSupermodel();toolDialog.close()}}
   PillButton {vrOverlayMode:root.vrOverlayMode;text:"Preview Supermodel removal";enabled:typeof hubServices!=="undefined"&&!uiController.installing;onClicked:{hubServices.previewSupermodelRemoval();toolDialog.close()}}
  }
 }
 Dialog {id:steamDialog;objectName:"steamDialog";parent:root;anchors.centerIn:parent;width:Math.min(root.width-32,680);modal:true;focus:true;title:typeof hubServices!=="undefined"&&hubServices.steamRemoving?"Review Steam shortcut removal":"Review Steam shortcut changes";standardButtons:Dialog.Close
  footer:DialogButtonBox{standardButtons:steamDialog.standardButtons;delegate:PillButton{vrOverlayMode:root.vrOverlayMode}}
  Column {width:parent.width;spacing:12
   UiText {width:parent.width;wrapMode:Text.Wrap;text:"Choose the account folder and review the exact files below. Save authorizes this displayed change. Steam must be fully closed; custom art is preserved."}
   ComboBox {id:steamAccount;implicitHeight:root.vrOverlayMode?56:40;model:typeof hubServices!=="undefined"?hubServices.steamAccounts:[];width:parent.width}
   PillButton {vrOverlayMode:root.vrOverlayMode;text:"Preview this account";enabled:steamAccount.currentIndex>=0;onClicked:hubServices.previewSteam(steamAccount.currentText)}
   ScrollView {width:parent.width;height:260;TextArea {text:typeof hubServices!=="undefined"?hubServices.steamPreview:"";readOnly:true;wrapMode:Text.Wrap;color:Theme.get("color.text.primary");font.pixelSize:11}}
   PillButton {vrOverlayMode:root.vrOverlayMode;text:typeof hubServices!=="undefined"&&hubServices.steamRemoving?"Approve the reviewed Steam removal":"Save the reviewed Steam shortcut";enabled:typeof hubServices!=="undefined"&&hubServices.steamWriteReady;onClicked:hubServices.approveSteamWrite()}
  }
 }
 Dialog {id:removeDialog;objectName:"removeDialog";parent:root;anchors.centerIn:parent;width:Math.min(root.width-32,600);modal:true;focus:true;title:"Remove owned installation files?";standardButtons:Dialog.Ok|Dialog.Cancel
  footer:DialogButtonBox{standardButtons:removeDialog.standardButtons;delegate:PillButton{vrOverlayMode:root.vrOverlayMode}}
  onOpened:removeSteam.selected=false
  Column {width:parent.width;spacing:10
  ScrollView {width:parent.width;height:240;TextArea {text:typeof hubServices!=="undefined"?hubServices.removalPlan:"";readOnly:true;wrapMode:Text.Wrap;color:Theme.get("color.text.primary")}}
  PillButton {vrOverlayMode:root.vrOverlayMode;id:removeSteam;objectName:"removeSteamOption";text:"Also review Steam entry removal (separate approval)";visible:typeof hubServices!=="undefined"&&hubServices.canRemoveSteam;selected:false;onClicked:selected=!selected}
  UiText {width:parent.width;wrapMode:Text.Wrap;text:"Steam changes require a separate exact preview and approval. Close Steam yourself before applying them."}
  }
  onAccepted:if(hubServices.confirmRemoval(removeSteam.selected))steamDialog.open()
 }
 Connections {target:typeof hubServices!=="undefined"?hubServices:null;function onRemovalPlanChanged(){removeDialog.open()}}
 FolderDialog{id:folder;title:"Choose a folder to scan";onAccepted:roots.text+=(roots.text.length?";":"")+uiController.localPath(selectedFolder)}
 Shortcut{sequence:"Ctrl+K";onActivated:header.focusSearch()}
 Shortcut{sequence:"/";enabled:!header.searchField.activeFocus;onActivated:header.focusSearch()}
 Shortcut{sequence:"Escape";onActivated:if(root.vrOverlayMode)root.panelBack();else root.closeTop()}
 Shortcut{sequence:"Alt+Left";onActivated:root.back()}
 Connections{target:uiSettings;function onChanged(){root.previewGame=root.previewGame}}
 // Diagnostics may open one menu at start, for pictures taken by the Hub itself.
 Timer{interval:300;running:typeof startMenu!=="undefined"&&startMenu.length>0;onTriggered:{if(startMenu==="help")help.open();else if(startMenu==="sort")sort.open();else if(startMenu==="filters")filters.open()}}
 // Over everything but the menus: a press just after a page change lands here and goes nowhere; any other press,
 // hover and the stick pass through.
 MouseArea {objectName:"clickGuard";anchors.fill:parent;z:9000;enabled:root.overlayPresentation;acceptedButtons:Qt.AllButtons
  onPressed:mouse=>{mouse.accepted=Date.now()<root.clickGuardUntil}
  onWheel:wheel=>{wheel.accepted=false}}
 Component.onCompleted:{if(root.vrOverlayMode&&root.view==="List"&&!root.startAt.length)root.view="Model3"
  if(root.showAll&&(uiSettings.values,uiSettings.get("scanOnStartup",false))&&(uiSettings.values,uiSettings.get("scanRoots","")).length)uiController.startScan((uiSettings.values,uiSettings.get("scanRoots","")).split(/\n|;/))}
}
