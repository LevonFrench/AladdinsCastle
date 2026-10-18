// SPDX-License-Identifier: GPL-3.0-or-later
// Injected API/policy tests. Never starts a presenter, loader or runtime.
#define ACVR_VIEWER_TEST
#include "Presenter.cpp"
#include "CleanQuit.h"
#include <cassert>
#include <string>
using namespace acvr_publish;
namespace {
bool missing=false,tracked=true;
void XRAPI_CALL Dummy(){}
XrResult XRAPI_CALL Resolve(XrInstance,const char* name,PFN_xrVoidFunction* out){
 if(missing && std::string(name)=="xrEndFrame"){*out=nullptr;return XR_ERROR_FUNCTION_UNSUPPORTED;}
 *out=Dummy;return XR_SUCCESS;
}
XrResult XRAPI_CALL Locate(XrSpace,XrSpace,XrTime,XrSpaceLocation* out){
 out->locationFlags=tracked?XR_SPACE_LOCATION_POSITION_VALID_BIT|XR_SPACE_LOCATION_ORIENTATION_VALID_BIT:0;
 out->pose={{0,.707106781f,0,.707106781f},{1,2,3}};return XR_SUCCESS;
}
XrResult XRAPI_CALL Views(XrSession,const XrViewLocateInfo*,XrViewState* state,std::uint32_t,std::uint32_t* count,XrView* out){
 *count=2;state->viewStateFlags=tracked?XR_VIEW_STATE_POSITION_VALID_BIT|XR_VIEW_STATE_ORIENTATION_VALID_BIT:0;
 for(unsigned eye=0;eye<2;eye++){out[eye].pose={{0,0,0,1},{eye?.032f:-.032f,1,2}};out[eye].fov={-.7f,.8f,.9f,-.6f};}
 return XR_SUCCESS;
}
}
int main(){
 p_xrGetInstanceProcAddr=Resolve;
 assert(ResolveRuntimeFunctions());assert(p_xrEndFrame && p_xrWaitSwapchainImage);
 missing=true;assert(!ResolveRuntimeFunctions());
#define X(name) assert(p_##name==nullptr);
 ACVR_XR_FUNCS(X)
#undef X
 p_xrGetInstanceProcAddr=nullptr;
 std::puts("PASS shared loader: complete injected table, missing entry fails closed and clears all functions; no loader opened");

 unsigned begins=0,ends=0,clears=0;SessionLoop loop;
 auto begin=[&]{++begins;return true;};auto end=[&]{++ends;};auto clear=[&]{++clears;};
 loop.transition(XR_SESSION_STATE_READY,begin,end,clear);loop.transition(XR_SESSION_STATE_READY,begin,end,clear);
 assert(begins==1 && loop.running());
 loop.transition(XR_SESSION_STATE_VISIBLE,begin,end,clear);assert(loop.running()&&!loop.focused());
 loop.transition(XR_SESSION_STATE_FOCUSED,begin,end,clear);assert(loop.focused());
 loop.transition(XR_SESSION_STATE_STOPPING,begin,end,clear);loop.transition(XR_SESSION_STATE_STOPPING,begin,end,clear);
 assert(!loop.running() && ends==1 && clears==5);
 loop.transition(XR_SESSION_STATE_LOSS_PENDING,begin,end,clear);assert(loop.exiting());
 SessionLoop failed;failed.transition(XR_SESSION_STATE_READY,[]{return false;},end,clear);assert(!failed.running());
 std::puts("PASS shared session: begin/end ownership, visible rendering independent of input focus, loss and failed begin");

 ControllerInput held;held.active=held.trigger=held.quitHeld=true;
 InputLease<ControllerInput> lease;lease.publish(1000,held);
 assert(lease.get(1249).trigger && !lease.get(1250).trigger && !lease.get(999).trigger);
 lease.clear();assert(!lease.get(1000).active);
 const auto now=GetTickCount64();s_input.publish(now,held);assert(controller_input().active);
 s_input.publish(now-250,held);assert(!controller_input().active);s_input.clear();
 CleanQuit quit;quit.poll(1,true,false);quit.poll(2999,false,false);quit.poll(3000,true,false);quit.poll(5999,true,false);assert(!quit.requested());quit.poll(6000,true,false);assert(quit.requested());
 std::puts("PASS shared input: exact 250ms expiry, backward-clock neutral, clear and interrupted hold-to-quit");

 unsigned remaining=3,reported=0;
 assert(drain_errors([&]{return remaining?remaining--:0;},[&](unsigned){++reported;})==3 && reported==3);
 assert(drain_errors([]{return 1;},[](unsigned){})==64);
 std::puts("PASS shared GL drain: stale queue drained and persistent failure capped");

 XrCompositionLayerQuad quads[2];const auto space=reinterpret_cast<XrSpace>(1);
 std::array<EyeImage,2> images={EyeImage{reinterpret_cast<XrSwapchain>(2),{{3,4},{800,600}},0},EyeImage{reinterpret_cast<XrSwapchain>(3),{{5,6},{900,700}},1}};
 XrPosef pose={{0,0,0,1},{.2f,.3f,-2}};screen_quads(quads,space,pose,4,3,images);
 for(unsigned eye=0;eye<2;eye++)assert(quads[eye].layerFlags==0 && quads[eye].space==space && quads[eye].pose.position.z==-2 && quads[eye].subImage.swapchain==images[eye].chain && quads[eye].subImage.imageRect.extent.width==images[eye].rectangle.extent.width && quads[eye].subImage.imageArrayIndex==eye);
 assert(quads[0].eyeVisibility==XR_EYE_VISIBILITY_LEFT && quads[1].eyeVisibility==XR_EYE_VISIBILITY_RIGHT);
 XrCompositionLayerProjectionView views[2]={{XR_TYPE_COMPOSITION_LAYER_PROJECTION_VIEW},{XR_TYPE_COMPOSITION_LAYER_PROJECTION_VIEW}};
 for(unsigned eye=0;eye<2;eye++){views[eye].pose=pose;views[eye].fov={-.6f,.7f,.8f,-.5f};views[eye].subImage={images[eye].chain,images[eye].rectangle,images[eye].array_index};}
 XrCompositionLayerProjection projection;
 assert(projection_layer(projection,space,views,2));assert(projection.views==views && projection.viewCount==2 && projection.layerFlags==0);
 assert(projection_layer(projection,space,views,2,XR_COMPOSITION_LAYER_BLEND_TEXTURE_SOURCE_ALPHA_BIT));assert(projection.layerFlags==XR_COMPOSITION_LAYER_BLEND_TEXTURE_SOURCE_ALPHA_BIT);
 assert(!projection_layer(projection,space,views,1) && projection.viewCount==0);
 views[1].subImage.imageRect.extent.width=0;assert(!projection_layer(projection,space,views,2));
 std::puts("PASS shared layers: eye-specific quads, exact rectangles, opaque/alpha projection, invalid pair rejection");
 ProjectionPacket packet;packet.valid=true;assert(valid_projection_packet(packet));
 packet.eyes[0].orientation[3]=0;assert(!valid_projection_packet(packet));packet.eyes[0].orientation[3]=1;
 p_xrLocateViews=Views;assert(!located_views(packet));UpdateViews(123);assert(located_views(packet));
 assert(packet.frame_time==123 && packet.eyes[0].position[0]==-.032f && packet.eyes[1].fov[2]==.9f);
 tracked=false;UpdateViews(124);assert(!located_views(packet));tracked=true;p_xrLocateViews=nullptr;s_wantViews=false;
 std::puts("PASS shared view packets: opt-in located views, exact time/pose/FOV, invalid tracking expires, bad quaternion rejected");

 s_distance=2;s_height=.3f;update_fov(-.8f,1.2f,-.5f,.5f);p_xrLocateSpace=Locate;
 assert(PlaceScreen(1));assert(std::abs(s_quadPose.position.x+1)<1e-5 && std::abs(s_quadPose.position.y-2.3f)<1e-5 && std::abs(s_quadPose.position.z-2.6f)<1e-5);
 tracked=false;assert(!PlaceScreen(2));p_xrLocateSpace=nullptr;
 std::puts("PASS shared recentre: asymmetric FOV centre, translated/rotated pose and invalid tracking rejection (injected API only)");
}
