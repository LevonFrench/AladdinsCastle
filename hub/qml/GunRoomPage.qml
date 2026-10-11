// SPDX-License-Identifier: GPL-3.0-only
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Dialogs
import AladdinsCastle.Hub
Flickable {
 id:page
 objectName:"gunRoomPage"
 contentWidth:width;contentHeight:form.implicitHeight;clip:true
 ScrollBar.vertical:ScrollBar{}
 property string executable:(uiSettings.values,uiSettings.get("gunRoomExecutable",""))
 function modelId(){return model.currentValue===undefined?"":model.currentValue}
 function handId(){return hand.currentValue===undefined?"":hand.currentValue}
 function preview(){if(typeof hubServices!=="undefined")hubServices.previewGunRoom(modelId(),handId(),executable)}
 onExecutableChanged:preview()
 ColumnLayout {
  id:form;width:Math.min(page.width,650);spacing:16
  UiText {Layout.fillWidth:true;text:uiController.appName+" Gun Room";font.pixelSize:24}
  UiText {Layout.fillWidth:true;wrapMode:Text.Wrap;text:"Inspect gun and hand models in VR. Game actions are unconfirmed until a game route provides them."}
  UiText {text:"Gun model"}
  ComboBox {id:model;objectName:"gunRoomModel";Layout.fillWidth:true;model:typeof hubServices!=="undefined"?hubServices.gunRoomModels:[];textRole:"title";valueRole:"id";currentIndex:-1;displayText:currentIndex<0?"Choose a gun model":currentText;onCurrentValueChanged:page.preview()}
  UiText {text:"Primary hand"}
  ComboBox {id:hand;objectName:"gunRoomHand";Layout.fillWidth:true;model:[{title:"Left hand",id:"left"},{title:"Right hand",id:"right"}];textRole:"title";valueRole:"id";currentIndex:-1;displayText:currentIndex<0?"Choose a hand":currentText;onCurrentValueChanged:page.preview()}
  PillButton {text:"Choose reviewed gun-room executable";onClicked:binary.open()}
  UiText {Layout.fillWidth:true;wrapMode:Text.Wrap;font.pixelSize:12;text:page.executable||"No gun-room executable selected."}
  PillButton {text:"Choose existing OpenXR loader";onClicked:loader.open()}
  UiText {Layout.fillWidth:true;wrapMode:Text.Wrap;font.pixelSize:12;text:(uiSettings.values,uiSettings.get("gunRoomLoader",""))||"Use the configured loader or the Windows system loader."}
  UiText {objectName:"gunRoomUnavailable";Layout.fillWidth:true;wrapMode:Text.Wrap;text:typeof hubServices==="undefined"?"Gun room service is unavailable.":hubServices.gunRoomModels.length===0?"Gun and hand model data is unavailable in this data root.":hubServices.gunRoomReason}
  PillButton {objectName:"gunRoomLaunch";text:"Open gun room";enabled:model.currentIndex>=0&&hand.currentIndex>=0&&typeof hubServices!=="undefined"&&hubServices.gunRoomAvailable;onClicked:hubServices.launchGunRoom(page.modelId(),page.handId(),page.executable)}
  UiText {Layout.fillWidth:true;wrapMode:Text.Wrap;visible:typeof hubServices!=="undefined"&&hubServices.gunRoomAvailable&&!hubServices.gunRoomCalibrationAvailable;text:typeof hubServices!=="undefined"?hubServices.gunRoomCalibrationReason:""}
  UiText {Layout.fillWidth:true;wrapMode:Text.Wrap;visible:typeof hubServices!=="undefined"&&hubServices.gunRoomCalibrationAvailable;text:"Keep the trigger released. Hold B (right hand) or Y (left hand) with the stick centred for 0.8 seconds, then release to edit. Use the stick to adjust yaw and pitch. Hold for 1.2 seconds and release to save; a short tap cancels. The room saves this model and hand only. Exit cancels unsaved changes."}
  PillButton {objectName:"gunRoomCalibrate";text:"Calibrate grip in VR";visible:typeof hubServices!=="undefined"&&hubServices.gunRoomCalibrationAvailable;enabled:model.currentIndex>=0&&hand.currentIndex>=0&&visible;onClicked:hubServices.calibrateGunRoom(page.modelId(),page.handId(),page.executable)}
 }
 FileDialog {id:binary;title:"Choose reviewed gun-room executable";nameFilters:["Gun room executable (*.exe)"];onAccepted:{uiSettings.set("gunRoomExecutable",uiController.localPath(selectedFile));page.preview()}}
 FileDialog {id:loader;title:"Choose existing OpenXR loader";nameFilters:["OpenXR loader (*.dll)"];onAccepted:{uiSettings.set("gunRoomLoader",uiController.localPath(selectedFile));page.preview()}}
 Component.onCompleted:preview()
}
