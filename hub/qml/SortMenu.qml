// SPDX-License-Identifier: GPL-3.0-only
import QtQuick
import QtQuick.Controls
import AladdinsCastle.Hub
Popup{
 id:menu
 objectName:"sortMenu"
 property bool vrOverlayMode:false
 width:vrOverlayMode?320:230;padding:12;modal:true;focus:true;closePolicy:Popup.CloseOnEscape|Popup.CloseOnPressOutside
 background:Rectangle{color:Theme.get("color.surface.panel");radius:Theme.get("radius.hero");border.color:Theme.get("color.line.strong")}
 Column{width:parent.width;spacing:5;UiText{text:"SORT EVERY GAME LIST";font.pixelSize:menu.vrOverlayMode?14:12;font.weight:700;font.letterSpacing:1.5;color:Theme.get("color.text.muted")}
  PillButton{objectName:"sortClose";vrOverlayMode:menu.vrOverlayMode;width:parent.width;text:"Close";onClicked:menu.close()}
  Repeater{model:[{id:"title",label:"Title · A–Z"},{id:"year",label:"Year · Oldest first"},{id:"manufacturer",label:"Manufacturer · A–Z"},{id:"hardware",label:"Hardware · Arcade, console, PC"},{id:"recent",label:"Recently played"},{id:"added",label:"Added to library"}];PillButton{vrOverlayMode:menu.vrOverlayMode;required property var modelData;width:parent.width;text:modelData.label;selected:gameFilter.sortMode===modelData.id;onClicked:{gameFilter.sortMode=modelData.id;menu.close()}}}
 }
}
