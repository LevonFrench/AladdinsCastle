// SPDX-License-Identifier: GPL-3.0-only
import QtQuick
import QtQuick.Effects
import AladdinsCastle.Hub
FocusScope {
 id: card
 objectName:"gameCard"
 layer.enabled: preview
 property var game: ({})
 property string sizeValue: "S"
 property bool vrOverlayMode: false
 property bool preview: false
 property bool scrolling: false
 property bool effectsEnabled: true
 property real factor: Theme.get("size.card.scale."+sizeValue)
 signal detailRequested(string gameId)
 signal actionRequested(string gameId)
 signal previewRequested(var game, var item)
 signal previewClosed()
 signal navigationRequested(string id, int dx, int dy)
 implicitWidth: Theme.get("size.card.base_w")*factor
 implicitHeight: Math.max(Theme.get("size.card.base_h")*factor,vrOverlayMode?248:0)
 activeFocusOnTab: true
 Accessible.name: game.title||"Game"
 Accessible.role: Accessible.Button
 Keys.onLeftPressed:navigationRequested(game.gameId,-1,0)
 Keys.onRightPressed:navigationRequested(game.gameId,1,0)
 Keys.onUpPressed:navigationRequested(game.gameId,0,-1)
 Keys.onDownPressed:navigationRequested(game.gameId,0,1)
 Keys.onReturnPressed: detailRequested(game.gameId)
 Keys.onSpacePressed: actionRequested(game.gameId)
 Rectangle { id: frame; anchors.fill: parent; color: Theme.mix(Theme.get("color.surface.card"), card.game.accent||Theme.get("color.brand.orange"),0.06); radius: Theme.get("size.card.radius"); border.width: card.activeFocus?2:1; border.color: card.activeFocus?Theme.get("color.glass.ring"):Theme.mix(Theme.get("color.line.strong"),card.game.accent||Theme.get("color.brand.orange"),Theme.get("formula.border.accent_mix")) }
 Loader { anchors.fill:frame;z:-1;active:card.effectsEnabled&&(hover.hovered||card.preview||card.activeFocus);sourceComponent:Component{MultiEffect{source:frame;shadowEnabled:true;shadowColor:card.game.accent||Theme.get("color.brand.orange");shadowBlur:0.5;shadowOpacity:0.45;shadowHorizontalOffset:0;shadowVerticalOffset:0}} }
 HoverHandler { id: hover }
 TapHandler { onTapped: card.detailRequested(card.game.gameId) }
 Timer { id: dwell; interval: Theme.get("motion.hover_dwell"); onTriggered: if(!card.scrolling&&!card.preview&&!primary.hovered)card.previewRequested(card.game,card) }
 onActiveFocusChanged: { if(activeFocus&&!preview)dwell.restart();else if(!hover.hovered){dwell.stop();previewClosed()} }
 Connections { target: hover; function onHoveredChanged(){if(hover.hovered&&!card.preview&&!card.scrolling)dwell.restart();else {dwell.stop();card.previewClosed()}} }
 onScrollingChanged: if(scrolling){dwell.stop();previewClosed()}
 Column {
  anchors.fill: parent; anchors.margins: (card.preview?7:Theme.get("space.card_inner_pad"))*card.factor; spacing: 2*card.factor
  GameArt { visible: card.preview; width: parent.width; height: card.preview?Theme.get("size.card.art_strip_h"):0; game:card.game; source: card.game.artBanner||"" }
  Row { width:parent.width; spacing:4; visible:!card.preview
   Rectangle { width:Math.min(Theme.get("size.card.family_pill_max_w")*card.factor,pill.implicitWidth+12);height:16*card.factor;radius:3;color:Theme.mix(Theme.get("color.surface.pill"),card.game.accent||Theme.get("color.brand.orange"),Theme.get("formula.pill.fill_accent_mix")); UiText { id:pill; anchors.centerIn:parent;text:card.game.pill||card.game.hardwareFamily||"CATALOG";font.pixelSize:8*card.factor;color:Theme.mix(Theme.get("color.text.primary"),card.game.accent||Theme.get("color.brand.orange"),0.5);width:parent.width-8;elide:Text.ElideRight;wrapMode:Text.NoWrap } }
   UiText { text:card.game.isWip?"WIP":"";font.pixelSize:8*card.factor;color:Theme.get("color.badge.wip") }
  }
  GradientText { width:parent.width;text:card.game.title||"";pixelSize:Math.round(Theme.get("type.card_title.size")*card.factor);bottomColor:Theme.get("type.gradient.card_title_bottom") }
  UiText { visible:!card.preview;width:parent.width;text:(card.game.manufacturerShort||"")+" · "+(card.game.hardwareLabel||"")+" · "+(card.game.year||"");font.pixelSize:9*card.factor;color:card.game.accent||Theme.get("color.brand.orange");elide:Text.ElideRight;wrapMode:Text.NoWrap }
  UiText { visible:!card.preview&&card.game.developerShown===true;width:parent.width;text:"by "+(card.game.developer||"");font.pixelSize:9*card.factor;color:Theme.get("color.text.author");elide:Text.ElideRight;wrapMode:Text.NoWrap }
  UiText { visible:!card.preview;width:parent.width;height:20*card.factor;text:card.game.blurb||"Catalog metadata · supply your own game files";font.pixelSize:10*card.factor;color:Theme.get("color.text.note");maximumLineCount:2;elide:Text.ElideRight }
  UiText { visible:!card.preview;width:parent.width;elide:Text.ElideRight;wrapMode:Text.NoWrap;text:uiController.vrLabel(card.game.vrBadge||0)+"  "+(card.game.players>1?card.game.players+"P":"")+"  "+(card.game.controlsLabel||"");font.pixelSize:8*card.factor;color:Theme.get("color.text.muted_detail") }
 }
 PillButton {vrOverlayMode:card.vrOverlayMode; id:primary; objectName:"cardPrimary"; anchors.left:parent.left;anchors.right:info.left;anchors.bottom:parent.bottom;anchors.margins:10*card.factor;text:uiController.primaryLabel(card.game.gameId||"");height:card.vrOverlayMode?56:40;neon:true;accent:card.game.stateColour||card.game.accent||Theme.get("color.brand.orange");onClicked:card.actionRequested(card.game.gameId);onHoveredChanged:if(hovered){dwell.stop();card.previewClosed()} }
 PillButton {vrOverlayMode:card.vrOverlayMode; id:info;anchors.right:parent.right;anchors.bottom:parent.bottom;anchors.margins:10*card.factor;width:vrOverlayMode?44:40;height:card.vrOverlayMode?56:40;text:"i";Accessible.name:"Details for "+(card.game.title||"");onClicked:card.detailRequested(card.game.gameId) }
}
