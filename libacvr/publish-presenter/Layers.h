// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <array>
#include <cstdint>
#include <cmath>
#include "Presenter.h"
#include <openxr/openxr.h>
namespace acvr_publish {
inline bool valid_projection_packet(const ProjectionPacket& packet){
 if(!packet.valid)return false;
 for(const auto& eye:packet.eyes){
  for(float v:eye.position)if(!std::isfinite(v))return false;
  float norm=0;for(float v:eye.orientation){if(!std::isfinite(v))return false;norm+=v*v;}
  if(std::abs(norm-1)>0.01f)return false;
  for(float v:eye.fov)if(!std::isfinite(v) || std::abs(v)>=1.5707963f)return false;
  if(!(eye.fov[1]>eye.fov[0] && eye.fov[2]>eye.fov[3]))return false;
 }
 return true;
}
struct EyeImage {XrSwapchain chain;XrRect2Di rectangle;std::uint32_t array_index=0;};
inline void screen_quads(XrCompositionLayerQuad (&out)[2],XrSpace space,const XrPosef& pose,
 float width,float height,const std::array<EyeImage,2>& pictures){
 for(unsigned eye=0;eye<2;eye++){
  auto& q=out[eye];q={XR_TYPE_COMPOSITION_LAYER_QUAD};q.space=space;
  q.eyeVisibility=eye?XR_EYE_VISIBILITY_RIGHT:XR_EYE_VISIBILITY_LEFT;
  q.subImage={pictures[eye].chain,pictures[eye].rectangle,pictures[eye].array_index};
  q.pose=pose;q.size={width,height};
 }
}
// The caller owns and retains both views through xrEndFrame; no hidden copy of
// pointers to temporary view arrays. Located poses/FOV are copied by the caller.
inline bool projection_layer(XrCompositionLayerProjection& out,XrSpace space,
 const XrCompositionLayerProjectionView* views,unsigned count,XrCompositionLayerFlags flags=0){
 out={XR_TYPE_COMPOSITION_LAYER_PROJECTION};out.space=space;
 if(!views || count!=2)return false;
 for(unsigned eye=0;eye<2;eye++)if(views[eye].type!=XR_TYPE_COMPOSITION_LAYER_PROJECTION_VIEW ||
  !views[eye].subImage.swapchain || views[eye].subImage.imageRect.extent.width<=0 || views[eye].subImage.imageRect.extent.height<=0)return false;
 out.layerFlags=flags;out.viewCount=count;out.views=views;return true;
}
}
