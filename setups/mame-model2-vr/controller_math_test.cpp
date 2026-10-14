// SPDX-License-Identifier: GPL-3.0-or-later
#include "presenter/AimMath.h"
#include "model2_input.h"
#include <cassert>
#include <limits>
int main()
{
 using namespace acvr_aim;
 const quat identity{0,0,0,1};
 auto centre=intersect({0,0,0},identity,{0,0,-4},identity,4,3);
 assert(centre.on_screen && centre.u==.5f && centre.v==.5f);
 auto corner=intersect({-2,1.5f,0},identity,{0,0,-4},identity,4,3);
 assert(corner.on_screen && corner.u==0 && corner.v==0);
 assert(!intersect({2.01f,0,0},identity,{0,0,-4},identity,4,3).on_screen);
 assert(!intersect({0,0,-5},identity,{0,0,-4},identity,4,3).on_screen);
 const float s=std::sqrt(.5f); const quat yaw{0,s,0,s};
 auto turned=intersect({0,0,0},yaw,{-4,0,0},yaw,4,3);
 assert(turned.on_screen && std::abs(turned.u-.5f)<1e-5f);
 assert(!intersect({0,0,0},yaw,{0,0,-4},identity,4,3).on_screen);
 assert(!intersect({0,0,0},identity,{0,0,-4},identity,0,3).on_screen);
 assert(!intersect({std::numeric_limits<float>::quiet_NaN(),0,0},identity,{0,0,-4},identity,4,3).on_screen);
 assert(model2_input::coordinate(0,137,630,false)==137);
 assert(model2_input::coordinate(1,137,630,false)==630);
 assert(model2_input::coordinate(.5f,36,425,false)==231);
 assert(model2_input::coordinate(.5f,137,630,true)==137);
 assert(model2_input::coordinate(std::numeric_limits<float>::quiet_NaN(),36,425,false)==36);
}
