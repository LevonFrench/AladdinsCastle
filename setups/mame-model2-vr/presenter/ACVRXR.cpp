// SPDX-License-Identifier: GPL-3.0-or-later
// Model 2 compatibility adapter. The production implementation is linked once
// from acvr_publish_presenter; the existing white-box harness includes it here.
#include "ACVRStereo.h"
#ifdef ACVR_VIEWER_TEST
#include "../../../libacvr/publish-presenter/Presenter.cpp"
using namespace acvr_publish;
#endif
float ACVR_XR_DistanceMetres(){return acvr_publish::distance_metres();}
bool ACVR_XR_Start(void* window){return acvr_publish::start(window);}
void ACVR_XR_Publish(unsigned l,unsigned r,unsigned w,unsigned h){acvr_publish::publish(l,r,w,h);}
bool ACVR_XR_Stop(){return acvr_publish::stop();}
void ACVR_XR_QueueShotPulse(){acvr_publish::queue_shot_pulse();}
void ACVR_XR_UpdateFov(float l,float r,float b,float t){
 g_acvrStereo.tanL=l;g_acvrStereo.tanR=r;g_acvrStereo.tanB=b;g_acvrStereo.tanT=t;
 acvr_publish::update_fov(l,r,b,t);
}
ACVRControllerInput ACVR_XR_ControllerInput(){return acvr_publish::controller_input();}
