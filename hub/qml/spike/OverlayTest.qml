// SPDX-License-Identifier: GPL-3.0-only
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Effects
Item {
    id: root
    property bool overlayPresentation: false
    property int selectedCard: -1
    Rectangle { anchors.fill: parent; color: surfaceColor }
    ColumnLayout {
        anchors.fill: parent; anchors.margins: 24; spacing: 12
        RowLayout {
            Layout.fillWidth: true
            Label { text: "AladdinsCastle · S1–S3"; color: primaryTextColor; font.pixelSize: 28; font.bold: true }
            Item { Layout.fillWidth: true }
            Label { text: root.overlayPresentation ? "DASHBOARD" : "DESKTOP"; color: brandColor }
        }
        Label { text: "TOP LEFT · 1280 × 900 · 50 original synthetic cards"; color: "#b9c0d0" }
        RowLayout {
            Button { objectName: "spikeClickButton"; text: "Test click · " + spikeState.clickCount; onClicked: spikeState.clicked() }
            Button { text: "Reset text"; onClicked: spikeState.text = "" }
            CheckBox { text: "Flip mouse Y"; checked: spikeState.flipY; onToggled: spikeState.flipY = checked }
            CheckBox { text: "50 glows"; checked: spikeState.glow; onToggled: spikeState.glow = checked }
            CheckBox { text: "Animate glows"; checked: spikeState.animate; onToggled: spikeState.animate = checked }
        }
        Rectangle {
            Layout.fillWidth: true; Layout.preferredHeight: 54; radius: 6
            color: "#1d2530"; border.color: editor.activeFocus ? brandColor : "#667080"
            TextInput {
                id: editor; objectName: "spikeTextInput"
                anchors.fill: parent; anchors.margins: 12
                color: primaryTextColor; font.pixelSize: 24; clip: true
                maximumLength: 256; selectByMouse: true
                text: spikeState.text
                onTextEdited: spikeState.text = text
                onActiveFocusChanged: {
                    if (activeFocus && root.overlayPresentation) overlayHost.requestKeyboard(editor)
                }
                Label { anchors.fill: parent; visible: !editor.text.length; text: "Click to type with SteamVR keyboard"; color: "#909bab" }
            }
            Connections {
                target: spikeState
                function onTextChanged() { if (editor.text !== spikeState.text) editor.text = spikeState.text }
            }
        }
        Label { Layout.fillWidth: true; text: spikeState.status; color: "#b9c0d0"; wrapMode: Text.WordWrap }
        ColumnLayout {
            Layout.fillWidth: true
            RowLayout {
                Label { text: "In-scene fallback keyboard"; color: primaryTextColor }
                Button { text: "Done"; onClicked: { editor.focus = false; if (root.overlayPresentation) overlayHost.dismissKeyboard() } }
            }
            Flow {
                Layout.fillWidth: true; Layout.preferredHeight: implicitHeight
                spacing: 4
                Repeater {
                    model: "abcdefghijklmnopqrstuvwxyz0123456789".split("").concat(["-", "Space", "⌫"])
                    Button {
                        required property string modelData
                        text: modelData; width: modelData === "Space" ? 76 : 38; height: 34
                        onClicked: {
                            if (modelData === "⌫") spikeState.text = Array.from(spikeState.text).slice(0, -1).join("")
                            else if (spikeState.text.length < 256) spikeState.text += modelData === "Space" ? " " : modelData
                        }
                    }
                }
            }
        }
        Flickable {
            id: scroll; objectName: "spikeGrid"
            Layout.fillWidth: true; Layout.fillHeight: true; clip: true
            contentWidth: width; contentHeight: cards.height + 24
            boundsBehavior: Flickable.StopAtBounds
            ScrollBar.vertical: ScrollBar { }
            GridLayout {
                id: cards; width: scroll.width - 18; columns: 5
                columnSpacing: 18; rowSpacing: 18
                Repeater {
                    model: 50
                    Item {
                        required property int index
                        objectName: "syntheticCard"
                        Layout.fillWidth: true; Layout.preferredHeight: 160
                        Rectangle {
                            id: card; anchors.fill: parent; anchors.margins: 8; radius: 8
                            color: Qt.hsla((index % 10) / 10, 0.40, 0.18, 1)
                            border.width: 2; border.color: root.selectedCard === index ? "#ffffff" : brandColor
                            Column {
                                anchors.centerIn: parent; spacing: 8
                                Label { anchors.horizontalCenter: parent.horizontalCenter; text: "◇ " + (index + 1); color: brandColor; font.pixelSize: 34 }
                                Label { text: "Synthetic card " + (index + 1); color: primaryTextColor; font.pixelSize: 17 }
                            }
                            MouseArea { anchors.fill: parent; onClicked: { root.selectedCard = index; spikeState.clicked() } }
                        }
                        MultiEffect {
                            anchors.fill: card; source: card
                            visible: spikeState.glow
                            shadowEnabled: true; shadowColor: brandColor
                            shadowBlur: 0.6; shadowOpacity: 0.55
                            shadowHorizontalOffset: 0; shadowVerticalOffset: 0
                            NumberAnimation on shadowOpacity {
                                from: 0.25; to: 0.8; duration: 1000
                                loops: Animation.Infinite
                                running: spikeState.animate && spikeState.glow && root.visible
                            }
                        }
                    }
                }
            }
        }
        RowLayout {
            Label { text: "BOTTOM LEFT · wheel-scroll the grid"; color: "#b9c0d0" }
            Item { Layout.fillWidth: true }
            Button { text: "Bottom-right click · " + spikeState.clickCount; onClicked: spikeState.clicked() }
        }
    }
}
