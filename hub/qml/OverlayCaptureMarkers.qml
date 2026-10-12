// SPDX-License-Identifier: GPL-3.0-only
import QtQuick
Item {
    id:root
    objectName:"overlayCaptureMarkers"
    // Passive, opaque corner witnesses; no input handler or automatic flip.
    Repeater {
        model:4
        Rectangle {
            required property int index
            objectName:"overlayCaptureCorner"+index
            width:92;height:36
            x:index%2?root.width-width-12:12
            y:index<2?12:root.height-height-12
            color:["#ff00ff","#00ff00","#0000ff","#ff8800"][index]
            Text {
                objectName:"overlayCaptureLabel"+parent.index
                anchors.centerIn:parent
                text:["TL 1","TR 2","BL 3","BR 4"][parent.index]
                color:parent.index===2?"white":"black"
                font.pixelSize:18;font.bold:true
            }
        }
    }
}
