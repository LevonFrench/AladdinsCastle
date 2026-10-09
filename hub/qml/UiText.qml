// SPDX-License-Identifier: GPL-3.0-only
import QtQuick
import AladdinsCastle.Hub
Text {
 property string token: "type.detail_body.M"
 color: Theme.get("color.text.body")
 font.family: Theme.get("font.family")
 font.pixelSize: Theme.get(token) || 14
 wrapMode: Text.Wrap
}
