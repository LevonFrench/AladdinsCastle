// SPDX-License-Identifier: GPL-3.0-or-later
// Builds the real implementation with test-only offline callbacks. No XR loader
// or runtime is opened. All GL windows are hidden; no desktop pixels are read.
#define ACVR_VIEWER_TEST
#include "ACVRXR.cpp"
#include "ShotFeedback.h"
#include <cassert>
#include <chrono>
#include <map>
#include <string>
#include <stdexcept>
ACVRStereoState g_acvrStereo;
using Clock=std::chrono::steady_clock;

namespace {
std::atomic<std::uint64_t> startedSerial{0},checkedSerial{0},consumerToken{0},gpuDoneToken{0};
std::atomic<bool> consuming{false},denyForeign{false};
HGLRC deniedContext=nullptr;
unsigned cleanupCalls=0,checkedPairs=0,blockedReuses=0;
GLuint destination[2]{};
unsigned destW=0,destH=0;
bool AllowContext(HGLRC context) { return (!context || context!=deniedContext) && (!denyForeign || !context || context==s_ownerContext); }
void ConsumerDone() { gpuDoneToken=consumerToken.load(); consuming=false; }
void Deadline(const std::function<bool()> &ready) {
 const auto limit=Clock::now()+std::chrono::seconds(5);
 while(!ready()) { if(Clock::now()>limit) throw std::runtime_error("test progress deadline"); Sleep(1); }
}
std::uint32_t Pixel(unsigned id,int eye,unsigned x,unsigned y) {
 return 0xff000000u|((id&1)?0x00400000u:0x00b00000u)|((id&31)<<16)|((eye?0x80u:0x10u)<<8)|((x*3+y*7+id)&255);
}
void ResizeDestination(unsigned w,unsigned h) {
 if(destW==w && destH==h) return;
 if(!destination[0]) glGenTextures(2,destination);
 for(auto texture:destination) { glBindTexture(GL_TEXTURE_2D,texture); glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA8,w,h,0,GL_BGRA,GL_UNSIGNED_BYTE,nullptr); }
 destW=w; destH=h;
}
void OfflineFrame() {
 const auto info=s_pair.peek();
 if(!info.serial || info.serial<=checkedSerial) { Sleep(1); return; }
 ResizeDestination(info.w,info.h);
 std::uint64_t copied=0;
 const bool ok=CopyPair(destination,info.w,info.h,copied,[&](int eye) {
   if(eye==0) {
     ++consumerToken; consuming=true; startedSerial=info.serial;
     Sleep(25); // producer attempts reuse/resize while consumer still owns the slot
   }
 });
 if(!ok) return; // producer resized before ownership was acquired
 std::vector<std::uint32_t> pixels(info.w*info.h);
 for(int eye=0;eye<2;++eye) {
   glBindTexture(GL_TEXTURE_2D,destination[eye]); glGetTexImage(GL_TEXTURE_2D,0,GL_BGRA,GL_UNSIGNED_BYTE,pixels.data());
   for(unsigned y=0;y<info.h;++y) for(unsigned x=0;x<info.w;++x) assert(pixels[y*info.w+x]==Pixel(unsigned(copied),eye,x,y));
 }
 assert(glGetError()==GL_NO_ERROR); ++checkedPairs; checkedSerial=copied;
}
void OfflineCleanup() {
 assert(wglGetCurrentContext()==s_xrContext); ++cleanupCalls;
 glDeleteTextures(2,destination); destination[0]=destination[1]=0; destW=destH=0;
}
void PrepareSource(GLuint textures[2],GLuint fbos[2],unsigned id,unsigned w,unsigned h) {
 std::vector<std::uint32_t> pixels(w*h);
 for(int eye=0;eye<2;++eye) {
   for(unsigned y=0;y<h;++y) for(unsigned x=0;x<w;++x) pixels[y*w+x]=Pixel(id,eye,x,y);
   glBindTexture(GL_TEXTURE_2D,textures[eye]); glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA8,w,h,0,GL_BGRA,GL_UNSIGNED_BYTE,pixels.data());
   glBindFramebuffer(GL_FRAMEBUFFER,fbos[eye]); glFramebufferTexture2D(GL_FRAMEBUFFER,GL_COLOR_ATTACHMENT0,GL_TEXTURE_2D,textures[eye],0);
 }
 glBindFramebuffer(GL_FRAMEBUFFER,0);
}

