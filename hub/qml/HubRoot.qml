// SPDX-License-Identifier: GPL-3.0-only
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
Item {
    id: root
    implicitWidth: 1100
    implicitHeight: 760
    Rectangle { anchors.fill: parent; color: surfaceColor }
    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 18
        spacing: 10
        Label { text: "AladdinsCastle · " + gameFilter.visibleCount + " / " + catalogGameCount + " games"; color: primaryTextColor; font.pixelSize: 26; font.bold: true }
        RowLayout {
            Layout.fillWidth: true
            TextField { id: searchField; Layout.fillWidth: true; placeholderText: "Search titles, developers…  -publisher  +publisher"; onTextChanged: gameFilter.query = text }
            ComboBox { model: ["title", "year", "manufacturer", "hardware", "recent"]; onActivated: gameFilter.sortMode = currentText }
            Button { text: "Clear filters"; onClicked: { gameFilter.clearFacets(); searchField.clear(); filters.reset(); yearMin.value = 1970; yearMax.value = 2026; libraryToggle.checked = false; statePicker.currentIndex = 0 } }
        }
        Flow {
            id: filters
            Layout.fillWidth: true
            Layout.preferredHeight: childrenRect.height
            spacing: 6
            function reset() { for (let i = 0; i < children.length; ++i) if (children[i].currentIndex !== undefined) children[i].currentIndex = 0 }
            Repeater {
                model: [{name:"Genre",key:"genre"},{name:"Graphics",key:"graphicsIds"},{name:"Manufacturer",key:"manufacturerIds"},{name:"Hardware kind",key:"hardwareKinds"},{name:"Hardware family",key:"hardwareFamilies"},{name:"Board",key:"hardwareIds"},{name:"VR",key:"vrKeys"},{name:"Players",key:"playersBuckets"},{name:"Controls",key:"controlsTypes"},{name:"Decade",key:"decades"}]
                ComboBox {
                    required property var modelData
                    property string facetKey: modelData.key
                    width: 160
                    textRole: "label"
                    valueRole: "id"
                    model: [{id:"", label:modelData.name + ": All"}].concat(gameFilter.choices(facetKey))
                    onActivated: gameFilter.setFacet(facetKey, currentValue === "" ? [] : [currentValue])
                }
            }
        }
        RowLayout {
            Label { text: "Year"; color: primaryTextColor }
            SpinBox { id: yearMin; from: 1970; to: 2026; value: 1970; editable: true; onValueModified: gameFilter.setFacet("yearMin",value) }
            Label { text: "to"; color: primaryTextColor }
            SpinBox { id: yearMax; from: 1970; to: 2026; value: 2026; editable: true; onValueModified: gameFilter.setFacet("yearMax",value) }
            CheckBox { id: libraryToggle; text: "In my library"; enabled: gameFilter.scanComplete; onToggled: gameFilter.setFacet("inLibraryOnly",checked) }
            ComboBox { id: statePicker; enabled: gameFilter.scanComplete; model: ["All states","To install","Ready","Updates","Needs files"]; onActivated: gameFilter.setFacet("statePills",currentIndex === 0 ? [] : [["toInstall","ready","updates","needsFiles"][currentIndex-1]]) }
            Button { text: "Warnings (" + gameModel.warnings.length + ")"; onClicked: warnings.open() }
        }
        ListView {
            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true
            model: gameFilter
            reuseItems: true
            spacing: 4
            ScrollBar.vertical: ScrollBar {}
            delegate: Rectangle {
                required property string gameId
                required property string title
                required property int year
                required property string manufacturerLabel
                required property string hardwareLabel
                required property string stateLabel
                required property color colourBase
                required property color accent
                required property string loadWarning
                width: ListView.view.width
                height: 66
                color: colourBase
                border.color: accent
                radius: 5
                RowLayout {
                    anchors.fill: parent; anchors.margins: 10
                    ColumnLayout {
                        Layout.fillWidth: true
                        Label { text: title; color: primaryTextColor; font.pixelSize: 17; font.bold: true }
                        Label { text: year + " · " + manufacturerLabel + " · " + hardwareLabel; color: "#aaaaaa" }
                    }
                    Label { text: loadWarning.length ? "⚠ " + stateLabel : stateLabel; color: accent }
                }
            }
        }
    }
    Popup {
        id: warnings
        width: Math.min(root.width - 40, 900); height: Math.min(root.height - 40, 560)
        anchors.centerIn: parent; modal: true; closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside
        ScrollView { anchors.fill: parent; TextArea { text: gameModel.warnings.join("\n"); readOnly: true; wrapMode: Text.Wrap; selectByMouse: true } }
    }
}
