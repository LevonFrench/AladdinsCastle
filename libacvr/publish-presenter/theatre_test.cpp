// SPDX-License-Identifier: GPL-3.0-or-later
// Injected API/GL tests: no loader, runtime, desktop capture or input automation.
#include "Presenter.cpp"
#include <cassert>
#include <map>
#include <string>
using namespace acvr_publish;
namespace {
std::map<std::string,XrPath> paths;
unsigned profiles=0,syncCalls=0,draws=0,updates=0,acquires=0,releases=0;
bool timeout=false,tracked=true;
int clickMode=0; // 0: both clicks held, 1: left click only
GLuint textures[2]{};unsigned created=0;
XrResult XRAPI_CALL Path(XrInstance,const char* name,XrPath* out){auto p=paths.emplace(name,paths.size()+1);*out=p.first->second;return XR_SUCCESS;}
XrResult XRAPI_CALL Set(XrInstance,const XrActionSetCreateInfo*,XrActionSet* out){*out=reinterpret_cast<XrActionSet>(1);return XR_SUCCESS;}
XrResult XRAPI_CALL Action(XrActionSet,const XrActionCreateInfo* info,XrAction* out){static uintptr_t id=10;*out=reinterpret_cast<XrAction>(++id);if(std::string(info->actionName)=="second_button")assert(info->actionType==XR_ACTION_TYPE_FLOAT_INPUT);return XR_SUCCESS;}
XrResult XRAPI_CALL Suggest(XrInstance,const XrInteractionProfileSuggestedBinding* info){
 ++profiles;assert(info->countSuggestedBindings==11);
 assert(info->suggestedBindings[3].action==s_grip);assert(info->suggestedBindings[4].action==s_secondary);
 assert(info->suggestedBindings[5].action==s_leftGrip && info->suggestedBindings[5].binding==paths.at("/user/hand/left/input/grip/pose"));
 assert(info->suggestedBindings[6].action==s_leftTrigger && info->suggestedBindings[6].binding==paths.at("/user/hand/left/input/trigger/value"));
 assert(info->suggestedBindings[7].action==s_leftStick && info->suggestedBindings[7].binding==paths.at("/user/hand/left/input/thumbstick"));
 assert(info->suggestedBindings[8].action==s_rightStick && info->suggestedBindings[8].binding==paths.at("/user/hand/right/input/thumbstick"));
 // Universal layer: Start and Coin are the stick clicks.
 assert(info->suggestedBindings[9].action==s_leftClick && info->suggestedBindings[9].binding==paths.at("/user/hand/left/input/thumbstick/click"));
 assert(info->suggestedBindings[10].action==s_rightClick && info->suggestedBindings[10].binding==paths.at("/user/hand/right/input/thumbstick/click"));
 // Face buttons, the menu and the system buttons are never requested in theatre mode.
 for(const char* off:{"/user/hand/right/input/a/click","/user/hand/right/input/b/click","/user/hand/left/input/y/click","/user/hand/left/input/x/click","/user/hand/left/input/menu/click","/user/hand/right/input/system/click"})assert(!paths.count(off));
 return XR_SUCCESS;
}
XrResult XRAPI_CALL Attach(XrSession,const XrSessionActionSetsAttachInfo*){return XR_SUCCESS;}
XrResult XRAPI_CALL Space(XrSession,const XrActionSpaceCreateInfo*,XrSpace* out){static uintptr_t id=20;*out=reinterpret_cast<XrSpace>(++id);return XR_SUCCESS;}
XrResult XRAPI_CALL Sync(XrSession,const XrActionsSyncInfo*){++syncCalls;return XR_SUCCESS;}
XrResult XRAPI_CALL Pose(XrSession,const XrActionStateGetInfo*,XrActionStatePose* out){out->isActive=true;return XR_SUCCESS;}
XrResult XRAPI_CALL Float(XrSession,const XrActionStateGetInfo*,XrActionStateFloat* out){out->isActive=true;out->currentState=.8f;return XR_SUCCESS;}
XrResult XRAPI_CALL Stick(XrSession,const XrActionStateGetInfo* info,XrActionStateVector2f* out){out->isActive=true;out->currentState={info->subactionPath==s_left?-.5f:.25f,.75f};return XR_SUCCESS;}
XrResult XRAPI_CALL Button(XrSession,const XrActionStateGetInfo* info,XrActionStateBoolean* out){out->isActive=true;out->currentState=clickMode==0||info->subactionPath==s_left;return XR_SUCCESS;}
XrResult XRAPI_CALL Locate(XrSpace,XrSpace,XrTime time,XrSpaceLocation* out){assert(time==123);out->locationFlags=XR_SPACE_LOCATION_POSITION_VALID_BIT|XR_SPACE_LOCATION_ORIENTATION_VALID_BIT;if(tracked)out->locationFlags|=XR_SPACE_LOCATION_POSITION_TRACKED_BIT|XR_SPACE_LOCATION_ORIENTATION_TRACKED_BIT;out->pose.orientation.w=1;out->pose.position={.1f,-.2f,-.3f};return XR_SUCCESS;}
XrResult XRAPI_CALL Views(XrSession,const XrViewLocateInfo* info,XrViewState* state,uint32_t,uint32_t* count,XrView* views){
 assert(info->displayTime==123);*count=2;state->viewStateFlags=XR_VIEW_STATE_POSITION_VALID_BIT|XR_VIEW_STATE_ORIENTATION_VALID_BIT;
 for(unsigned e=0;e<2;++e){views[e].pose.orientation.w=1;views[e].pose.position.x=e?.032f:-.032f;views[e].fov={-.7f,.7f,.7f,-.7f};}return XR_SUCCESS;
}
XrResult XRAPI_CALL Extents(XrInstance,XrSystemId,XrViewConfigurationType,uint32_t,uint32_t* count,XrViewConfigurationView* views){*count=2;views[0].recommendedImageRectWidth=64;views[0].recommendedImageRectHeight=48;views[1].recommendedImageRectWidth=80;views[1].recommendedImageRectHeight=56;return XR_SUCCESS;}
XrResult XRAPI_CALL Resolve(XrInstance,const char* name,PFN_xrVoidFunction* out){assert(std::string(name)=="xrEnumerateViewConfigurationViews");*out=reinterpret_cast<PFN_xrVoidFunction>(Extents);return XR_SUCCESS;}
XrResult XRAPI_CALL Chain(XrSession,const XrSwapchainCreateInfo* info,XrSwapchain* out){unsigned e=created++;assert(e<2);assert(info->width==s_theatreW[e] && info->height==s_theatreH[e]);glGenTextures(1,&textures[e]);glBindTexture(GL_TEXTURE_2D,textures[e]);glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA8,info->width,info->height,0,GL_RGBA,GL_UNSIGNED_BYTE,nullptr);*out=reinterpret_cast<XrSwapchain>(uintptr_t(e+1));return XR_SUCCESS;}
XrResult XRAPI_CALL Images(XrSwapchain chain,uint32_t capacity,uint32_t* count,XrSwapchainImageBaseHeader* out){*count=1;if(capacity)reinterpret_cast<XrSwapchainImageOpenGLKHR*>(out)->image=textures[uintptr_t(chain)-1];return XR_SUCCESS;}
XrResult XRAPI_CALL Acquire(XrSwapchain,const XrSwapchainImageAcquireInfo*,uint32_t* index){++acquires;*index=0;return XR_SUCCESS;}
XrResult XRAPI_CALL Wait(XrSwapchain,const XrSwapchainImageWaitInfo* info){assert(info->timeout==10000000);if(timeout){Sleep(10);return XR_TIMEOUT_EXPIRED;}return XR_SUCCESS;}
XrResult XRAPI_CALL Release(XrSwapchain,const XrSwapchainImageReleaseInfo*){++releases;return XR_SUCCESS;}
XrResult XRAPI_CALL Destroy(XrSwapchain){return XR_SUCCESS;}
struct Client: TheatreClient {
 GLuint fbo=0;
 void initialize()override{glGenFramebuffers(1,&fbo);}
 bool update(const TheatreFrame&)override{++updates;return true;}
 void draw_eye(unsigned eye,unsigned colour,const TheatreFrame& frame,bool)override{
  ++draws;assert(frame.width[eye]==s_theatreW[eye]);assert(frame.views.frame_time==123);
  glBindFramebuffer(GL_FRAMEBUFFER,fbo);glFramebufferTexture2D(GL_FRAMEBUFFER,GL_COLOR_ATTACHMENT0,GL_TEXTURE_2D,colour,0);
  glViewport(0,0,frame.width[eye],frame.height[eye]);glClearColor(eye?0:1,eye?1:0,0,1);glClear(GL_COLOR_BUFFER_BIT);
 }
 void shutdown()override{glDeleteFramebuffers(1,&fbo);}
};
}
int main(){
 assert(!GetModuleHandleA("openxr_loader.dll"));
 auto* window=Host_CreateWindow("offline theatre fixture",0,0,16,16,Host_WINDOW_OPENGL|Host_WINDOW_HIDDEN);
 assert(window && Host_GL_CreateContext(window) && Host_LoadGL());
 Client client;client.initialize();s_theatre=&client;
 s_instance=reinterpret_cast<XrInstance>(1);s_session=reinterpret_cast<XrSession>(1);s_space=reinterpret_cast<XrSpace>(1);s_format=GL_RGBA8;
 p_xrStringToPath=Path;p_xrCreateActionSet=Set;p_xrCreateAction=Action;p_xrSuggestInteractionProfileBindings=Suggest;p_xrAttachSessionActionSets=Attach;p_xrCreateActionSpace=Space;
 assert(InitActions() && profiles==2 && s_gripSpace!=s_aimSpace && s_leftGripSpace && s_leftGripSpace!=s_gripSpace);
 p_xrGetInstanceProcAddr=Resolve;assert(TheatreExtents());assert(s_theatreW[0]==64 && s_theatreW[1]==80 && s_theatreH[0]==48 && s_theatreH[1]==56);
 p_xrLocateViews=Views;p_xrSyncActions=Sync;p_xrGetActionStatePose=Pose;p_xrGetActionStateFloat=Float;p_xrGetActionStateBoolean=Button;p_xrGetActionStateVector2f=Stick;p_xrLocateSpace=Locate;
 auto frame=TheatreState(123,true);assert(frame.views.valid && frame.aim.valid && frame.grip.valid && frame.trigger==.8f && frame.button2);
 assert(!frame.start && !frame.coin && frame.quitHeld); // both clicks at once are a chord: no Start or Coin, quit hold running
 assert(frame.leftGrip.valid && frame.leftTrigger==.8f && frame.squeeze==.8f && frame.leftStick[0]==-.5f && frame.leftStick[1]==.75f && frame.rightStick[0]==.25f);
 const auto count=syncCalls;auto unfocused=TheatreState(123,false);assert(unfocused.views.valid && !unfocused.leftGrip.valid && !unfocused.leftTrigger && !unfocused.leftStick[0] && !unfocused.aim.valid && !unfocused.trigger && !unfocused.start && !unfocused.coin && !unfocused.quitHeld && syncCalls==count);
 tracked=false;auto lost=TheatreState(123,true);assert(!lost.aim.valid && !lost.grip.valid && !lost.trigger);tracked=true;
 // U1: the left click alone is Start, held back for the 150 ms chord window, never Coin and never a quit hold.
 s_chord.reset();clickMode=1;auto solo=TheatreState(123,true);assert(!solo.start && !solo.coin && !solo.quitHeld);
 Sleep(170);solo=TheatreState(123,true);assert(solo.start && !solo.coin && !solo.quitHeld);clickMode=0;s_chord.reset();
 p_xrCreateSwapchain=Chain;p_xrEnumerateSwapchainImages=Images;p_xrAcquireSwapchainImage=Acquire;p_xrWaitSwapchainImage=Wait;p_xrReleaseSwapchainImage=Release;p_xrDestroySwapchain=Destroy;
 assert(!RenderTheatre(frame,false) && updates==1 && draws==0 && acquires==0);
 timeout=true;assert(!RenderTheatre(frame,true));assert(acquires==1 && releases==0 && s_acquired[0] && !s_imageReady[0]);
 timeout=false;assert(RenderTheatre(frame,true));assert(draws==2 && acquires==2 && releases==2);
 for(unsigned eye=0;eye<2;++eye){std::vector<uint32_t> px(s_theatreW[eye]*s_theatreH[eye]);glBindTexture(GL_TEXTURE_2D,textures[eye]);glGetTexImage(GL_TEXTURE_2D,0,GL_RGBA,GL_UNSIGNED_BYTE,px.data());assert(px.front()==(eye?0xff00ff00u:0xff0000ffu));}
 assert(RenderTheatre(unfocused,true));assert(draws==4); // drawing independent of focus
 client.shutdown();DestroyChains();glDeleteTextures(2,textures);s_theatre=nullptr;s_shotPulses=0;
 assert(glGetError()==GL_NO_ERROR && !GetModuleHandleA("openxr_loader.dll"));Host_DestroyWindow(window);
 std::puts("PASS theatre actions (left grip, left trigger and both thumbsticks included), per-eye extents, tracked poses, focus release, zero-draw update, bounded wait retry and opaque render inputs; no loader");
}