unsigned acquires=0,releases=0,waitCalls=0; bool imageTimeout=true;
XrResult XRAPI_CALL Acquire(XrSwapchain,const XrSwapchainImageAcquireInfo*,uint32_t *index) { ++acquires; *index=0; return XR_SUCCESS; }
XrResult XRAPI_CALL Wait(XrSwapchain,const XrSwapchainImageWaitInfo *info) {
 assert(info->timeout==10000000); ++waitCalls;
 if(imageTimeout) { Sleep(10); return XR_TIMEOUT_EXPIRED; } return XR_SUCCESS;
}
XrResult XRAPI_CALL Release(XrSwapchain,const XrSwapchainImageReleaseInfo*) { ++releases; return XR_SUCCESS; }
unsigned hapticCalls=0,suggestedProfiles=0; std::map<std::string,XrPath> paths;
XrResult XRAPI_CALL StringPath(XrInstance,const char *text,XrPath *out) { auto result=paths.emplace(text,paths.size()+1); *out=result.first->second; return XR_SUCCESS; }
XrResult XRAPI_CALL ActionSet(XrInstance,const XrActionSetCreateInfo*,XrActionSet *out) { *out=reinterpret_cast<XrActionSet>(1); return XR_SUCCESS; }
XrResult XRAPI_CALL Action(XrActionSet,const XrActionCreateInfo *info,XrAction *out) {
 static std::uintptr_t next=10; *out=reinterpret_cast<XrAction>(++next);
 if(std::string(info->actionName)=="shot_pulse") assert(info->actionType==XR_ACTION_TYPE_VIBRATION_OUTPUT);
 return XR_SUCCESS;
}
XrResult XRAPI_CALL Suggest(XrInstance,const XrInteractionProfileSuggestedBinding *info) {
 assert(info->countSuggestedBindings==5);
 assert(info->suggestedBindings[4].action==s_haptic);
 assert(info->suggestedBindings[4].binding==paths.at("/user/hand/right/output/haptic")); ++suggestedProfiles; return XR_SUCCESS;
}
XrResult XRAPI_CALL Attach(XrSession,const XrSessionActionSetsAttachInfo*) { return XR_SUCCESS; }
XrResult XRAPI_CALL ActionSpace(XrSession,const XrActionSpaceCreateInfo*,XrSpace *out) { *out=reinterpret_cast<XrSpace>(2); return XR_SUCCESS; }
XrResult XRAPI_CALL Haptic(XrSession,const XrHapticActionInfo *info,const XrHapticBaseHeader *base) {
 assert(info->action==s_haptic && info->subactionPath==s_right);
 auto *pulse=reinterpret_cast<const XrHapticVibration*>(base);
 assert(pulse->type==XR_TYPE_HAPTIC_VIBRATION && pulse->duration==30000000 && pulse->amplitude==1.0f && pulse->frequency==XR_FREQUENCY_UNSPECIFIED);
 ++hapticCalls; return XR_SUCCESS;
}
}

