// SPDX-License-Identifier: GPL-3.0-or-later
// Included once inside the shared presenter's private implementation namespace.
bool TheatreExtents(){
 PFN_xrEnumerateViewConfigurationViews enumerate=nullptr;
 if(XR_FAILED(p_xrGetInstanceProcAddr(s_instance,"xrEnumerateViewConfigurationViews",reinterpret_cast<PFN_xrVoidFunction*>(&enumerate))) || !enumerate)return false;
 XrViewConfigurationView views[2]={{XR_TYPE_VIEW_CONFIGURATION_VIEW},{XR_TYPE_VIEW_CONFIGURATION_VIEW}};
 uint32_t count=0;
 if(!Check(enumerate(s_instance,s_system,XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO,2,&count,views),"theatre view extents") || count!=2)return false;
 for(unsigned eye=0;eye<2;++eye){
  s_theatreW[eye]=views[eye].recommendedImageRectWidth;s_theatreH[eye]=views[eye].recommendedImageRectHeight;
  if(!s_theatreW[eye] || !s_theatreH[eye] || s_theatreW[eye]>16384 || s_theatreH[eye]>16384)return false;
 }
 return true;
}
TheatreFrame TheatreState(XrTime time,bool focused){
 TheatreFrame frame;frame.views.frame_time=time;frame.focused=focused;
 for(unsigned eye=0;eye<2;++eye){frame.width[eye]=s_theatreW[eye];frame.height[eye]=s_theatreH[eye];}
 XrViewLocateInfo locate={XR_TYPE_VIEW_LOCATE_INFO};locate.viewConfigurationType=XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO;locate.displayTime=time;locate.space=s_space;
 XrViewState state={XR_TYPE_VIEW_STATE};uint32_t count=0;
 XrView views[2]={{XR_TYPE_VIEW},{XR_TYPE_VIEW}};
 const auto valid=XR_VIEW_STATE_POSITION_VALID_BIT|XR_VIEW_STATE_ORIENTATION_VALID_BIT;
 if(XR_SUCCEEDED(p_xrLocateViews(s_session,&locate,&state,2,&count,views)) && count==2 && (state.viewStateFlags&valid)==valid){
  frame.views.valid=true;
  for(unsigned eye=0;eye<2;++eye){const auto& v=views[eye];auto& e=frame.views.eyes[eye];
   e.position[0]=v.pose.position.x;e.position[1]=v.pose.position.y;e.position[2]=v.pose.position.z;
   e.orientation[0]=v.pose.orientation.x;e.orientation[1]=v.pose.orientation.y;e.orientation[2]=v.pose.orientation.z;e.orientation[3]=v.pose.orientation.w;
   e.fov[0]=v.fov.angleLeft;e.fov[1]=v.fov.angleRight;e.fov[2]=v.fov.angleUp;e.fov[3]=v.fov.angleDown;
  }
  frame.views.valid=valid_projection_packet(frame.views);
 }
 const Uint8* keys=Host_GetKeyboardState(nullptr);frame.recenter=keys && keys[Host_SCANCODE_HOME];
 // Focus gates input only; VISIBLE sessions can still draw and begin/end frames.
 bool clicksRead=false;
 if(focused){
  XrActiveActionSet set{s_actions,XR_NULL_PATH};XrActionsSyncInfo sync={XR_TYPE_ACTIONS_SYNC_INFO};sync.countActiveActionSets=1;sync.activeActionSets=&set;
  if(p_xrSyncActions(s_session,&sync)==XR_SUCCESS){
   auto pose=[&](XrAction action,XrSpace space,TrackedPose& out,XrPath hand=XR_NULL_PATH){
    XrActionStateGetInfo get={XR_TYPE_ACTION_STATE_GET_INFO};get.action=action;get.subactionPath=hand==XR_NULL_PATH?s_right:hand;
    XrActionStatePose active={XR_TYPE_ACTION_STATE_POSE};XrSpaceLocation location={XR_TYPE_SPACE_LOCATION};
    const auto need=XR_SPACE_LOCATION_POSITION_VALID_BIT|XR_SPACE_LOCATION_ORIENTATION_VALID_BIT|XR_SPACE_LOCATION_POSITION_TRACKED_BIT|XR_SPACE_LOCATION_ORIENTATION_TRACKED_BIT;
    if(XR_SUCCEEDED(p_xrGetActionStatePose(s_session,&get,&active)) && active.isActive && XR_SUCCEEDED(p_xrLocateSpace(space,s_space,time,&location)) && (location.locationFlags&need)==need){
     out.valid=true;out.position[0]=location.pose.position.x;out.position[1]=location.pose.position.y;out.position[2]=location.pose.position.z;
     out.orientation[0]=location.pose.orientation.x;out.orientation[1]=location.pose.orientation.y;out.orientation[2]=location.pose.orientation.z;out.orientation[3]=location.pose.orientation.w;
    }
   };
   auto scalar=[&](XrAction action,XrPath hand=XR_NULL_PATH){XrActionStateGetInfo get={XR_TYPE_ACTION_STATE_GET_INFO};get.action=action;get.subactionPath=hand==XR_NULL_PATH?s_right:hand;XrActionStateFloat value={XR_TYPE_ACTION_STATE_FLOAT};return XR_SUCCEEDED(p_xrGetActionStateFloat(s_session,&get,&value)) && value.isActive?value.currentState:0.f;};
   auto button=[&](XrAction action,XrPath hand){XrActionStateGetInfo get={XR_TYPE_ACTION_STATE_GET_INFO};get.action=action;get.subactionPath=hand;XrActionStateBoolean value={XR_TYPE_ACTION_STATE_BOOLEAN};return XR_SUCCEEDED(p_xrGetActionStateBoolean(s_session,&get,&value)) && value.isActive && value.currentState;};
   pose(s_aim,s_aimSpace,frame.aim);pose(s_grip,s_gripSpace,frame.grip);
   frame.trigger=frame.aim.valid?scalar(s_trigger):0;frame.squeeze=scalar(s_secondary);frame.button2=frame.squeeze>=.55f;
   if(s_leftGrip){pose(s_leftGrip,s_leftGripSpace,frame.leftGrip,s_left);frame.leftTrigger=scalar(s_leftTrigger,s_left);}
   auto stick=[&](XrAction action,XrPath hand,float out[2]){XrActionStateGetInfo get={XR_TYPE_ACTION_STATE_GET_INFO};get.action=action;get.subactionPath=hand;XrActionStateVector2f value={XR_TYPE_ACTION_STATE_VECTOR2F};if(action && p_xrGetActionStateVector2f && XR_SUCCEEDED(p_xrGetActionStateVector2f(s_session,&get,&value)) && value.isActive){out[0]=value.currentState.x;out[1]=value.currentState.y;}};
   stick(s_leftStick,s_left,frame.leftStick);stick(s_rightStick,s_right,frame.rightStick);
   // U1-U4: raw stick clicks in, resolved Start/Coin, recentre pulse and quit hold out.
   clicksRead=true;
   const auto stickState=s_chord.step(GetTickCount64(),button(s_leftClick,s_left),button(s_rightClick,s_right));
   frame.start=stickState.start;frame.coin=stickState.select;frame.quitHeld=stickState.chord;
   frame.recenter=frame.recenter||stickState.recenter;frame.chordMs=stickState.chord_ms;frame.quitProgress=stickState.quit_visible&&!stickState.quit_cancelled?stickState.quit_progress:0.f;
  }
 }
 if(!clicksRead)s_chord.step(GetTickCount64(),false,false); // focus loss or a failed sync releases every click
 DispatchHaptics(frame.focused);
 return frame;
}
bool EnsureTheatreChains(){
 if(s_chain[0])return true;
 for(unsigned eye=0;eye<2;++eye){
  XrSwapchainCreateInfo info={XR_TYPE_SWAPCHAIN_CREATE_INFO};info.usageFlags=XR_SWAPCHAIN_USAGE_COLOR_ATTACHMENT_BIT|XR_SWAPCHAIN_USAGE_SAMPLED_BIT;
  info.format=s_format;info.sampleCount=1;info.width=s_theatreW[eye];info.height=s_theatreH[eye];info.faceCount=1;info.arraySize=1;info.mipCount=1;
  if(!Check(p_xrCreateSwapchain(s_session,&info,&s_chain[eye]),"theatre swapchain")){DestroyChains();return false;}
  uint32_t count=0;
  if(!Check(p_xrEnumerateSwapchainImages(s_chain[eye],0,&count,nullptr),"theatre images") || !count){DestroyChains();return false;}
  s_images[eye].assign(count,{XR_TYPE_SWAPCHAIN_IMAGE_OPENGL_KHR});
  if(!Check(p_xrEnumerateSwapchainImages(s_chain[eye],count,&count,reinterpret_cast<XrSwapchainImageBaseHeader*>(s_images[eye].data())),"theatre images")){DestroyChains();return false;}
 }
 return true;
}
bool RenderTheatre(const TheatreFrame& frame,bool shouldRender){
 bool ready=false;
 try{ready=s_theatre->update(frame);}catch(const std::exception& e){Log("theatre update: %s",e.what());s_stop=true;return false;}
 if(!ready || !shouldRender || !frame.views.valid || s_stop)return false;
 if(!EnsureTheatreChains() || !AcquireImages())return false;
 bool ok=true;
 try{
  for(unsigned eye=0;eye<2;++eye)s_theatre->draw_eye(eye,s_images[eye][s_imageIndex[eye]].image,frame,s_format!=GL_RGBA8);
  glFinish(); // complete client writes before relinquishing runtime images
  ok=DrainGL("theatre render")==0;
 }catch(const std::exception& e){Log("theatre draw: %s",e.what());ok=false;s_stop=true;glFinish();}
 for(unsigned eye=0;eye<2;++eye){
  XrSwapchainImageReleaseInfo release={XR_TYPE_SWAPCHAIN_IMAGE_RELEASE_INFO};
  ok=Check(p_xrReleaseSwapchainImage(s_chain[eye],&release),"theatre release") && ok;s_acquired[eye]=s_imageReady[eye]=false;
 }
 if(!ok)s_stop=true;
 return ok;
}
