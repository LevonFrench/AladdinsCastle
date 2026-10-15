// SPDX-License-Identifier: GPL-3.0-only
import QtQuick
import QtQuick.Controls
import AladdinsCastle.Hub
// A drop-down in the Hub's own dark style, with rows large enough for the headset pointer.
ComboBox {
 id:combo
 property bool vrOverlayMode:false
 implicitHeight:vrOverlayMode?56:40
 font.family:Theme.get("font.family");font.pixelSize:vrOverlayMode?16:12
 background:Rectangle {radius:6;color:combo.down?Theme.get("color.surface.menu_hover"):Theme.get("color.surface.pill");border.width:combo.activeFocus?2:1;border.color:combo.activeFocus?Theme.get("color.glass.ring"):Theme.get("color.line.strong")}
 contentItem:Text {leftPadding:12;rightPadding:combo.indicator.width+8;text:combo.displayText;font:combo.font;color:Theme.get("color.text.primary");verticalAlignment:Text.AlignVCenter;elide:Text.ElideRight}
 indicator:Text {x:combo.width-width-12;y:(combo.height-height)/2;text:"▾";font.pixelSize:combo.font.pixelSize;color:Theme.get("color.text.inactive")}
 delegate:ItemDelegate {id:row;required property var modelData;required property int index;width:ListView.view?ListView.view.width:combo.width;height:combo.vrOverlayMode?56:40
  highlighted:combo.highlightedIndex===index
  contentItem:Text {text:row.modelData;font:combo.font;color:combo.currentIndex===row.index?Theme.get("color.state.ready_neon"):Theme.get("color.text.primary");verticalAlignment:Text.AlignVCenter;elide:Text.ElideRight}
  background:Rectangle {color:row.highlighted||row.hovered?Theme.get("color.surface.menu_hover"):"transparent"}}
 popup:Popup {y:combo.height+2;width:combo.width;padding:4;implicitHeight:Math.min(contentItem.implicitHeight+8,combo.vrOverlayMode?56*6:40*8)
  contentItem:ListView {clip:true;implicitHeight:contentHeight;model:combo.popup.visible?combo.delegateModel:null;currentIndex:combo.highlightedIndex;ScrollBar.vertical:ScrollBar{}}
  background:Rectangle {radius:6;color:Theme.get("color.surface.panel");border.color:Theme.get("color.line.button")}}
}
