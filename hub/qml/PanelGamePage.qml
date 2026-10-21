// SPDX-License-Identifier: GPL-3.0-only
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import AladdinsCastle.Hub
// One game's page on the SteamVR panel, after Steam's: a hero banner with the title, one big Play button
// per way the game can be seen (3D screen, open frame, theatre, inside the game, the monitor), what the
// controls are, where it stands, and Back. Every Play goes through the same launch path as the VR games page.
FocusScope {
 id:page
 objectName:"panelGamePage"
 property var row:({})
 property bool vrOverlayMode:true
 property bool reduceMotion:false
 // From the library: the system's name, and whether this row's "warning" holds its controls.
 property string systemLabel:""
 property bool warningIsControls:false
 signal backRequested()
 signal detailRequested(string id)
 signal setupRequested()
 readonly property real side:28
 readonly property color readyColour:Theme.get("color.state.ready_neon")
 readonly property color needsColour:Theme.get("color.state.needs_neon")
 readonly property bool catalog:row.system==="catalog"
 // The launcher's own name for this game while it starts or runs.
 readonly property string sessionId:row.system?String(row.id||""):"model3-"+(row.id||"")
 readonly property var session:uiController.nowPlaying
 readonly property bool starting:session.gameId===sessionId&&!session.running
 readonly property bool waitingForSteamVr:!!uiController.steamVrStarting&&uiController.steamVrStarting===row.title
 // Every Play button turns off the moment one is pressed: the launch gate closes as the game is queued.
 readonly property bool canStart:uiController.launchGateOpen&&!row.problem
 function headsetViews(){return (row.views||uiController.model3Views).filter(v=>v.id!=="monitor")}
 function monitorView(){for(const v of (row.views||[]))if(v.id==="monitor")return v;return null}
 function defaultView(){const list=headsetViews();for(const v of list)if(v.id===row.view)return v.id;return list.length?list[0].id:""}
 readonly property var plays:{
  if(catalog)return [{id:"catalog",label:"Play",help:"Starts the set-up this PC has for it."}]
  const list=headsetViews().slice();const monitor=monitorView();if(monitor)list.push(monitor);return list
 }
 function play(view){
  if(!canStart)return
  if(catalog)uiController.primary(row.id);else uiController.playGame(row.id,view)
 }
 function hardwareLabel(){return systemLabel||(row.system?String(row.pill||row.system):"Model 3")}
 // Read from the row, the same for every system: its own controls, else its controls note; Model 3 rows
 // (no system) use the emulator's keys.
 readonly property string controlsText:{
  if(row.controls)return String(row.controls)
  if(!row.system)return "Home = set straight ahead · 1 = start · 3 = coin · Esc = quit."+(row.cabinetControl?" Inside the game, the cabinet's "+row.cabinetControl+" can be shown in front of you.":"")
  if(catalog)return (row.card||{}).controlsLabel?"Controls: "+row.card.controlsLabel+".":""
  return warningIsControls?(row.warning||""):""
 }
 readonly property string beforeText:!warningIsControls&&!catalog?(row.warning||""):""
 readonly property string statusText:row.problem?row.problem:waitingForSteamVr?"Waiting for SteamVR to start, then the game starts.":starting?"Starting the game. It opens in a moment.":session.gameId===sessionId?"Playing now.":!uiController.launchGateOpen?"Another game is running. Close it before starting this one.":uiController.steamVrRunning?"Ready to play.":"Ready to play. SteamVR is off: Play starts it first, then the game."
 readonly property string factsText:"In the Hub's list: yes   ·   On this PC: "+(catalog?"set up":row.found?"found":"not ready")+"   ·   In the headset: "+(row.accepted?"accepted by you ("+row.accepted+")":"not yet accepted by you")
 // A view's own play button: big, with what it is in a line underneath.
 component PlayButton:Button {
  id:button
  property var view:({})
  property bool primary:false
  objectName:"panelPlay"
  implicitWidth:300;implicitHeight:108;padding:16;hoverEnabled:true;focusPolicy:Qt.NoFocus
  enabled:page.canStart
  Accessible.name:text
  text:page.starting?"Starting…":page.waitingForSteamVr?"Waiting for SteamVR…":(view.id==="monitor"?"Play on monitor":view.id==="catalog"?"Play":"Play · "+view.label)
  onClicked:page.play(view.id)
  background:Rectangle {radius:Theme.get("radius.card")
   color:!button.enabled?Theme.alpha(button.primary?page.readyColour:Theme.get("color.surface.card"),0.25):button.primary?(button.down?Qt.darker(page.readyColour,1.2):page.readyColour):(button.down?Theme.get("color.surface.menu_hover"):Theme.get("color.surface.card"))
   border.width:button.hovered&&button.enabled?4:0;border.color:Theme.get("color.glass.ring")}
  contentItem:ColumnLayout {spacing:4
   Text {Layout.fillWidth:true;text:"▶  "+button.text;elide:Text.ElideRight;color:button.primary&&button.enabled?Theme.get("color.surface.window"):Theme.get("color.text.primary");font.family:Theme.get("font.family");font.weight:800;font.pixelSize:22}
   Text {Layout.fillWidth:true;text:button.view.help||"";visible:text.length>0;wrapMode:Text.Wrap;maximumLineCount:2;elide:Text.ElideRight;color:button.primary&&button.enabled?Theme.alpha(Theme.get("color.surface.window"),0.8):Theme.get("color.text.muted");font.family:Theme.get("font.family");font.weight:500;font.pixelSize:15}
   Item {Layout.fillHeight:true}
  }
 }
 // The slim bar: Back and where you are.
 Rectangle {
  id:bar;anchors.left:parent.left;anchors.right:parent.right;height:76;color:Theme.get("color.surface.header");z:2
  RowLayout {anchors.fill:parent;anchors.leftMargin:page.side;anchors.rightMargin:page.side;spacing:14
   PillButton {objectName:"panelGameBack";vrOverlayMode:page.vrOverlayMode;text:"‹  Back";onClicked:page.backRequested()}
   Text {Layout.fillWidth:true;text:"Library  ›  "+page.hardwareLabel()+"  ›  "+(page.row.title||"");elide:Text.ElideRight;color:Theme.get("color.text.muted");font.family:Theme.get("font.family");font.weight:600;font.pixelSize:18}
   PillButton {objectName:"panelGameArt";visible:!page.catalog;vrOverlayMode:page.vrOverlayMode;text:uiController.artworkBusy?"Getting artwork…":"Get artwork";enabled:!uiController.artworkBusy;onClicked:uiController.getArtwork(page.row.artId||page.row.id)}
   PillButton {objectName:"panelGameDetails";visible:page.catalog;vrOverlayMode:page.vrOverlayMode;text:"Details";onClicked:page.detailRequested(page.row.id)}
  }
 }
 Flickable {
  id:scroll;objectName:"panelGameScroll";anchors.top:bar.bottom;anchors.left:parent.left;anchors.right:parent.right;anchors.bottom:parent.bottom
  contentHeight:column.implicitHeight+24;clip:true;interactive:false;boundsBehavior:Flickable.StopAtBounds
  ScrollBar.vertical:ScrollBar {width:18;policy:scroll.contentHeight>scroll.height?ScrollBar.AlwaysOn:ScrollBar.AlwaysOff}
  ColumnLayout {
   id:column;width:scroll.width;spacing:18
   HeroBanner {
    objectName:"panelGameHero";Layout.fillWidth:true;Layout.preferredHeight:260;vrOverlayMode:page.vrOverlayMode
    system:page.row.system==="catalog"?"catalog":(page.row.system||"model3")
    art:page.row.heroArt||page.row.art||(page.catalog&&page.row.card?(page.row.card.artBanner||""):"");artBlurred:!!page.row.heroBlur
    eyebrow:(page.hardwareLabel()+(page.row.year?"  ·  "+page.row.year:"")).toUpperCase()
    title:page.row.title||""
    meta:page.statusText;metaColour:page.row.problem?page.needsColour:Theme.get("color.text.muted")
   }
   Text {objectName:"panelGameTitle";visible:false;text:page.row.title||""}
   Flow {
    objectName:"panelPlays";Layout.fillWidth:true;Layout.leftMargin:page.side;Layout.rightMargin:page.side;spacing:16
    Repeater {model:page.plays
     PlayButton {required property var modelData;view:modelData;primary:modelData.id===page.defaultView()||modelData.id==="catalog"}}
   }
   UiText {objectName:"panelBefore";Layout.fillWidth:true;Layout.leftMargin:page.side;Layout.rightMargin:page.side;visible:page.beforeText.length>0
    text:"Before you start: "+page.beforeText;font.pixelSize:18;color:page.needsColour}
   RowLayout {
    Layout.fillWidth:true;Layout.leftMargin:page.side;Layout.rightMargin:page.side;spacing:28
    ColumnLayout {Layout.fillWidth:true;Layout.preferredWidth:1;Layout.alignment:Qt.AlignTop;spacing:6;visible:page.controlsText.length>0
     Text {text:"Controls";color:Theme.get("color.text.primary");font.family:Theme.get("font.family");font.weight:700;font.pixelSize:22}
     UiText {objectName:"panelControls";Layout.fillWidth:true;text:page.controlsText;font.pixelSize:18;color:Theme.get("color.text.body")}
    }
    ColumnLayout {Layout.fillWidth:true;Layout.preferredWidth:1;Layout.alignment:Qt.AlignTop;spacing:6
     Text {text:"Status";color:Theme.get("color.text.primary");font.family:Theme.get("font.family");font.weight:700;font.pixelSize:22}
     UiText {objectName:"panelStatus";Layout.fillWidth:true;text:page.statusText;font.pixelSize:18;color:page.row.problem?page.needsColour:Theme.get("color.text.body")}
     UiText {objectName:"panelFacts";Layout.fillWidth:true;visible:!page.catalog;text:page.factsText;font.pixelSize:15;color:Theme.get("color.text.muted_detail")}
     PillButton {visible:!!page.row.problem;vrOverlayMode:page.vrOverlayMode;text:"Open Set-up";onClicked:page.setupRequested()}
    }
   }
   // Inside the game, the cabinet's own wheel or handlebars can be drawn in front of you.
   SettingRow {
    objectName:"panelCabinetControl";Layout.fillWidth:true;Layout.leftMargin:page.side;Layout.rightMargin:page.side;vrOverlayMode:page.vrOverlayMode
    visible:!page.row.system&&!!page.row.cabinetControl
    label:"Show the cabinet's "+(page.row.cabinetControl||"wheel")+" in front of you";on:!!page.row.showControl
    detail:"Inside the game only. It turns with the steering the game receives."
    onClicked:uiController.setModel3Control(page.row.id,!on)
   }
   // Several copies of this game in the library: which one is used.
   ColumnLayout {
    id:copiesBox;objectName:"panelCopies";Layout.fillWidth:true;Layout.leftMargin:page.side;Layout.rightMargin:page.side;spacing:8
    readonly property string system:page.row.system||"model3"
    readonly property string game:page.row.system?String(page.row.id||"").substring(page.row.system.length+1):(page.row.id||"")
    readonly property var copies:page.catalog?[]:(uiController.library,uiSettings.values,uiController.copiesOf(system,game))
    readonly property string inUse:page.catalog?"":(uiController.library,uiSettings.values,uiController.copyInUse(system,game))
    visible:copies.length>1
    Text {text:"Copy used ("+copiesBox.copies.length+" found)";color:Theme.get("color.text.primary");font.family:Theme.get("font.family");font.weight:700;font.pixelSize:20}
    Flow {Layout.fillWidth:true;spacing:8
     Repeater {model:copiesBox.copies
      PillButton {required property var modelData;vrOverlayMode:page.vrOverlayMode;readonly property string file:modelData.file||""
       neon:file===copiesBox.inUse;accent:Theme.get("color.text.primary");text:file.split("/").pop()+(modelData.region?"  ·  "+modelData.region:"")
       onClicked:uiController.chooseCopy(copiesBox.system,copiesBox.game,file)}}}
   }
  }
 }
 MouseArea {anchors.fill:scroll;acceptedButtons:Qt.NoButton;z:1;onWheel:wheel=>{wheel.accepted=true;scroller.wheel(wheel.angleDelta.y,Date.now())}}
 PanelScroller {id:scroller;flick:scroll;rowHeight:120;reduceMotion:page.reduceMotion}
}
