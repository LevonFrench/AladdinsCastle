// SPDX-License-Identifier: GPL-3.0-only
import QtQuick
import QtQuick.Controls
import AladdinsCastle.Hub
Popup{
 id:menu
 property bool vrOverlayMode:false
 width:vrOverlayMode?320:230;padding:12;modal:true;closePolicy:Popup.CloseOnEscape|Popup.CloseOnPressOutside
 background:Rectangle{color:Theme.get("color.surface.panel");radius:8;border.color:Theme.get("color.line.button")}
 Column{width:parent.width;spacing:5;UiText{text:"SORT EVERY GAME LIST";font.pixelSize:10;color:Theme.get("color.text.menu_caption")}
  Repeater{model:[{id:"title",label:"Title · A–Z"},{id:"year",label:"Year · Oldest first"},{id:"manufacturer",label:"Manufacturer · A–Z"},{id:"hardware",label:"Hardware · Arcade, console, PC"},{id:"recent",label:"Recently played"},{id:"added",label:"Added to library"}];PillButton{vrOverlayMode:menu.vrOverlayMode;required property var modelData;width:parent.width;text:modelData.label;selected:gameFilter.sortMode===modelData.id;onClicked:{gameFilter.sortMode=modelData.id;menu.close()}}}
 }
 WheelHandler{onWheel:menu.close()}
}
