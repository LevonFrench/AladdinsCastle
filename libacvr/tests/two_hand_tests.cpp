// SPDX-License-Identifier: MIT
#include "two_hand.hpp"
#include <cmath>
#include <iostream>
#include <stdexcept>
namespace {acvr_pose pose(float x,float y,float z){acvr_pose p{};ACVR_INIT(&p);p.position_m[0]=x;p.position_m[1]=y;p.position_m[2]=z;p.orientation_xyzw[3]=1;return p;}}
void check(bool value){if(!value)throw std::runtime_error("two-hand check failed");}
int main(){
    const auto main=pose(0,1,0),off=pose(.08f,1,-.3f);const float angle[]{0,0,0,1};
    auto result=pose(9,9,9);
    check(acvr::solve_two_hand_grip(main,off,angle,{0,0,-.3f},.1f,result));
    check(result.position_m[0]==0&&result.position_m[1]==1&&result.position_m[2]==0);
    check(result.orientation_xyzw[1]<0);
    const float old=result.orientation_xyzw[1];
    check(!acvr::solve_two_hand_grip(main,pose(1,1,0),angle,{0,0,-.3f},.1f,result));
    check(result.orientation_xyzw[1]==old);
    auto yawed=pose(0,1,0);
    yawed.orientation_xyzw[1]=yawed.orientation_xyzw[3]=std::sqrt(.5f);
    const auto pitched_off=pose(-.3f,1.04f,0);
    check(acvr::solve_two_hand_grip(yawed,pitched_off,angle,{0,0,-.3f},.1f,result));
    float norm=0;for(float v:result.orientation_xyzw)norm+=v*v;
    check(std::abs(norm-1.f)<.0001f);
    check(std::abs(result.orientation_xyzw[0])>.01f && std::abs(result.orientation_xyzw[1])>.1f);
    std::cout<<"two-hand grip geometry checks passed\n";
}
