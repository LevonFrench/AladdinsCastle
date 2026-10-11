// SPDX-License-Identifier: MIT
#include "room_scene.hpp"
#include <cmath>
#include <iostream>
#include <stdexcept>
void check(bool value){if(!value)throw std::runtime_error("room geometry check failed");}
int main(){
    room::Scene scene;
    const auto ray=room::Ray{{0,1.8f,0},{0,0,-1}};
    check(scene.intersect(ray)==1);
    check(scene.shoot(ray)&&scene.hits==1&&scene.targets[1].lit);
    check(scene.intersect({{0,1.8f,0},{0,0,1}})==-1);
    std::array<float,4> q{0,0,0,1};
    check(room::two_hand_bore({0,1,0},{0,1,-.25f},{0,0,-.25f},.05f,q));
    check(std::abs(q[3]-1)<1e-5f);
    check(!room::two_hand_bore({0,1,0},{1,1,0},{0,0,-.25f},.05f,q));
    const auto left=room::yaw_angle(-30),right=room::yaw_angle(30);
    check(left[1]<0&&right[1]>0&&std::abs(left[3]-right[3])<1e-6f);
    std::cout<<"gun room geometry checks passed\n";
}
