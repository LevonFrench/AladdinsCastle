// SPDX-License-Identifier: MIT
#pragma once
#include "acvr.h"
#include <array>
#include <cmath>

namespace acvr {
// Diagnostic authoring poses only. Freeze these once from the first valid head;
// they are not controller measurements and must be identified in the receipt.
inline bool fixed_capture_poses(const acvr_pose &head,std::array<acvr_pose,2> &out) noexcept {
    if(head.size<sizeof(head)||head.version!=ACVR_STRUCT_VERSION)return false;
    double norm=0;
    for(float v:head.position_m)if(!std::isfinite(v))return false;
    for(float v:head.orientation_xyzw){if(!std::isfinite(v))return false;norm+=v*v;}
    if(std::abs(norm-1.0)>.002)return false;
    const double scale=1.0/std::sqrt(norm);
    const double x=head.orientation_xyzw[0]*scale,y=head.orientation_xyzw[1]*scale;
    const double z=head.orientation_xyzw[2]*scale,w=head.orientation_xyzw[3]*scale;
    auto candidate=out;
    for(unsigned hand=0;hand<2;++hand){
        const double px=hand==0?.18:-.18,py=-.18,pz=-.55;
        const double tx=2*(y*pz-z*py),ty=2*(z*px-x*pz),tz=2*(x*py-y*px);
        auto &pose=candidate[hand];pose={};ACVR_INIT(&pose);
        pose.position_m[0]=float(head.position_m[0]+px+w*tx+y*tz-z*ty);
        pose.position_m[1]=float(head.position_m[1]+py+w*ty+z*tx-x*tz);
        pose.position_m[2]=float(head.position_m[2]+pz+w*tz+x*ty-y*tx);
        pose.orientation_xyzw[0]=float(x);pose.orientation_xyzw[1]=float(y);
        pose.orientation_xyzw[2]=float(z);pose.orientation_xyzw[3]=float(w);
    }
    out=candidate;return true;
}
}
