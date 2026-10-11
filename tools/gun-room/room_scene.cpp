// SPDX-License-Identifier: MIT
#include "room_scene.hpp"
#include <algorithm>
#include <cmath>
#include <limits>
#include <sstream>

namespace room {
namespace {
float dot(Vec3 a,Vec3 b){return a[0]*b[0]+a[1]*b[1]+a[2]*b[2];}
Vec3 minus(Vec3 a,Vec3 b){return {a[0]-b[0],a[1]-b[1],a[2]-b[2]};}
float length(Vec3 a){return std::sqrt(dot(a,a));}
}
Scene::Scene():targets{{{{-.55f,1.5f,-3.f}},.21f,false},{{{0,1.8f,-4.2f}},.25f,false},
                       {{{.65f,1.2f,-3.5f}},.19f,false}}{}
int Scene::intersect(const Ray &r) const {
    const float a=dot(r.direction,r.direction);
    if(!std::isfinite(a)||a<1e-12f)return -1;
    float best=std::numeric_limits<float>::infinity();int hit=-1;
    for(size_t i=0;i<targets.size();++i){
        const auto delta=minus(r.origin,targets[i].center);
        const float b=2*dot(delta,r.direction),c=dot(delta,delta)-targets[i].radius*targets[i].radius;
        const float discriminant=b*b-4*a*c;
        if(discriminant<0||!std::isfinite(discriminant))continue;
        const float t=(-b-std::sqrt(discriminant))/(2*a);
        if(t>0&&t<best){best=t;hit=static_cast<int>(i);}
    }
    return hit;
}
bool Scene::shoot(const Ray &r){
    const int i=intersect(r);if(i<0)return false;
    targets[static_cast<size_t>(i)].lit=true;++hits;return true;
}
bool two_hand_bore(Vec3 main,Vec3 off,Vec3 second,float radius,std::array<float,4> &q){
    if(!std::isfinite(radius)||radius<=0)return false;
    for(float v:main)if(!std::isfinite(v))return false;
    for(float v:off)if(!std::isfinite(v))return false;
    for(float v:second)if(!std::isfinite(v))return false;
    if(length(minus(off,Vec3{main[0]+second[0],main[1]+second[1],main[2]+second[2]}))>radius)return false;
    auto direction=minus(off,main);const float n=length(direction);
    if(n<1e-4f)return false;
    for(float &v:direction)v/=n;
    // Shortest rotation from canonical -Z to the line; antipodal +Z is a
    // deterministic half turn around +Y. This leaves no arbitrary bank drift.
    const float d=-direction[2];
    if(d<-.99999f){q={0,1,0,0};return true;}
    const Vec3 cross{direction[1],-direction[0],0};
    const float s=std::sqrt(2*(1+d));
    q={cross[0]/s,cross[1]/s,cross[2]/s,s*.5f};return true;
}
std::array<float,4> yaw_angle(float degrees){
    const float half=degrees*.008726646259971648f;
    return {0,std::sin(half),0,std::cos(half)};
}
std::string preview_svg(const Scene &scene){
    std::ostringstream out;
    out<<"<svg xmlns='http://www.w3.org/2000/svg' viewBox='0 0 960 540'>\n"
          "<rect width='960' height='540' fill='#111726'/><path d='M0 540L250 290H710L960 540' fill='#273143'/>"
          "<path d='M250 290V90H710V290' fill='none' stroke='#647184' stroke-width='4'/>"
          "<path d='M480 515L480 290' stroke='#dfeeff' stroke-width='3'/>\n";
    for(const auto &t:scene.targets){
        const float z=-t.center[2],x=480+t.center[0]*480/z,y=270-(t.center[1]-1.45f)*480/z;
        const float r=t.radius*480/z;
        out<<"<circle cx='"<<x<<"' cy='"<<y<<"' r='"<<r
           <<"' fill='"<<(t.lit?"#f8bd43":"#db5969")<<"' stroke='#f2f0df' stroke-width='4'/>\n";
    }
    out<<"<path d='M480 505L480 270' stroke='#74d9fb' stroke-width='2'/><text x='30' y='45' "
          "fill='white' font-family='sans-serif' font-size='26'>ACVR gun room • geometry preview</text>"
          "<text x='30' y='80' fill='#a8b7c7' font-family='sans-serif' font-size='18'>"
          "Native eye capture requires OpenXR loader and an active session</text></svg>\n";
    return out.str();
}
}
