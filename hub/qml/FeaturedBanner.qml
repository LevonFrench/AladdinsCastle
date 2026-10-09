// SPDX-License-Identifier: GPL-3.0-only
import QtQuick
import AladdinsCastle.Hub
Rectangle {
 id:banner
 property var game: ({})
 property string sizeValue:"S"
 property bool explore:false
 property bool vrOverlayMode:false
 property bool reduceMotion:false
 signal showRequested(string id)
 signal exploreRequested()
 signal shuffleRequested()
 signal disableRequested()
 height:Math.max(vrOverlayMode?200:0,140+(sizeValue==="M"?16:sizeValue==="L"?34:0));radius:8;color:Theme.get("color.surface.banner");border.color:Theme.get("color.line.strong");clip:true
 GameArt {anchors.right:parent.right;width:parent.width*0.56;height:parent.height;game:banner.game;source:banner.game.artBanner||""}
 Rectangle { anchors.fill:parent;gradient:Gradient {orientation:Gradient.Horizontal;GradientStop{position:0;color:Theme.get("gradient.list_banner_fade.stop")[0].color} GradientStop{position:0.65;color:Theme.get("gradient.list_banner_fade.stop")[1].color} GradientStop{position:1;color:Theme.get("gradient.list_banner_fade.stop")[2].color}} }
 Column {anchors.left:parent.left;anchors.top:parent.top;anchors.margins:18;spacing:5;width:Math.min(parent.width-70,410)
  UiText {text:"● "+(banner.explore?"FEATURED PICK":"FEATURED GAME")+" · "+uiController.vrLabel(banner.game.vrBadge||0);font.pixelSize:10;color:banner.game.accent||Theme.get("color.brand.orange")}
  GradientText {width:parent.width;text:banner.game.title||"Discover the catalog";pixelSize:Theme.get("type.banner_title."+banner.sizeValue)}
  UiText {width:parent.width;text:(banner.game.subgenreLabels||[]).slice(0,3).join(" · ")+" · "+(banner.game.year||"");font.pixelSize:11;elide:Text.ElideRight;wrapMode:Text.NoWrap}
  Row {spacing:10;PillButton {text:banner.explore?"View this game":"Show";neon:true;accent:Theme.get("color.brand.gold");vrOverlayMode:banner.vrOverlayMode;onClicked:defer.restart()} PillButton {text:banner.explore?"Shuffle":"Explore all games ›";neon:true;accent:Theme.get("color.brand.orange");vrOverlayMode:banner.vrOverlayMode;onClicked:if(banner.explore)banner.shuffleRequested();else banner.exploreRequested()} }
 }
 PillButton {anchors.right:parent.right;anchors.top:parent.top;anchors.margins:8;width:40;text:"×";Accessible.name:"Hide featured banner";onClicked:banner.disableRequested()}
 Timer {id:defer;interval:Theme.get("motion.banner_defer");onTriggered:banner.showRequested(banner.game.gameId)}
}
