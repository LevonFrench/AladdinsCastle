// SPDX-License-Identifier: GPL-3.0-only
import QtQuick
import QtQuick.Controls
ApplicationWindow {
    width: overlaySpikeEnabled ? 1280 : 1000
    height: overlaySpikeEnabled ? 900 : 700
    visible: true
    title: "AladdinsCastle"
    HubRoot { anchors.fill: parent }
}
