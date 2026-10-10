// SPDX-License-Identifier: GPL-3.0-only
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import AladdinsCastle.Hub
ScrollView {
 id:page
 property var detail:uiController.detail
 property bool vrOverlayMode:false
 property string sizeValue:"M"
 property int similarLimit:4
 signal detailRequested(string id)
 clip:true
 objectName:"detailPage"
 Component.onCompleted:uiController.refreshDetailControls()
 Component.onDestruction:uiController.leaveDetail()
 Column{width:page.availableWidth;spacing:18
  GameArt {objectName:"detailHero";anchors.horizontalCenter:parent.horizontalCenter;width:Math.min(parent.width,Theme.get("size.detail.hero_max_w"));height:Theme.get("size.detail.hero_h");game:page.detail;source:page.detail.artBanner||""}
  GradientText{width:parent.width;text:page.detail.title||"Game";pixelSize:28}
  Flow{width:parent.width;spacing:6
   Repeater{model:[page.detail.pill||"CATALOG",uiController.vrLabel(page.detail.vrBadge||0),page.detail.stateLabel||"No setup yet"].concat(page.detail.badges||[]);PillButton{vrOverlayMode:page.vrOverlayMode;required property var modelData;text:typeof modelData==="string"?modelData:modelData.label||modelData.id||"";enabled:false}}
  }
  Flow{width:parent.width;spacing:24
   Repeater{model:[{label:"MANUFACTURER",value:page.detail.manufacturerLabel},{label:"HARDWARE",value:page.detail.hardwareLabel},{label:"YEAR",value:page.detail.year},{label:"DEVELOPER",value:page.detail.developerShown?page.detail.developer:""}];Column{required property var modelData;visible:!!modelData.value;UiText{text:modelData.label;font.pixelSize:9;color:Theme.get("color.text.label")} UiText{text:String(modelData.value||"");font.pixelSize:13;color:Theme.get("color.text.value")}}}
  }
  UiText{text:"VARIANT";font.pixelSize:11;color:Theme.get("color.text.label")}
  Flow{width:parent.width;spacing:6
   Repeater{model:page.detail.variants||[];PillButton{vrOverlayMode:page.vrOverlayMode;required property var modelData;text:modelData.title+" · "+(modelData.quality||"flat")+(modelData.m1Available?"":" · outside M1");selected:page.detail.variantId===modelData.id;onClicked:uiController.selectVariant(modelData.id)}}
  }
  Rectangle{width:parent.width;height:stateText.implicitHeight+24;radius:6;color:Theme.alpha(page.detail.stateColour||Theme.get("color.state.planned_neon"),0.1);border.color:page.detail.stateColour||Theme.get("color.state.planned_neon");UiText{id:stateText;anchors.fill:parent;anchors.margins:12;text:page.detail.stateLabel==="No setup yet"?"No setup yet. Browse the metadata and documentation; a playable setup will appear here when available.":!page.detail.m1Available?"This VR setup is outside M1. Choose a flat emulator route for this milestone.":page.detail.state===5?"Needs your files. We never download game content.":page.detail.state===6?"Needs an emulator. Install it or locate an existing copy.":page.detail.stateLabel||"No setup yet";color:page.detail.stateColour||Theme.get("color.state.planned_neon")}}
  Column{width:parent.width;spacing:6;visible:!uiController.installing
   UiText{text:"WHAT YOU NEED";font.weight:600;color:Theme.get("color.text.heading")}
   Repeater{model:page.detail.needs||[];RowLayout{required property var modelData;width:parent.width;spacing:10
    UiText{text:modelData.found?"✓":"○";color:Theme.get(modelData.found?"color.console.ok":"color.console.warn")}
    UiText{Layout.fillWidth:true;text:modelData.name+" · "+modelData.status}
    PillButton{vrOverlayMode:page.vrOverlayMode;text:modelData.kind==="media"?"Find my files":modelData.kind==="tool"?"Locate":"Check at launch";onClicked:uiController.openLocation(modelData.kind)}
   }}
   UiText{visible:(page.detail.needs||[]).length===0;text:"No install requirements have been authored for this entry.";color:Theme.get("color.text.muted_detail")}
  }
  Flow{width:parent.width;spacing:12
   PillButton{objectName:"detailPrimary";text:page.detail.playing?"Playing":page.detail.state===4?page.detail.variantTitle:page.detail.state===6?"Locate / install emulator":page.detail.stateLabel||"No setup yet";implicitWidth:162;neon:true;accent:page.detail.stateColour||Theme.get("color.brand.orange");vrOverlayMode:page.vrOverlayMode;enabled:page.detail.m1Available===true&&!page.detail.playing&&[3,4,5,6,7,2].indexOf(page.detail.state)>=0;onClicked:uiController.primary(page.detail.gameId)}
   PillButton{objectName:"detailStop";text:"Stop";visible:typeof hubServices!=="undefined"&&hubServices.launchBusy;vrOverlayMode:page.vrOverlayMode;onClicked:uiController.stopLaunch()}
   UiText{objectName:"launchProgress";text:"Preparing launch…";visible:typeof hubServices!=="undefined"&&hubServices.preparing}
   PillButton{vrOverlayMode:page.vrOverlayMode;text:"Reinstall";visible:page.detail.reinstallVisible===true;onClicked:uiController.startInstall(page.detail.gameId,page.detail.variantId)}
   PillButton{vrOverlayMode:page.vrOverlayMode;text:"Add to Steam library";visible:page.detail.state===4;onClicked:uiController.openLocation("steam-shortcut")}
   PillButton{vrOverlayMode:page.vrOverlayMode;text:"Upstream page";visible:(page.detail.components||[]).length>0&&!!page.detail.components[0].upstream;onClicked:uiController.openLink(page.detail.components[0].upstream)}
  }
  InstallConsole{width:parent.width;vrOverlayMode:page.vrOverlayMode}
  RecoveryPanel{width:parent.width;vrOverlayMode:page.vrOverlayMode}
  Column{width:parent.width;spacing:8;visible:(page.detail.settingsSupported||[]).length>0
   UiText{text:"SETTINGS FOR THIS GAME";font.weight:600;color:Theme.get("color.text.heading")}
   Flow{width:parent.width;spacing:8
    ComboBox{id:cover;implicitHeight:page.vrOverlayMode?56:40;model:["Hold to shoot","Hold to cover","Toggle"];Accessible.name:"Cover mode";currentIndex:["hold_to_shoot","hold_to_cover","toggle"].indexOf(page.detail.settings?.cover||"hold_to_shoot")}
    PillButton{vrOverlayMode:page.vrOverlayMode;id:laser;text:"Laser "+(selected?"On":"Off");selected:page.detail.settings?.laser!=="off";onClicked:selected=!selected}
    Slider{id:angle;from:-60;to:60;stepSize:1;value:page.detail.settings?.gun_pitch||0;Accessible.name:"Gun angle"}
    UiText{text:Math.round(angle.value)+"°"}
    PillButton{vrOverlayMode:page.vrOverlayMode;id:hand;text:selected?"Left handed":"Right handed";selected:page.detail.settings?.left_handed===true;onClicked:selected=!selected}
    PillButton{vrOverlayMode:page.vrOverlayMode;text:"Save";onClicked:uiSettings.saveGame(page.detail.gameId,{cover:["hold_to_shoot","hold_to_cover","toggle"][cover.currentIndex],laser:laser.selected?"on":"off",gun_pitch:Math.round(angle.value),left_handed:hand.selected})}
   }
   UiText{text:"Saved settings are used by the next owned install or repair. Existing emulator profiles are kept.";color:Theme.get("color.text.muted_detail");font.pixelSize:11}
  }
  UiText{text:"ABOUT";font.weight:600;color:Theme.get("color.text.heading")}
  UiText{width:parent.width;text:page.detail.blurb||"Catalog entry. No authored setup yet."}
  Rectangle{visible:!!page.detail.notice;width:parent.width;height:visible?notice.implicitHeight+24:0;color:Theme.get("color.notice.fill");border.color:Theme.get("color.notice.line");radius:6;UiText{id:notice;anchors.fill:parent;anchors.margins:12;text:page.detail.notice||"";color:Theme.get("color.notice.body")}}
  UiText{text:"SIMILAR GAMES";font.weight:600;color:Theme.get("color.text.heading")}
  Repeater{model:(page.detail.similar||[]).slice(0,page.similarLimit);PillButton{vrOverlayMode:page.vrOverlayMode;required property var modelData;width:parent.width;text:modelData.title+" · "+modelData.controlsLabel+" · "+modelData.vrBadge+" · "+modelData.stateLabel;onClicked:page.detailRequested(modelData.gameId)}}
  PillButton{vrOverlayMode:page.vrOverlayMode;text:"Show more";visible:(page.detail.similar||[]).length>page.similarLimit;onClicked:page.similarLimit=12}
  UiText{text:"CONTROLS";font.weight:600;color:Theme.get("color.text.heading")}
  UiText{objectName:"detailControlsStatus";width:parent.width;visible:text.length>0;text:uiController.detailControlsStatus;color:Theme.get("color.text.muted_detail")}
  PillButton{text:"Reload controls";objectName:"reloadDetailControls";visible:!page.vrOverlayMode;enabled:!uiController.detailControlsLoading;onClicked:uiController.reloadDetailControls()}
  Loader{
   id:controlsView;objectName:"detailControlsLoader";width:parent.width
   active:!page.vrOverlayMode&&uiController.detailControls.gameId===page.detail.gameId&&uiController.detailControls.gameId.length>0
   visible:active;height:item?item.implicitHeight:0;source:"qrc:/controls/ControlsView.qml"
   onLoaded:{
    item.controlsModel=uiController.detailControls
    item.width=Qt.binding(()=>controlsView.width)
    item.panelColor=Qt.binding(()=>Theme.get("color.surface.panel"))
    item.textColor=Qt.binding(()=>Theme.get("color.text.body"))
    item.mutedColor=Qt.binding(()=>Theme.get("color.text.muted_detail"))
    item.accentColor=Qt.binding(()=>Theme.get("color.brand.orange"))
    item.textSize=Qt.binding(()=>Theme.get("type.detail_body."+page.sizeValue))
   }
  }
  RowLayout{width:parent.width;UiText{Layout.preferredWidth:parent.width*0.25;text:"Action";font.weight:600} UiText{Layout.preferredWidth:parent.width*0.3;text:"Input";font.weight:600} UiText{Layout.fillWidth:true;text:"Notes";font.weight:600}}
  Repeater{model:page.detail.controls||[];RowLayout{required property var modelData;width:parent.width;UiText{Layout.preferredWidth:parent.width*0.25;text:modelData.action} UiText{Layout.preferredWidth:parent.width*0.3;text:modelData.input} UiText{Layout.fillWidth:true;text:modelData.notes}}}
  UiText{width:parent.width;text:"The pause overlay uses the reserved menu input. Bind flat inputs in your emulator.";font.pixelSize:11;color:Theme.get("color.text.muted_detail")}
  UiText{text:"README";font.weight:600;color:Theme.get("color.text.heading")}
  TextEdit{objectName:"readmeText";width:parent.width;readOnly:true;selectByMouse:true;text:page.detail.readme||"No setup documentation yet.";textFormat:TextEdit.MarkdownText;wrapMode:TextEdit.Wrap;font.family:Theme.get("font.family");font.pixelSize:Theme.get("type.detail_body."+page.sizeValue);color:Theme.get("color.text.body");onLinkActivated:link=>uiController.openLink(link)}
  UiText{text:"WHAT IT INSTALLS";font.weight:600;color:Theme.get("color.text.heading")}
  Repeater{model:page.detail.components||[];Column{required property var modelData;width:parent.width;UiText{width:parent.width;text:modelData.name+" · "+(modelData.version||"Recipe version")+" · "+(modelData.license||"See upstream licence")+" · "+modelData.role} PillButton{vrOverlayMode:page.vrOverlayMode;text:"Upstream";visible:!!modelData.upstream;onClicked:uiController.openLink(modelData.upstream)}}}
  Column{visible:page.detail.reinstallVisible===true||page.detail.m1Available===true;width:parent.width;spacing:8;UiText{text:"OWNED FILES AND STEAM";font.weight:600;color:Theme.get("color.text.heading")} UiText{width:parent.width;text:"Review the ownership manifest and optionally the Hub-owned Steam shortcut. An empty manifest means no owned installation files to remove. This action does not clean emulator profiles."} PillButton{vrOverlayMode:page.vrOverlayMode;objectName:"detailOwnedRemoval";text:"Preview owned files / Steam removal";accent:Theme.get("color.state.uninstall");neon:true;onClicked:uiController.uninstall(page.detail.gameId,page.detail.variantId)}}
  UiText{visible:!!page.detail.quip;width:parent.width;text:page.detail.quip||"";font.italic:true;color:page.detail.accent||Theme.get("color.brand.orange")}
  Item{height:24;width:1}
 }
}
