// Copyright 2026 AladdinsCastle contributors
// SPDX-License-Identifier: GPL-3.0-or-later
#include "ACFacing.h"
#include <iostream>
#include <limits>
#include <stdexcept>

void Require(bool value, const char* message)
{
  if (!value) throw std::runtime_error(message);
}
bool Near(float a, float b) { return std::abs(a-b) < 1e-5f; }
std::array<float, 4> Yaw(float angle) { return {0, std::sin(angle/2), 0, std::cos(angle/2)}; }
bool Apply(ACFacing::State& state, float yaw, bool focus=true, bool valid=true)
{
  return state.Apply(focus,valid,valid,true,{3,2,7},Yaw(yaw));
}
int main()
{
  try
  {
    constexpr float pi = 3.14159265358979323846f;
    for (float yaw : {0.0f,pi/2,-pi/2,pi})
    {
      const auto heading=ACFacing::Heading(Yaw(yaw));
      Require(heading.has_value(),"Valid yaw");
      const auto forward=ACFacing::ToGame(ACFacing::Vector{-std::sin(yaw),0,-std::cos(yaw)},*heading);
      Require(Near(forward[0],0)&&Near(forward[1],0)&&Near(forward[2],-1),"Facing basis sign");
      const auto translated=ACFacing::ToGame(ACFacing::Vector{std::cos(yaw),0,-std::sin(yaw)},*heading);
      Require(Near(translated[0],1)&&Near(translated[2],0),"Relative position matches direction");
      ACFacing::Rotation rotation{std::cos(yaw),0,std::sin(yaw),0,1,0,-std::sin(yaw),0,std::cos(yaw)};
      const auto mapped=ACFacing::ToGame(rotation,*heading);
      Require(Near(mapped[0],1)&&Near(mapped[4],1)&&Near(mapped[8],1),"Eye/aim rotation basis");
    }
    Require(!ACFacing::Heading({0,0,0,0}),"Zero orientation rejected");
    Require(!ACFacing::Heading({std::sin(pi/4),0,0,std::cos(pi/4)}),"Vertical heading deferred");
    // A yaw anchor must leave pitch/roll and subsequent head motion intact.
    const float pitch=0.3f, roll=-0.2f;
    const ACFacing::Rotation pitch_rotation{1,0,0,0,std::cos(pitch),-std::sin(pitch),0,std::sin(pitch),std::cos(pitch)};
    const ACFacing::Rotation roll_rotation{std::cos(roll),-std::sin(roll),0,std::sin(roll),std::cos(roll),0,0,0,1};
    const auto tilt=ACFacing::Compose(pitch_rotation,roll_rotation);
    const ACFacing::Rotation yaw_rotation{0,0,1,0,1,0,-1,0,0};
    const auto tilted_raw=ACFacing::Compose(yaw_rotation,tilt);
    const auto tilted_game=ACFacing::ToGame(tilted_raw,pi/2);
    for(int i=0;i<9;++i) Require(Near(tilted_game[i],tilt[i]),"Pitch and roll preserved");
    // Nonidentity camera anchor must compose with facing-adjusted eye/aim axes.
    const ACFacing::Rotation raw{0,0,1,0,1,0,-1,0,0};
    const ACFacing::Rotation anchor{1,0,0,0,0,-1,0,1,0};
    const auto eye=ACFacing::Compose(anchor,ACFacing::ToGame(raw,pi/2));
    const auto hand=ACFacing::Compose(anchor,ACFacing::ToGame(raw,pi/2));
    const auto wrong=ACFacing::Compose(anchor,raw);
    for(int i=0;i<9;++i) Require(Near(eye[i],anchor[i])&&Near(eye[i],hand[i]),"Camera anchor preserves facing basis");
    Require(!Near(eye[0],wrong[0]),"Fixture detects A*rawR regression");
    ACFacing::State state;
    state.ResetSession(false);
    Require(!Apply(state,pi/2)&&!state.Captured(),"Focus is not wear detection");
    state.Request();
    Require(!Apply(state,pi/2,false)&&state.Pending(),"Request survives missing focus");
    Require(!Apply(state,pi/2,true,false)&&state.Pending(),"Request survives invalid pose");
    Require(Apply(state,pi/2)&&!state.Pending(),"Explicit controller confirmation");
    Require(!Apply(state,0)&&Near(state.Yaw(),pi/2),"Head turns do not move anchor");
    Require(!Apply(state,0,false)&&!Apply(state,0,true)&&Near(state.Yaw(),pi/2),"Focus regain is not recenter");
    state.ResetSession(true);
    Require(!Apply(state,0),"Unknown presence does not auto anchor");
    state.ObservePresence(false);
    Require(!state.Pending()&&!Apply(state,0),"Known absent does not anchor");
    state.ObservePresence(true);
    Require(state.Pending()&&!Apply(state,-pi/2,false),"Don waits for focused valid pose");
    Require(Apply(state,-pi/2),"Don capture");
    state.ObservePresence(true);
    Require(!state.Pending()&&!Apply(state,0)&&Near(state.Yaw(),-pi/2),"Repeated present does not swim");
    state.ObservePresence(false);state.ObservePresence(true);
    Require(Apply(state,pi/2),"Redon captures new heading once");
    state.ObservePresence(false);state.ObservePresence(true);state.ObservePresence(false);
    Require(!Apply(state,0),"Interrupted don cancelled");
    state.Request();
    Require(!Apply(state,0)&&state.Pending(),"Manual request waits while known absent");
    state.ObservePresence(true);
    Require(Apply(state,0),"Pending request applies after redon");
    state.ResetAnchor();
    Require(Apply(state,0),"New reference space can capture current known presence");
    state.ResetSession(true);state.ObservePresence(true);
    Require(!state.Apply(true,true,true,false,{0,0,0},Yaw(0))&&state.Pending(),"Two eyes required");
    Require(!state.Apply(true,true,true,true,{std::numeric_limits<float>::infinity(),0,0},Yaw(0)),"Finite center required");
    // Same basis for both eye offsets and a center/corner aim ray onto game z=-2.
    const float yaw=pi/2;
    const auto left=ACFacing::ToGame(ACFacing::Vector{0,0,0.031f},yaw);
    const auto right=ACFacing::ToGame(ACFacing::Vector{0,0,-0.034f},yaw);
    Require(Near(right[0]-left[0],0.065f),"Asymmetric stereo separation preserved");
    const auto ray=ACFacing::ToGame(ACFacing::Vector{-2,0,-0.5f},yaw);
    Require(Near(ray[0],0.5f)&&Near(ray[2],-2),"Corner ray remains registered to panel");
    // Physical controller rays, eye origins and a nonidentity camera anchor
    // agree at four synthetic panel corners, despite yaw and room translation.
    for(float heading : {0.0f,pi/2,-pi/2,pi})
      for(float corner_x : {-0.8f,0.8f})
        for(float corner_y : {-0.45f,0.45f})
        {
          const ACFacing::Vector room_origin{4,1.5f,-3};
          const auto physical_offset=ACFacing::ToGame(ACFacing::Vector{0.08f,-0.1f,0.03f},-heading);
          const ACFacing::Vector hand_position{room_origin[0]+physical_offset[0],room_origin[1]+physical_offset[1],room_origin[2]+physical_offset[2]};
          const ACFacing::Vector relative{hand_position[0]-room_origin[0],hand_position[1]-room_origin[1],hand_position[2]-room_origin[2]};
          const auto game_origin=ACFacing::ToGame(relative,heading);
          const auto physical_ray=ACFacing::ToGame(ACFacing::Vector{corner_x-0.08f,corner_y+0.1f,-2.03f},-heading);
          const auto game_ray=ACFacing::ToGame(physical_ray,heading);
          const float distance=(-2-game_origin[2])/game_ray[2];
          Require(Near(game_origin[0]+distance*game_ray[0],corner_x)&&Near(game_origin[1]+distance*game_ray[1],corner_y),"Corner intersection preserves registration");
        }
    std::cout<<"Synthetic facing/presence state checks passed\n";
    return 0;
  }
  catch(const std::exception& error) { std::cerr<<error.what()<<'\n';return 1; }
}
