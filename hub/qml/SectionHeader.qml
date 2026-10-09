// SPDX-License-Identifier: GPL-3.0-only
import QtQuick
import AladdinsCastle.Hub
Rectangle {
 property string title: ""
 property int count: 0
 property color accent: Theme.get("color.brand.orange")
 implicitHeight: 40; color:Theme.get("color.glass.section_fill");border.color:Theme.get("color.glass.section_line");radius:11
 Row { anchors.verticalCenter:parent.verticalCenter;anchors.left:parent.left;anchors.leftMargin:12;spacing:12;UiText {text:"ϟ";color:parent.parent.accent} UiText {text:parent.parent.title;font.weight:600;color:Theme.get("color.text.primary")} UiText {text:parent.parent.count+" games";color:Theme.get("color.text.count");font.pixelSize:11} }
}
