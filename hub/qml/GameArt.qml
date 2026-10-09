// SPDX-License-Identifier: GPL-3.0-only
import QtQuick
import AladdinsCastle.Hub
Item {
 id: art
 property var game: ({})
 property string kind: "banner"
 property url source: ""
 property url effectiveSource:{let s=source.toString();if(s.startsWith("http:")||s.startsWith("https:"))return "";if(s.startsWith("image://art/")){if(!uiController.artProviderReady)return "";return s+(s.includes("?")?"&":"?")+"v="+uiController.artRevision}return source}
 clip: true
 Rectangle { anchors.fill: parent; color: Theme.get("color.hero.bg"); gradient: Gradient { orientation: Gradient.Horizontal; GradientStop { position: 0; color: Theme.mix(Theme.get("color.surface.card"), art.game.accent || Theme.get("color.brand.orange"),0.15) } GradientStop { position: 1; color: Theme.mix(Theme.get("color.surface.banner"), art.game.accent || Theme.get("color.brand.orange"),0.45) } } }
 // Original abstract fallback: lane F may replace this with a local art provider.
 Repeater { model: 8; Rectangle { required property int index; x: art.width*0.45+index*22; y: -art.height; width: 2; height: art.height*3; rotation: 28; color: Theme.alpha(art.game.accent||Theme.get("color.brand.orange"),0.17) } }
 Rectangle { anchors.right: parent.right; anchors.rightMargin: parent.width*0.08; anchors.verticalCenter: parent.verticalCenter; width: Math.min(parent.height*0.65,parent.width*0.34); height: width; radius: art.game.genreId==="racing"?width/2:12; rotation: art.game.genreId==="racing"?0:-18; color: Theme.alpha(art.game.accent||Theme.get("color.brand.orange"),0.13); border.width: 3; border.color: Theme.alpha(art.game.accent||Theme.get("color.brand.orange"),0.75)
 UiText { anchors.centerIn: parent; text: art.game.genreId==="racing"?"◉":"⌖"; font.pixelSize: parent.width*0.5; color: art.game.accent||Theme.get("color.brand.orange") } }
 Image { id: localImage; anchors.fill: parent; fillMode: Image.PreserveAspectCrop; asynchronous: true; source:art.effectiveSource; visible: status===Image.Ready }
 UiText { visible: localImage.status!==Image.Ready; anchors.left: parent.left; anchors.bottom: parent.bottom; anchors.margins: 16; text: (art.game.hardwareLabel||"AladdinsCastle")+" · "+(art.game.year||""); color: Theme.get("color.text.muted_detail"); font.pixelSize: 10; width: parent.width-32; elide: Text.ElideRight; wrapMode: Text.NoWrap }
}
