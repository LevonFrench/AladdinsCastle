// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "Presenter.h"
namespace acvr_publish {
struct TrackedPose {
 float position[3]={}, orientation[4]={0,0,0,1};
 bool valid=false;
};
struct TheatreFrame {
 ProjectionPacket views;
 TrackedPose aim,grip;
 // Body controls (motion cabinets): left grip pose, left trigger and the right squeeze value.
 TrackedPose leftGrip;
 float leftTrigger=0,squeeze=0;
 float leftStick[2]={},rightStick[2]={}; // thumbsticks, x right +, y up +
 unsigned width[2]={},height[2]={};
 float trigger=0;
 // start/coin are the universal stick clicks (U1/U2). quitHeld is both clicks held as a chord,
 // recenter pulses for one frame when the chord is released inside a second (U3). quitProgress is
 // 0..1 toward the 3 s quit and stays 0 until the chord has been held past the recentre window.
 bool focused=false,button2=false,start=false,coin=false,quitHeld=false,recenter=false;
 unsigned chordMs=0;float quitProgress=0;
};
// All callbacks run on the presenter's owning GL context. update is called
// even when shouldRender is false, so input release/quit is never draw-gated.
// The client samples its picture once in update and retains it for both eyes.
class TheatreClient {
public:
 virtual ~TheatreClient()=default;
 virtual void initialize()=0;
 virtual bool update(const TheatreFrame&)=0;
 virtual void draw_eye(unsigned eye,unsigned colour,const TheatreFrame&,bool srgb)=0;
 virtual void shutdown()=0;
};
bool start_theatre(void* host_window,TheatreClient* client);
bool theatre_running();
}
