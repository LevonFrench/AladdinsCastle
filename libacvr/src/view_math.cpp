// SPDX-License-Identifier: MIT
#include "view_math.hpp"
#include "pose_math.hpp"
#include <algorithm>
#include <array>
#include <cmath>
#include <limits>

namespace acvr {
namespace {
using V3=std::array<double,3>;
using Q4=std::array<double,4>;
bool pose_valid(const acvr_pose &p) {
    if(p.size<sizeof(p)||p.version!=ACVR_STRUCT_VERSION) return false;
    double length=0;
    for(float n:p.position_m) if(!std::isfinite(n)) return false;
    for(float n:p.orientation_xyzw) {if(!std::isfinite(n)) return false;length+=double(n)*n;}
    return std::abs(length-1)<.001;
}
Q4 quaternion(const acvr_pose &p) {
    return normalized_quaternion(p.orientation_xyzw);
}
Q4 multiply(Q4 a,Q4 b) {
    return {a[3]*b[0]+a[0]*b[3]+a[1]*b[2]-a[2]*b[1],
            a[3]*b[1]-a[0]*b[2]+a[1]*b[3]+a[2]*b[0],
            a[3]*b[2]+a[0]*b[1]-a[1]*b[0]+a[2]*b[3],
            a[3]*b[3]-a[0]*b[0]-a[1]*b[1]-a[2]*b[2]};
}
V3 rotate(Q4 q,V3 v) {
    const V3 t{2*(q[1]*v[2]-q[2]*v[1]),2*(q[2]*v[0]-q[0]*v[2]),2*(q[0]*v[1]-q[1]*v[0])};
    return {v[0]+q[3]*t[0]+q[1]*t[2]-q[2]*t[1],
            v[1]+q[3]*t[1]+q[2]*t[0]-q[0]*t[2],
            v[2]+q[3]*t[2]+q[0]*t[1]-q[1]*t[0]};
}
bool fits(double n) {return std::isfinite(n)&&std::abs(n)<=std::numeric_limits<float>::max();}
}
acvr_result compose_eye(const acvr_pose &anchor,const acvr_pose &eye_pose,float scale,
                        EyeFov fov,float near_m,float far_m,uint32_t api,acvr_eye &eye) noexcept {
    if(eye.size<sizeof(eye)||eye.version!=ACVR_STRUCT_VERSION) return ACVR_BAD_VERSION;
    if(api!=ACVR_GRAPHICS_GL&&api!=ACVR_GRAPHICS_VULKAN) return ACVR_UNSUPPORTED;
    if(!pose_valid(anchor)||!pose_valid(eye_pose)||!std::isfinite(scale)||scale<=0||
       !std::isfinite(near_m)||!std::isfinite(far_m)||near_m<=0||far_m<=near_m) return ACVR_BAD_ARGUMENT;
    constexpr double half_pi=1.57079632679489661923;
    for(float angle:{fov.left,fov.right,fov.down,fov.up})
        if(!std::isfinite(angle)||std::abs(double(angle))>=half_pi) return ACVR_BAD_ARGUMENT;
    if(fov.left>=fov.right||fov.down>=fov.up) return ACVR_BAD_ARGUMENT;
    const auto a=quaternion(anchor),e=quaternion(eye_pose),q=multiply(a,e);
    const Q4 inverse{-q[0],-q[1],-q[2],q[3]};
    auto position=rotate(a,{eye_pose.position_m[0],eye_pose.position_m[1],eye_pose.position_m[2]});
    for(unsigned i=0;i<3;++i) position[i]=(position[i]+anchor.position_m[i])*scale;
    const auto translation=rotate(inverse,{-position[0],-position[1],-position[2]});
    std::array<double,16> view{},projection{};view[15]=1;
    for(unsigned column=0;column<3;++column) {
        V3 basis{};basis[column]=1;const auto axis=rotate(inverse,basis);
        for(unsigned row=0;row<3;++row) view[column*4+row]=axis[row];
        view[12+column]=translation[column];
    }
    const double left=std::tan(fov.left),right=std::tan(fov.right),down=std::tan(fov.down),up=std::tan(fov.up);
    const double n=double(near_m)*scale,f=double(far_m)*scale;
    const double y=api==ACVR_GRAPHICS_VULKAN?-1:1;
    projection[0]=2/(right-left);projection[5]=y*2/(up-down);
    projection[8]=(right+left)/(right-left);projection[9]=y*(up+down)/(up-down);
    projection[10]=api==ACVR_GRAPHICS_GL?-(f+n)/(f-n):-f/(f-n);
    projection[11]=-1;projection[14]=api==ACVR_GRAPHICS_GL?-2*f*n/(f-n):-f*n/(f-n);
    for(unsigned i=0;i<16;++i) if(!fits(view[i])||!fits(projection[i])) return ACVR_BAD_ARGUMENT;
    // Reject unusable underflow too; a finite zero near/depth term is not a
    // valid perspective frustum even when extremely small scales were supplied.
    if(float(projection[0])==0||float(projection[5])==0||float(projection[14])==0) return ACVR_BAD_ARGUMENT;
    // Finite coefficients alone do not prove that a float matrix represents the
    // requested frustum. Adjacent float near/far planes can otherwise map the
    // near plane to -2 instead of -1 through coefficient rounding.
    std::array<double,16> packed{};
    for(unsigned i=0;i<16;++i) packed[i]=float(projection[i]);
    const auto close=[](double actual,double expected){return std::isfinite(actual)&&std::abs(actual-expected)<=.0001;};
    if(!close(-packed[10]+packed[14]/n,api==ACVR_GRAPHICS_GL?-1:0)||
       !close(-packed[10]+packed[14]/f,1)||
       !close(packed[0]*left-packed[8],-1)||!close(packed[0]*right-packed[8],1)||
       !close(packed[5]*down-packed[9],-y)||!close(packed[5]*up-packed[9],y)) return ACVR_BAD_ARGUMENT;
    for(unsigned i=0;i<16;++i) {eye.view_from_scene[i]=float(view[i]);eye.projection_from_view[i]=float(projection[i]);}
    return ACVR_OK;
}
acvr_result recenter_anchor(const acvr_pose &head,float height,acvr_pose &out) noexcept {
    if(out.size<sizeof(out)||out.version!=ACVR_STRUCT_VERSION) return ACVR_BAD_VERSION;
    if(!pose_valid(head)||!std::isfinite(height)||height<0) return ACVR_BAD_ARGUMENT;
    const auto forward=rotate(quaternion(head),{0,0,-1});
    if(std::hypot(forward[0],forward[2])<1e-6) return ACVR_BAD_ARGUMENT;
    const double yaw=std::atan2(-forward[0],-forward[2]);
    const Q4 rotation{0,-std::sin(yaw/2),0,std::cos(yaw/2)};
    auto translation=rotate(rotation,{-head.position_m[0],-head.position_m[1],-head.position_m[2]});
    translation[1]+=height;
    for(double n:translation) if(!fits(n)) return ACVR_BAD_ARGUMENT;
    for(unsigned i=0;i<3;++i) out.position_m[i]=float(translation[i]);
    for(unsigned i=0;i<4;++i) out.orientation_xyzw[i]=float(rotation[i]);
    return ACVR_OK;
}
}
