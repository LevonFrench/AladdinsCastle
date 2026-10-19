// SPDX-License-Identifier: GPL-3.0-or-later
#include "presenter/legacy/AimMath.h"
#include "model2_script.h"
#include <cstdio>
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
 // Steering from roll: level is straight, 60 degrees clockwise (right side down) is full right.
 assert(std::abs(steer_from_roll(identity,60))<1e-6f);
 const float r=30*3.14159265f/180; // 60 degrees about the pointing axis (-z)
 const quat clockwise{0,0,-std::sin(r),std::cos(r)}, anticlockwise{0,0,std::sin(r),std::cos(r)};
 assert(std::abs(steer_from_roll(clockwise,60)-1)<1e-4f && std::abs(steer_from_roll(anticlockwise,60)+1)<1e-4f);
 // Analog cabinet controls, MAME's reversal honoured; open-frame coordinates back to 4:3.
 assert(model2_input::analog(0,0x20,0xe0,false)==0x20 && model2_input::analog(1,0x20,0xe0,false)==0xe0);
 assert(model2_input::analog(.5f,0x00,0xff,true)==0x80 && model2_input::analog(0,0x00,0xff,true)==0xff);
 assert(model2_input::analog(2,0x69,0xff,false)==0xff);
 bool off=false; assert(std::abs(model2_input::from_wide(.5f,93,496,off)-.5f)<1e-6f && !off);
 assert(std::abs(model2_input::from_wide(93.f/682,93,496,off))<1e-5f && !off);
 model2_input::from_wide(.05f,93,496,off); assert(off);
 assert(model2_input::find("hotdo","hotd")->type==model2_input::kind::light_gun);
 assert(model2_input::find("indy500d","indy500")->shifter==model2_input::gears::buttons);
 assert(!model2_input::find("vf2",nullptr));
 // Measured trim: the inverse puts the game's mark on the aim point.
 const auto *gun=model2_input::find("gunblade",nullptr);
 for(float aim:{.15f,.5f,.85f}) assert(std::abs(gun->trim[0]*model2_input::untrim(aim,gun->trim[0],gun->trim[1])+gun->trim[1]-aim)<1e-5f);
 assert(model2_input::find("vcop2",nullptr)->trim[0]==1 && model2_input::find("vcop2",nullptr)->trim[1]==0);
 assert(model2_input::shift(4,true,false)==4 && model2_input::shift(1,false,true)==1 && model2_input::shift(2,true,false)==3);
 // Proof scripts: settings hold until changed; snap counts.
 const char *path="controller-math-script.txt";
 FILE *f=std::fopen(path,"w"); std::fputs("# test\n1 u=.25 trigger=1 snap\n2 trigger=0 steer=-1 gripr=1\n3 snap\n",f); std::fclose(f);
 model2_script::ScriptFile script; std::string error; assert(script.load(path,error));
 assert(script.at(.5).trigger==0 && script.at(.5).snaps==0);
 assert(script.at(1.5).trigger==1 && script.at(1.5).u==.25f && script.at(1.5).snaps==1);
 assert(script.at(2.5).trigger==0 && script.at(2.5).steer==-1 && script.at(2.5).gripr==1 && script.at(2.5).u==.25f);
 assert(script.at(9).snaps==2);
 f=std::fopen(path,"w"); std::fputs("1 bogus=1\n",f); std::fclose(f); assert(!script.load(path,error));
 std::remove(path);
}
