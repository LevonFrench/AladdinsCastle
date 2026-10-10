// SPDX-License-Identifier: MIT
#include "view_math.hpp"
#include "runtime_host.hpp"
#include <array>
#include <cmath>
#include <cstring>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string>

namespace {
unsigned checks=0;
void check(bool b) {++checks;if(!b) throw std::runtime_error("view check "+std::to_string(checks));}
bool near(double a,double b) {return std::abs(a-b)<.0002;}
template<class T> T init() {T t{};ACVR_INIT(&t);return t;}
acvr_pose pose() {auto p=init<acvr_pose>();p.orientation_xyzw[3]=1;return p;}
using V4=std::array<double,4>;
V4 transform_point(const float *m,V4 p) {
    V4 out{};for(unsigned r=0;r<4;++r) for(unsigned c=0;c<4;++c) out[r]+=m[c*4+r]*p[c];return out;
}
void frusta() {
    const acvr::EyeFov fov{-.65f,.8f,-.55f,.7f};
    for(auto api:{ACVR_GRAPHICS_GL,ACVR_GRAPHICS_VULKAN}) for(float scale:{1.f,10.f,.01f}) {
        auto eye=init<acvr_eye>();eye.eye_index=1;eye.rect_x=42;eye.rect_width=640;
        check(acvr::compose_eye(pose(),pose(),scale,fov,.1f,100.f,api,eye)==ACVR_OK);
        check(eye.eye_index==1&&eye.rect_x==42&&eye.rect_width==640);
        const double n=.1*scale,f=100.*scale;
        auto clip=transform_point(eye.projection_from_view,{0,0,-n,1});check(near(clip[2]/clip[3],api==ACVR_GRAPHICS_GL?-1:0));
        clip=transform_point(eye.projection_from_view,{0,0,-f,1});check(near(clip[2]/clip[3],1));
        clip=transform_point(eye.projection_from_view,{std::tan(fov.left)*n,0,-n,1});check(near(clip[0]/clip[3],-1));
        clip=transform_point(eye.projection_from_view,{std::tan(fov.right)*n,0,-n,1});check(near(clip[0]/clip[3],1));
        clip=transform_point(eye.projection_from_view,{0,std::tan(fov.up)*n,-n,1});check(near(clip[1]/clip[3],api==ACVR_GRAPHICS_GL?1:-1));
        clip=transform_point(eye.projection_from_view,{0,std::tan(fov.down)*n,-n,1});check(near(clip[1]/clip[3],api==ACVR_GRAPHICS_GL?-1:1));
    }
}
void anchors_and_ipd() {
    const acvr::EyeFov fov{-.7f,.7f,-.7f,.7f};auto anchor=pose(),p=pose();auto eye=init<acvr_eye>();
    anchor.position_m[0]=10;anchor.orientation_xyzw[1]=anchor.orientation_xyzw[3]=std::sqrt(.5f);p.position_m[0]=1;
    check(acvr::compose_eye(anchor,p,10,fov,.1f,100,ACVR_GRAPHICS_GL,eye)==ACVR_OK);
    auto v=transform_point(eye.view_from_scene,{100,0,-10,1});check(near(v[0],0)&&near(v[1],0)&&near(v[2],0));
    v=transform_point(eye.view_from_scene,{90,0,-10,1});check(near(v[0],0)&&near(v[1],0)&&near(v[2],-10));
    // Same scene point acquires opposite disparity solely from physical IPD.
    double x[2]{};
    for(unsigned i=0;i<2;++i) {
        p=pose();p.position_m[0]=i?.032f:-.032f;
        check(acvr::compose_eye(pose(),p,10,fov,.1f,100,ACVR_GRAPHICS_GL,eye)==ACVR_OK);
        v=transform_point(eye.projection_from_view,transform_point(eye.view_from_scene,{0,0,-20,1}));x[i]=v[0]/v[3];
    }
    check(x[0]>0&&x[1]<0&&near(x[0],-x[1]));
    check(near(x[0],.032/(2*std::tan(.7f))));
}
void recenter() {
    auto head=pose(),anchor=pose();head.position_m[0]=2;head.position_m[1]=1.6f;head.position_m[2]=3;
    const float s=std::sqrt(.5f),pitch=.2617993878f;
    head.orientation_xyzw[0]=s*std::sin(pitch);head.orientation_xyzw[1]=s*std::cos(pitch);
    head.orientation_xyzw[2]=-s*std::sin(pitch);head.orientation_xyzw[3]=s*std::cos(pitch);
    check(acvr::recenter_anchor(head,1.2f,anchor)==ACVR_OK);
    check(near(anchor.orientation_xyzw[0],0)&&near(anchor.orientation_xyzw[2],0)&&near(anchor.orientation_xyzw[1],-s));
    auto eye=init<acvr_eye>();check(acvr::compose_eye(anchor,head,10,{-.7f,.7f,-.7f,.7f},.1f,100,ACVR_GRAPHICS_GL,eye)==ACVR_OK);
    auto v=transform_point(eye.view_from_scene,{0,12,0,1});check(near(v[0],0)&&near(v[1],0)&&near(v[2],0));
    v=transform_point(eye.view_from_scene,{0,0,-10,0});check(near(v[0],0)&&near(v[1],-5)&&near(v[2],-std::sqrt(75.)));
    // Recenter is based on a fresh physical pose, not accumulated on old output.
    const auto first=anchor;check(acvr::recenter_anchor(head,1.2f,anchor)==ACVR_OK&&std::memcmp(&first,&anchor,sizeof(anchor))==0);
    head=pose();head.orientation_xyzw[0]=head.orientation_xyzw[3]=s;
    check(acvr::recenter_anchor(head,1.2f,anchor)==ACVR_BAD_ARGUMENT&&std::memcmp(&first,&anchor,sizeof(anchor))==0);
}
void eye_ray_agreement() {
    // Rounded quaternions within the public validity tolerance must produce
    // the same rigid transform for eyes and tracked gun rays.
    auto anchor=pose(),grip=pose();anchor.orientation_xyzw[1]=anchor.orientation_xyzw[3]=std::sqrt(.5004f);
    anchor.position_m[0]=2;grip.position_m[0]=.032f;
    auto eye=init<acvr_eye>();check(acvr::compose_eye(anchor,grip,10,{-.7f,.7f,-.7f,.7f},.1f,100,ACVR_GRAPHICS_GL,eye)==ACVR_OK);
    const float angle[]{0,0,0,1};auto ray=init<acvr_ray>();
    check(acvr::anchored_muzzle_ray(anchor,grip,pose(),angle,10,5,ray)==ACVR_OK);
    const auto origin=transform_point(eye.view_from_scene,{ray.origin_scene[0],ray.origin_scene[1],ray.origin_scene[2],1});
    const auto direction=transform_point(eye.view_from_scene,{ray.direction_scene[0],ray.direction_scene[1],ray.direction_scene[2],0});
    check(near(origin[0],0)&&near(origin[1],0)&&near(origin[2],0));
    check(near(direction[0],0)&&near(direction[1],0)&&near(direction[2],-1));
}
void invalid_atomic() {
    auto eye=init<acvr_eye>();for(auto &n:eye.view_from_scene)n=17;for(auto &n:eye.projection_from_view)n=23;
    const auto original=eye;auto p=pose();const acvr::EyeFov fov{-.7f,.7f,-.7f,.7f};
    auto reject=[&](acvr_pose input,float scale,acvr::EyeFov f,float n,float far,uint32_t api) {
        check(acvr::compose_eye(pose(),input,scale,f,n,far,api,eye)!=ACVR_OK);
        check(std::memcmp(&eye,&original,sizeof(eye))==0);
    };
    reject(p,0,fov,.1f,10,ACVR_GRAPHICS_GL);reject(p,1,fov,0,10,ACVR_GRAPHICS_GL);
    reject(p,1,fov,10,10,ACVR_GRAPHICS_GL);reject(p,1,{0,0,-.7f,.7f},.1f,10,ACVR_GRAPHICS_GL);
    reject(p,1,fov,1,std::nextafter(1.f,2.f),ACVR_GRAPHICS_GL);
    reject(p,1,{.7f,std::nextafter(.7f,1.f),-.7f,.7f},.1f,10,ACVR_GRAPHICS_GL);
    reject(p,1,{-2,2,-.7f,.7f},.1f,10,ACVR_GRAPHICS_GL);reject(p,1,fov,.1f,10,ACVR_GRAPHICS_D3D11);
    p.orientation_xyzw[3]=2;reject(p,1,fov,.1f,10,ACVR_GRAPHICS_GL);
    p=pose();p.position_m[0]=std::numeric_limits<float>::max();reject(p,10,fov,.1f,10,ACVR_GRAPHICS_GL);
    p=pose();reject(p,std::numeric_limits<float>::denorm_min(),fov,std::numeric_limits<float>::denorm_min(),1,ACVR_GRAPHICS_GL);
    p.position_m[0]=std::numeric_limits<float>::quiet_NaN();reject(p,1,fov,.1f,10,ACVR_GRAPHICS_GL);
}
}
int main() {try {frusta();anchors_and_ipd();recenter();eye_ray_agreement();invalid_atomic();}catch(const std::exception &e){std::cerr<<e.what()<<'\n';return 1;}std::cout<<checks<<" provider view checks passed (CPU only)\n";}
