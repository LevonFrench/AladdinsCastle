// SPDX-License-Identifier: MIT
#include "fixed_capture_pose.hpp"
#include <cmath>
#include <cstring>
#include <iostream>
#include <limits>

int main(){
    auto near=[](float a,float b){return std::abs(a-b)<.0001f;};
    acvr_pose head{};ACVR_INIT(&head);head.orientation_xyzw[3]=1;
    head.position_m[0]=1;head.position_m[1]=1.6f;head.position_m[2]=2;
    std::array<acvr_pose,2> hands{};
    if(!acvr::fixed_capture_poses(head,hands))return 1;
    if(!near(hands[0].position_m[0],1.18f)||!near(hands[1].position_m[0],.82f)||
       !near(hands[0].position_m[1],1.42f)||!near(hands[0].position_m[2],1.45f))return 2;
    head.orientation_xyzw[1]=head.orientation_xyzw[3]=std::sqrt(.5f);
    if(!acvr::fixed_capture_poses(head,hands)||!near(hands[0].position_m[0],.45f)||
       !near(hands[0].position_m[2],1.82f)||!near(hands[1].position_m[2],2.18f))return 3;
    const auto before=hands;
    head.orientation_xyzw[3]=0;
    if(acvr::fixed_capture_poses(head,hands)||std::memcmp(before.data(),hands.data(),sizeof(hands)))return 4;
    head.orientation_xyzw[3]=std::sqrt(.5f);head.position_m[1]=std::numeric_limits<float>::infinity();
    if(acvr::fixed_capture_poses(head,hands)||std::memcmp(before.data(),hands.data(),sizeof(hands)))return 5;
    head.position_m[1]=0;head.version=0;
    if(acvr::fixed_capture_poses(head,hands))return 6;
    std::cout<<"Fixed capture pose placement, rotation and invalid-input preservation pass; no XR.\n";
    return 0;
}
