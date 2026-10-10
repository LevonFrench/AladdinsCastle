// SPDX-License-Identifier: GPL-3.0-only
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Column {
    id: root
    objectName: "universalControlsView"
    property var controlsModel: null
    property color panelColor: "#16161a"
    property color textColor: "#c8c8d4"
    property color mutedColor: "#8a8f99"
    property color accentColor: "#dd6600"
    property int textSize: 14
    spacing: 12
    width: 1000
    Text { width: parent.width; text: root.controlsModel && root.controlsModel.title ? root.controlsModel.title : "CONTROLS"; color: root.textColor; font.pixelSize: 18; font.bold: true; wrapMode: Text.Wrap }
    Text { objectName: "controlsNotice"; width: parent.width; text: root.controlsModel ? root.controlsModel.notice : "Choose a game to see its controls"; color: root.mutedColor; font.pixelSize: root.textSize; wrapMode: Text.Wrap }
    Flow {
        width: parent.width; spacing: 14
        Column {
            width: Math.min(root.width, 500); spacing: 6
            Rectangle {
                width: parent.width; height: width
                color: root.panelColor; radius: 8
                Image {
                    id: gunPreview
                    objectName: "gunPreviewImage"
                    anchors.fill: parent; anchors.margins: 8
                    visible: root.controlsModel && root.controlsModel.previewAvailable
                    source: visible ? root.controlsModel.previewImage : ""
                    fillMode: Image.PreserveAspectFit; asynchronous: false
                }
                Text {
                    objectName: "gunPreviewPlaceholder"
                    anchors.centerIn: parent; width: parent.width - 40
                    visible: !gunPreview.visible
                    text: "Gun preview is not available yet.\nThe mappings below remain available for review."
                    horizontalAlignment: Text.AlignHCenter; wrapMode: Text.Wrap
                    color: root.mutedColor; font.pixelSize: root.textSize
                }
                Repeater {
                    model: gunPreview.visible && root.controlsModel ? root.controlsModel.markers : []
                    Rectangle {
                        required property var modelData
                        objectName: "gunCalloutMarker"
                        x: gunPreview.x + (gunPreview.width-gunPreview.paintedWidth)/2 + modelData.x*gunPreview.paintedWidth - width/2
                        y: gunPreview.y + (gunPreview.height-gunPreview.paintedHeight)/2 + modelData.y*gunPreview.paintedHeight - height/2
                        width: number.implicitWidth + 14; height: 27; radius: 13
                        color: modelData.pressed ? root.accentColor : "#16161a"
                        border.width: 2; border.color: root.accentColor
                        Text { id: number; anchors.centerIn: parent; text: modelData.numbers; font.pixelSize: 12; color: root.textColor }
                        HoverHandler { id: markerHover }
                        ToolTip.visible: markerHover.hovered
                        ToolTip.text: modelData.callout
                        Accessible.name: modelData.callout
                    }
                }
            }
            Row {
                spacing: 8
                Button { text: "Three-quarter"; checkable: true; checked: root.controlsModel ? root.controlsModel.previewView === "threequarter" : true; onClicked: if (root.controlsModel) root.controlsModel.previewView = "threequarter" }
                Button { text: "Side"; checkable: true; checked: root.controlsModel ? root.controlsModel.previewView === "front" : false; onClicked: if (root.controlsModel) root.controlsModel.previewView = "front" }
            }
        }
        Row {
            spacing: 12
            width: Math.min(root.width, 472)
            ControllerOutline { width: (parent.width-12)/2; controlsModel: root.controlsModel; hand: "left"; textColor: root.textColor; accentColor: root.accentColor }
            ControllerOutline { width: (parent.width-12)/2; controlsModel: root.controlsModel; hand: "right"; textColor: root.textColor; accentColor: root.accentColor }
        }
    }
    Text { width: parent.width; text: "Part → what it does in this game → your controller"; color: root.textColor; font.bold: true; font.pixelSize: root.textSize; wrapMode: Text.Wrap }
    Repeater {
        model: root.controlsModel ? root.controlsModel.rows : []
        Rectangle {
            id: callout
            required property var modelData
            required property int index
            objectName: "controlsCallout"
            width: root.width; height: description.implicitHeight + 26
            color: modelData.pressed ? "#35200c" : root.panelColor
            border.color: modelData.pressed ? root.accentColor : "#222230"
            border.width: modelData.pressed ? 2 : 1; radius: 6
            Column {
                id: description
                x: 12; y: 10; width: parent.width - 24; spacing: 4
                Text {
                    objectName: "controlsCalloutText"
                    width: parent.width
                    text: (callout.index+1) + ". " + callout.modelData.callout
                    color: root.textColor; font.pixelSize: root.textSize; wrapMode: Text.Wrap
                }
                Text {
                    width: parent.width
                    text: "Player " + (callout.modelData.player+1) + " · " + callout.modelData.availabilityLabel
                    color: root.mutedColor; font.pixelSize: root.textSize - 2; wrapMode: Text.Wrap
                }
                Text {
                    visible: !!callout.modelData.fallbackLabel
                    width: parent.width
                    text: "With two guns: " + callout.modelData.fallbackLabel
                    color: root.mutedColor; font.pixelSize: root.textSize - 2; wrapMode: Text.Wrap
                }
            }
            Accessible.name: modelData.callout + ". " + modelData.availability
        }
    }
}
