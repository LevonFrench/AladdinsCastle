// SPDX-License-Identifier: GPL-3.0-only
import QtQuick
import QtQuick.Layouts
import AladdinsCastle.Hub
// The files the Game library looked at and could not identify, each with the reason in plain words. Shown only when there are some.
// It never offers to change or move a file.
Rectangle {
 id:card
 objectName:"unrecognisedFiles"
 property bool vrOverlayMode:false
 property int shown:20
 readonly property var files:uiController.unrecognisedFiles
 readonly property int total:uiController.unrecognisedCount
 function sizeText(bytes){return bytes>=1073741824?(bytes/1073741824).toFixed(1)+" GB":bytes>=1048576?Math.round(bytes/1048576)+" MB":Math.max(1,Math.round(bytes/1024))+" KB"}
 visible:total>0
 radius:Theme.get("radius.card");color:Theme.get("color.surface.card")
 height:visible?column.implicitHeight+(vrOverlayMode?40:32):0
 Column {
  id:column;x:card.vrOverlayMode?20:16;y:card.vrOverlayMode?20:16;width:parent.width-2*x;spacing:8
  UiText {objectName:"unrecognisedTitle";width:parent.width;font.weight:700;font.pixelSize:card.vrOverlayMode?20:17;color:Theme.get("color.text.primary")
   text:card.total===1?"1 file was not recognised":card.total+" files were not recognised"}
  UiText {width:parent.width;font.pixelSize:card.vrOverlayMode?16:14;color:Theme.get("color.text.muted_detail")
   text:"The Hub reads what is inside a file, never its name. These are left as they are."}
  Repeater {
   model:card.files.slice(0,card.shown)
   Column {
    required property var modelData
    objectName:"unrecognisedRow";width:column.width;spacing:2
    UiText {width:parent.width;elide:Text.ElideRight;maximumLineCount:1;font.weight:600;font.pixelSize:card.vrOverlayMode?16:14;text:modelData.name+(modelData.size?"  ·  "+card.sizeText(modelData.size):"")}
    UiText {width:parent.width;font.pixelSize:card.vrOverlayMode?15:13;color:Theme.get("color.text.muted_detail");text:modelData.reason||""}
   }
  }
  UiText {objectName:"unrecognisedMore";visible:card.total>card.shown;width:parent.width;font.pixelSize:card.vrOverlayMode?15:13;color:Theme.get("color.text.muted_detail")
   text:card.files.length>card.shown?"and "+(card.total-card.shown)+" more.":"and "+(card.total-card.files.length)+" more that are not listed."}
  PillButton {objectName:"unrecognisedShowMore";visible:card.files.length>card.shown;vrOverlayMode:card.vrOverlayMode;text:"Show more";onClicked:card.shown=Math.min(card.files.length,card.shown+40)}
 }
}
