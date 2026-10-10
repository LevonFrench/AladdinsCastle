// SPDX-License-Identifier: GPL-3.0-only
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Column {
    id: root
    property var controlsModel: null
    property string hand: "right"
    property color textColor: "#c8c8d4"
    property color accentColor: "#dd6600"
    property var bindings: []
    width: 230
    spacing: 6
    function refresh() { bindings = controlsModel ? controlsModel.controllerBindings(hand) : [] }
    onControlsModelChanged: refresh()
    onHandChanged: refresh()
    Connections { target: root.controlsModel; function onChanged() { root.refresh() } }
    Text { text: root.hand === "left" ? "LEFT · X / Y" : "RIGHT · A / B"; color: root.textColor; font.pixelSize: 13; font.bold: true }
    Item {
        width: root.width; height: 280
        Image { anchors.fill: parent; source: root.controlsModel ? root.controlsModel.controllerImage(root.hand) : ""; fillMode: Image.PreserveAspectFit; smooth: true }
        Repeater {
            model: [
                {control:"primary", x:root.hand === "right" ? 0.67 : 0.33, y:0.48, label:root.hand === "right" ? "A" : "X"},
                {control:"secondary", x:root.hand === "right" ? 0.52 : 0.55, y:0.34, label:root.hand === "right" ? "B" : "Y"},
                {control:"thumbstick_click", x:root.hand === "right" ? 0.31 : 0.72, y:0.55, label:"Stick"},
                {control:"trigger", x:0.78, y:0.68, label:"Trigger"},
                {control:"grip", x:0.37, y:0.81, label:"Grip"}
            ]
            Rectangle {
                required property var modelData
                property var matching: root.bindings.filter(row => row.controllerControl === modelData.control)
                property bool active: matching.some(row => row.pressed)
                property bool used: matching.length > 0
                objectName: "controller-" + root.hand + "-" + modelData.control
                x: modelData.x * parent.width - width/2; y: modelData.y * parent.height - height/2
                width: Math.max(26, label.implicitWidth + 10); height: 27; radius: 13
                color: active ? root.accentColor : "#16161a"
                border.color: used ? root.accentColor : "#555568"
                border.width: used ? 2 : 1
                opacity: used ? 1 : 0.45
                Text { id: label; anchors.centerIn: parent; text: modelData.label; color: root.textColor; font.pixelSize: 11 }
                HoverHandler { id: hover }
                ToolTip.visible: hover.hovered && used
                ToolTip.text: matching.map(row => row.part + " → " + row.action + " (" + row.availability + ")").join("\n")
                Accessible.name: root.hand + " " + modelData.label + ": " + matching.map(row => row.action).join(", ")
            }
        }
    }
    Text { width: root.width; text: "Original simplified controller outline"; color: "#8a8f99"; font.pixelSize: 11; wrapMode: Text.Wrap }
}
