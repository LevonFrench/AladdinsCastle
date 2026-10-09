// SPDX-License-Identifier: GPL-3.0-only
import QtQuick
import QtQuick.Controls
import QtQuick.Effects
import AladdinsCastle.Hub
Button {
 id: control
 property bool selected: false
 property color accent: Theme.get("color.glass.ring")
 property bool neon: false
 property bool vrOverlayMode: false
 property int targetHeight: vrOverlayMode ? 56 : 40
 implicitHeight: targetHeight
 implicitWidth: Math.max(40, label.implicitWidth+24)
 hoverEnabled: true
 padding: 10
 Accessible.name: text
 Accessible.checked: selected
 contentItem: Text { id: label; text: control.text; color: control.enabled ? (control.neon ? control.accent : Theme.get("color.text.inactive")) : Theme.get("color.text.faint"); font.family: Theme.get("font.family"); font.pixelSize: control.vrOverlayMode ? 16 : 12; font.weight: 600; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter; elide: Text.ElideRight }
 background: Item {
 Rectangle { id:frame;anchors.fill:parent;color: control.down ? Theme.get("color.surface.menu_hover") : control.neon ? Theme.get("formula.neon.fill") : Theme.get("color.surface.pill"); radius: Theme.get("radius.pill"); border.width: control.selected||control.activeFocus?2:1; border.color: control.activeFocus||control.selected ? Theme.get("color.glass.ring") : control.neon ? control.accent : control.hovered ? Theme.get("color.line.button") : Theme.get("color.glass.hairline") }
 Loader{anchors.fill:frame;z:-1;active:control.neon&&!(uiSettings.values,uiSettings.get("cheapEffects",true));sourceComponent:Component{MultiEffect{source:frame;shadowEnabled:true;shadowColor:control.accent;shadowBlur:0.45;shadowOpacity:0.42;shadowHorizontalOffset:0;shadowVerticalOffset:0}}}
 }
}
