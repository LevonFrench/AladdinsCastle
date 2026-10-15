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
 implicitWidth: Math.max(vrOverlayMode?44:40, label.implicitWidth+24)
 hoverEnabled: true
 padding: 10
 Accessible.name: text
 Accessible.checked: selected
 contentItem: Text { id: label; text: control.text; color: !control.enabled ? Theme.get("color.text.faint") : control.neon ? Theme.get("color.surface.window") : Theme.get("color.text.inactive"); font.family: Theme.get("font.family"); font.pixelSize: control.vrOverlayMode ? 17 : 13; font.weight: 700; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter; elide: Text.ElideRight }
 background: Item {
 // Direction C: a filled pill; the primary action filled in its own colour; a white ring for focus, hover and the selected choice.
 Rectangle { id:frame;anchors.fill:parent;color: control.neon ? (control.enabled ? (control.down ? Qt.darker(control.accent,1.2) : control.accent) : Theme.alpha(control.accent,0.18)) : control.down ? Theme.get("color.surface.menu_hover") : control.selected ? Theme.get("color.line.strong") : Theme.get("color.surface.pill"); radius: Theme.get("radius.pill")
  border.width: control.activeFocus||control.selected||(control.hovered&&control.enabled)?2:0; border.color: Theme.get("color.glass.ring") }
 }
}
