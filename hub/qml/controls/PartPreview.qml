// SPDX-License-Identifier: GPL-3.0-only
import QtQuick
import QtQuick.Controls
Column {
    id: root
    property url imageSource
    property var markers: []
    property string label
    property color accentColor: "#dd6600"
    property color textColor: "#c8c8d4"
    spacing: 6
    Text { text: root.label; color: root.textColor; font.pixelSize: 16; font.bold: true }
    Rectangle {
        width: root.width; height: width
        color: "#16161a"; radius: 8
        Image { id: picture; anchors.fill: parent; anchors.margins: 8; source: root.imageSource; fillMode: Image.PreserveAspectFit }
        Repeater {
            model: root.markers
            Item {
                required property var modelData
                required property int index
                id: marker
                objectName: "assemblyCalloutMarker"
                anchors.fill: picture
                property real px: (picture.width-picture.paintedWidth)/2+modelData.x*picture.paintedWidth
                property real py: (picture.height-picture.paintedHeight)/2+modelData.y*picture.paintedHeight
                property real ly: Math.min(height-26,18+index*32)
                Canvas {
                    anchors.fill: parent
                    onPaint: {
                        const c=getContext("2d");c.clearRect(0,0,width,height)
                        c.strokeStyle=root.accentColor;c.lineWidth=1;c.beginPath()
                        c.moveTo(marker.px,marker.py);c.lineTo(marker.modelData.x<.5?22:width-22,marker.ly+10);c.stroke()
                    }
                }
                Rectangle { x: marker.px-3;y: marker.py-3;width: 6;height: 6;radius: 3;color: root.accentColor }
                Rectangle {
                    x: modelData.x<.5?4:marker.width-width-4;y: marker.ly
                    width: number.implicitWidth+12;height: 22;radius: 3
                    color: modelData.pressed?root.accentColor:"#16161a";border.color: root.accentColor
                    Text { id: number;anchors.centerIn: parent;text: modelData.numbers;color: root.textColor;font.pixelSize: 12 }
                    HoverHandler { id: hover }
                    ToolTip.visible: hover.hovered;ToolTip.text: modelData.callout
                }
                Accessible.name: modelData.callout
            }
        }
    }
}
