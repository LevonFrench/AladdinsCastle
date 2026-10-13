// SPDX-License-Identifier: MIT
#include "two_hand.hpp"
#include "pose_math.hpp"
#include <algorithm>
#include <cmath>

namespace acvr {
namespace {
using V=std::array<double,3>;using Q=std::array<double,4>;
bool valid(const acvr_pose &p) {
    if(p.size!=sizeof(p)||p.version!=ACVR_STRUCT_VERSION)return false;
    for(float v:p.position_m)if(!std::isfinite(v))return false;
    double n=0;for(float v:p.orientation_xyzw){if(!std::isfinite(v))return false;n+=double(v)*v;}
    return std::abs(n-1)<.001;
}
Q multiply(Q a,Q b){return {a[3]*b[0]+a[0]*b[3]+a[1]*b[2]-a[2]*b[1],
    a[3]*b[1]-a[0]*b[2]+a[1]*b[3]+a[2]*b[0],
    a[3]*b[2]+a[0]*b[1]-a[1]*b[0]+a[2]*b[3],
    a[3]*b[3]-a[0]*b[0]-a[1]*b[1]-a[2]*b[2]};}
V rotate(Q q,V v){
    const V t{2*(q[1]*v[2]-q[2]*v[1]),2*(q[2]*v[0]-q[0]*v[2]),2*(q[0]*v[1]-q[1]*v[0])};
    return {v[0]+q[3]*t[0]+q[1]*t[2]-q[2]*t[1],v[1]+q[3]*t[1]+q[2]*t[0]-q[0]*t[2],
        v[2]+q[3]*t[2]+q[0]*t[1]-q[1]*t[0]};
}
V cross(V a,V b){return {a[1]*b[2]-a[2]*b[1],a[2]*b[0]-a[0]*b[2],a[0]*b[1]-a[1]*b[0]};}
double dot(V a,V b){return a[0]*b[0]+a[1]*b[1]+a[2]*b[2];}
}
bool solve_two_hand_grip(const acvr_pose &main,const acvr_pose &off,const float angle[4],
                         std::array<float,3> second,float radius,acvr_pose &out) noexcept {
    if(!valid(main)||!valid(off)||!angle||!std::isfinite(radius)||radius<=0)return false;
    double n=0;for(unsigned i=0;i<4;++i){if(!std::isfinite(angle[i]))return false;n+=double(angle[i])*angle[i];}
    if(std::abs(n-1)>.001)return false;
    for(float v:second)if(!std::isfinite(v))return false;
    const Q base=normalized_quaternion(main.orientation_xyzw),calibration=normalized_quaternion(angle);
    const V local{second[0],second[1],second[2]};
    auto expected=rotate(base,rotate(calibration,local));
    const V actual{double(off.position_m[0])-main.position_m[0],
                   double(off.position_m[1])-main.position_m[1],
                   double(off.position_m[2])-main.position_m[2]};
    const V error{expected[0]-actual[0],expected[1]-actual[1],expected[2]-actual[2]};
    if(dot(error,error)>double(radius)*radius)return false;
    const double a=std::sqrt(dot(expected,expected)),b=std::sqrt(dot(actual,actual));
    if(a<1e-4||b<1e-4)return false;
    for(auto &v:expected)v/=a;
    const V target{actual[0]/b,actual[1]/b,actual[2]/b};
    const double d=std::clamp(dot(expected,target),-1.,1.);
    Q delta{};
    if(d<-.99999) {
        V axis=cross(expected,{0,1,0});
        if(dot(axis,axis)<1e-8)axis=cross(expected,{1,0,0});
        const double length=std::sqrt(dot(axis,axis));delta={axis[0]/length,axis[1]/length,axis[2]/length,0};
    } else {
        const auto c=cross(expected,target);delta={c[0],c[1],c[2],1+d};
        const double length=std::sqrt(dot(c,c)+(1+d)*(1+d));for(auto &v:delta)v/=length;
    }
    const Q result=multiply(delta,base);
    out=main;for(unsigned i=0;i<4;++i)out.orientation_xyzw[i]=static_cast<float>(result[i]);
    return true;
}
}
