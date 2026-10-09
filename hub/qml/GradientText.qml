// SPDX-License-Identifier: GPL-3.0-only
import QtQuick
import AladdinsCastle.Hub
Item {
 id:root
 property string text:""
 property int pixelSize:22
 property int weight:700
 property color topColor:Theme.get("type.gradient.title_top")
 property color bottomColor:Theme.get("type.gradient.title_bottom")
 implicitWidth:label.implicitWidth
 implicitHeight:label.implicitHeight
 // Cheaper text fallback: a clean theme-derived heading, no per-title mask texture.
 Text{id:label;anchors.fill:parent;text:root.text;font.family:Theme.get("font.family");font.pixelSize:root.pixelSize;font.weight:root.weight;color:Theme.mix(root.topColor,root.bottomColor,0.3);renderType:Text.QtRendering;elide:Text.ElideRight}
}
