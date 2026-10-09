// SPDX-License-Identifier: GPL-3.0-only
import QtQuick
import QtQuick.Controls
import AladdinsCastle.Hub
ApplicationWindow {
 id:window
 width:1120
 height:720
 Component.onCompleted:{width=uiSettings.get("winWidth",1120);height=uiSettings.get("winHeight",720)}
 minimumWidth:500;minimumHeight:400
 visible:true;title:"AladdinsCastle"
 color:Theme.get("color.surface.window")
 Loader{anchors.fill:parent;source:typeof overlaySpikeEnabled!=="undefined"&&overlaySpikeEnabled?"SpikeRoot.qml":"HubRoot.qml"}
 onClosing:close=>{if(uiController.installing){close.accepted=false;confirm.open()}else if(uiController.scanning){close.accepted=false;uiController.cancelScan()}else {uiSettings.set("winWidth",width);uiSettings.set("winHeight",height)}}
 Dialog{id:confirm;anchors.centerIn:parent;modal:true;title:"Stop the install after this step?";standardButtons:Dialog.Cancel|Dialog.Ok;onAccepted:uiController.cancelInstall()}
}
