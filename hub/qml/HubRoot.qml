// SPDX-License-Identifier: GPL-3.0-only
import QtQuick
import QtQuick.Controls
Item {
    id: root
    implicitWidth: 1000
    implicitHeight: 700
    Rectangle { anchors.fill: parent; color: surfaceColor }
    Column {
        anchors.centerIn: parent
        spacing: 20
        Label {
            text: "AladdinsCastle: " + catalogGameCount + " games"
            color: primaryTextColor
            font.pixelSize: 32
            font.bold: true
            Accessible.name: text
        }
        Label {
            text: "Hub foundation · M1 lane A"
            color: brandColor
            font.pixelSize: 18
        }
    }
}
