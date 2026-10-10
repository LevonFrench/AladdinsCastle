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
    Repeater {
        model: 4
        Button {
            required property int index
            objectName: "spikeCorner" + index
            x: index % 2 ? root.width - width - 24 : 24
            y: index < 2 ? 24 : root.height - height - 24
            width: 164; height: 56
            text: ["TOP LEFT", "TOP RIGHT", "BOTTOM LEFT", "BOTTOM RIGHT"][index] + " · " + spikeState.cornerHits[index]
            onClicked: spikeState.hitCorner(index)
        }
    }
    Label {
        x: 212; y: 28; width: root.width - 424; height: 56
        text: "AladdinsCastle · input diagnostics\n1280 × 800 · " + (root.overlayPresentation ? "DASHBOARD" : "DESKTOP") + " · observations, not routing fixes"
        color: primaryTextColor; horizontalAlignment: Text.AlignHCenter; font.pixelSize: 18
    }
    ColumnLayout {
        anchors.fill: parent; anchors.leftMargin: 24; anchors.rightMargin: 24
        anchors.topMargin: 96; anchors.bottomMargin: 96; spacing: 8
        RowLayout {
            spacing: 8
            Button { implicitHeight:48; text:"Reset text"; onClicked:spikeState.text="" }
            CheckBox { implicitHeight:48; text:"Flip mouse Y"; checked:spikeState.flipY; onToggled:spikeState.flipY=checked }
            CheckBox { implicitHeight:48; text:"50 glows"; checked:spikeState.glow; onToggled:spikeState.glow=checked }
            CheckBox { implicitHeight:48; text:"Animate glows"; checked:spikeState.animate; onToggled:spikeState.animate=checked }
            Label { Layout.fillWidth:true; text:"Last Qt pointer: cursor " + spikeState.pointerCursor + " (" + spikeState.pointerPosition.x.toFixed(1) + ", " + spikeState.pointerPosition.y.toFixed(1) + ")"; color:primaryTextColor }
        }
        ListView {
            objectName:"spikeInputObservations"
            Layout.fillWidth:true; Layout.preferredHeight:64; clip:true
            model:spikeState.cursors
            ScrollBar.vertical:ScrollBar {}
            delegate:Label {
                required property var modelData
                width:ListView.view.width; height:32; color:"#b9c0d0"; font.pixelSize:11
                text:"Cursor " + modelData.cursor + " raw event " + modelData.rawEvent + " → Qt " + modelData.qtEvent
                     + " last mouse raw (" + (modelData.rawX||0) + ", " + (modelData.rawY||0) + ") → Qt (" + modelData.qtX + ", " + modelData.qtY + ") buttons " + modelData.qtButtons
                     + " · move " + (modelData.moves||0) + " / press " + (modelData.presses||0) + " / release " + (modelData.releases||0)
                     + "\nSmooth " + (modelData.smooth||0) + " raw Δ(" + (modelData.smoothRawX||0) + ", " + (modelData.smoothRawY||0) + ") Qt angle Δ(" + (modelData.smoothQtX||0) + ", " + (modelData.smoothQtY||0) + ")"
                     + " · discrete " + (modelData.discrete||0) + " raw Δ(" + (modelData.discreteRawX||0) + ", " + (modelData.discreteRawY||0) + ") Qt angle Δ(" + (modelData.discreteQtX||0) + ", " + (modelData.discreteQtY||0) + ")"
            }
        }
        RowLayout {
            Layout.fillWidth:true; spacing:8
            Rectangle {
                Layout.fillWidth:true; Layout.preferredHeight:54; radius:6
                color:"#1d2530"; border.color:editor.activeFocus?brandColor:"#667080"
                TextInput {
                    id:editor; objectName:"spikeTextInput"
                    anchors.fill:parent; anchors.margins:12
                    color:primaryTextColor; font.pixelSize:24; clip:true
                    maximumLength:256; selectByMouse:true; text:spikeState.text
                    onTextEdited:spikeState.text=text
                    Label { anchors.fill:parent; visible:!editor.text.length; text:"Transient text · use the keyboard button or panel keys"; color:"#909bab" }
                }
                Connections { target:spikeState; function onTextChanged(){if(editor.text!==spikeState.text)editor.text=spikeState.text} }
            }
            Button {
                objectName:"spikeKeyboardRequest"; implicitHeight:54
                text:"Open SteamVR keyboard"; enabled:root.overlayPresentation
                onClicked:overlayHost.requestKeyboard(editor)
            }
        }
        Label { Layout.fillWidth:true; text:spikeState.status + (spikeState.droppedPackets?" · Dropped packet observations: "+spikeState.droppedPackets:""); color:"#b9c0d0"; elide:Text.ElideRight }
        RowLayout {
            Layout.fillWidth:true; spacing:8
            Label { Layout.fillWidth:true; text:"On-panel keyboard · every key updates the transient field above"; color:primaryTextColor }
            Button { objectName:"spikeKeyboardDone"; implicitHeight:48; text:"Done"; onClicked:{editor.focus=false;if(root.overlayPresentation)overlayHost.dismissKeyboard()} }
        }
        Flow {
            Layout.fillWidth:true; Layout.preferredHeight:implicitHeight; spacing:8
            Repeater {
                model:"abcdefghijklmnopqrstuvwxyz0123456789".split("").concat(["-", "Space", "⌫"])
                Button {
                    required property string modelData
                    objectName:"spikePanelKey"; text:modelData
                    width:modelData==="Space"?76:44; height:56
                    onClicked:{
                        if(modelData==="⌫")spikeState.text=Array.from(spikeState.text).slice(0,-1).join("")
                        else if(spikeState.text.length<256)spikeState.text+=modelData==="Space"?" ":modelData
                    }
                }
            }
        }
        Label { objectName:"spikeScrollPosition"; text:"Long list · contentY " + scroll.contentY.toFixed(1) + " / " + Math.max(0,scroll.contentHeight-scroll.height).toFixed(1); color:primaryTextColor }
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
    }
    Rectangle {
        objectName:"spikePointer"; z:20; visible:spikeState.hasPointer
        width:16; height:16; radius:8; color:"transparent"; border.width:2; border.color:"#ffff66"
        x:spikeState.pointerPosition.x-width/2; y:spikeState.pointerPosition.y-height/2
        Label { x:18; text:"Qt · " + spikeState.pointerCursor; color:"#ffff66"; font.pixelSize:12 }
    }
}