int main()
{
 SetPriorityClass(GetCurrentProcess(),BELOW_NORMAL_PRIORITY_CLASS);
 auto *owner=Host_CreateWindow("Hidden presenter test",0,0,16,16,Host_WINDOW_HIDDEN);
 assert(owner && Host_GL_CreateContext(owner) && Host_LoadGL());
 Host_TestAllowCurrent=AllowContext;
 GLuint textures[2]{},fbos[2]{}; glGenTextures(2,textures); glGenFramebuffers(2,fbos);
 s_test.offline=true; s_test.frame=OfflineFrame; s_test.cleanup=OfflineCleanup; ACVR_TestConsumerDone=ConsumerDone;
 for(unsigned cycle=0;cycle<2;++cycle) {
   startedSerial=checkedSerial=consumerToken=gpuDoneToken=0; consuming=false;
   assert(ACVR_XR_Start(owner)); assert(s_pair.peek().serial==0);
   for(unsigned id=1;id<=24;++id) {
     const unsigned w=id==24?48:48+((id-1)%3)*16, h=id==24?32:32+((id-1)%2)*8;
     PrepareSource(textures,fbos,id,w,h);
     const bool blocked=consuming; const auto token=consumerToken.load();
     ACVR_XR_Publish(fbos[0],fbos[1],w,h);
     if(blocked) { assert(gpuDoneToken>=token); ++blockedReuses; }
     Deadline([&] {return startedSerial>=id;});
   }
   Deadline([] {return checkedSerial==24;});
   assert(ACVR_XR_Stop()); assert(s_pair.peek().serial==0 && s_pair.peek().w==0);
   assert(wglGetCurrentContext()==owner->context);
 }
 assert(checkedPairs==48 && blockedReuses>0);
 std::printf("PASS GPU: %u exact stereo pairs, alternating IDs, repeated resizes; %u reuses waited for consumer GPU completion; two same-size restart cycles\n",checkedPairs,blockedReuses);

 // A foreign context must never receive cleanup callbacks after activation fails.
 auto *foreign=Host_CreateWindow("Hidden foreign context",0,0,16,16,Host_WINDOW_HIDDEN);
 assert(foreign && Host_GL_CreateContext(foreign)); GLuint sentinel=0; glGenTextures(1,&sentinel); glBindTexture(GL_TEXTURE_2D,sentinel);
 glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA8,1,1,0,GL_RGBA,GL_UNSIGNED_BYTE,nullptr);
 assert(Host_GL_MakeCurrent(owner,owner->context)==0);
 for(int failure=0;failure<2;++failure) {
   startedSerial=checkedSerial=0; assert(ACVR_XR_Start(owner));
   PrepareSource(textures,fbos,1,48,32); ACVR_XR_Publish(fbos[0],fbos[1],48,32); Deadline([] {return checkedSerial==1;});
   const auto before=cleanupCalls;
   deniedContext=failure==0?s_xrContext:s_ownerContext;
   assert(Host_GL_MakeCurrent(foreign,foreign->context)==0);
   assert(!ACVR_XR_Stop()); assert(s_pair.peek().serial==1);
   assert(cleanupCalls==before+(failure==1?1:0));
   assert(wglGetCurrentContext()==foreign->context && glIsTexture(sentinel));
   deniedContext=nullptr; assert(ACVR_XR_Stop()); assert(s_pair.peek().serial==0);
   assert(wglGetCurrentContext()==foreign->context && glIsTexture(sentinel));
   assert(Host_GL_MakeCurrent(owner,owner->context)==0);
 }
 // Fail the worker's initial activation, then recover without stale objects.
 denyForeign=true; const auto before=cleanupCalls; assert(ACVR_XR_Start(owner));
 assert(!ACVR_XR_Stop()); assert(cleanupCalls==before);
 denyForeign=false; assert(ACVR_XR_Stop()); assert(!s_pair.peek().serial);
 std::puts("PASS contexts: failed XR/producer activation skipped destructive callbacks, retry recovered, foreign texture intact, partial startup unwound");

 // Exercise the real CopyEyes acquisition/retry path with delayed runtime calls.
 ACVR_TestConsumerDone=nullptr; PrepareSource(textures,fbos,1,48,32); const GLuint sources[]={fbos[0],fbos[1]}; s_pair.publish(sources,48,32);
 glGenFramebuffers(1,&s_readFbo); glGenFramebuffers(1,&s_drawFbo); ResizeDestination(48,32);
 for(int eye=0;eye<2;++eye) { s_chain[eye]=reinterpret_cast<XrSwapchain>(std::uintptr_t(eye+1)); s_images[eye].push_back({XR_TYPE_SWAPCHAIN_IMAGE_OPENGL_KHR,nullptr,destination[eye]}); }
 s_chainW=48; s_chainH=32; s_stop=false;
 p_xrAcquireSwapchainImage=Acquire; p_xrWaitSwapchainImage=Wait; p_xrReleaseSwapchainImage=Release;
 std::uint64_t shown=0;
 auto start=Clock::now(); assert(!CopyEyes(shown));
 assert(Clock::now()-start<std::chrono::seconds(1) && acquires==1 && releases==0 && shown==0);
 s_pair.publish(sources,48,32); // producer can still progress while an image is pending
 std::thread stopper([] { Sleep(35); s_stop=true; }); start=Clock::now(); assert(!CopyEyes(shown)); stopper.join();
 assert(Clock::now()-start<std::chrono::milliseconds(200) && releases==0);
 s_stop=false; imageTimeout=false; assert(CopyEyes(shown)); assert(acquires==2 && releases==2 && shown==2);
 for(int eye=0;eye<2;++eye) { s_chain[eye]=XR_NULL_HANDLE; s_images[eye].clear(); }
 s_chainW=s_chainH=0; glDeleteFramebuffers(1,&s_readFbo); glDeleteFramebuffers(1,&s_drawFbo); s_readFbo=s_drawFbo=0;
 glDeleteTextures(2,destination); destination[0]=destination[1]=0; destW=destH=0; s_pair.reset();
 std::printf("PASS waits: %u finite image waits; deadline and stop observed; no timeout release/reacquire; resumed complete pair\n",waitCalls);

 p_xrStringToPath=StringPath; p_xrCreateActionSet=ActionSet; p_xrCreateAction=Action; p_xrSuggestInteractionProfileBindings=Suggest;
 p_xrAttachSessionActionSets=Attach; p_xrCreateActionSpace=ActionSpace;
 s_session=reinterpret_cast<XrSession>(1); assert(InitActions()); assert(suggestedProfiles==2);
 p_xrApplyHapticFeedback=Haptic;
 s_shotPulses=2; DispatchHaptics(true); DispatchHaptics(true); assert(hapticCalls==2);
 s_shotPulses=1; DispatchHaptics(false); DispatchHaptics(true); assert(hapticCalls==2);
 ShotFeedback feedback; s_started=true; s_stop=false;
 feedback.observe(10,100,100,true,ACVR_XR_QueueShotPulse); // attach baseline: no historical pulses
 feedback.observe(11,110,110,true,ACVR_XR_QueueShotPulse); DispatchHaptics(true); assert(hapticCalls==3);
 feedback.observe(11,110,120,true,ACVR_XR_QueueShotPulse); DispatchHaptics(true); assert(hapticCalls==3);
 feedback.observe(12,120,500,true,ACVR_XR_QueueShotPulse); DispatchHaptics(true); assert(hapticCalls==3);
 feedback.observe(13,500,500,false,ACVR_XR_QueueShotPulse); DispatchHaptics(true); assert(hapticCalls==3);
 feedback.observe(14,510,510,true,ACVR_XR_QueueShotPulse); DispatchHaptics(false);
 feedback.observe(14,510,520,true,ACVR_XR_QueueShotPulse); DispatchHaptics(true); assert(hapticCalls==3);
 assert(feedback.acknowledged==4 && feedback.queued==2); s_started=false;
 s_session=XR_NULL_HANDLE; s_actions=XR_NULL_HANDLE; s_aimSpace=XR_NULL_HANDLE; s_haptic=XR_NULL_HANDLE;
 std::puts("PASS haptics: both output bindings, game serial to right-hand 30 ms/strong API call, no duplicate/stale/attach/focus-regain replay (injected API only)");
 glDeleteFramebuffers(2,fbos); glDeleteTextures(2,textures);
 assert(Host_GL_MakeCurrent(foreign,foreign->context)==0); glDeleteTextures(1,&sentinel); Host_DestroyWindow(foreign);
 Host_DestroyWindow(owner);
 return 0;
}
