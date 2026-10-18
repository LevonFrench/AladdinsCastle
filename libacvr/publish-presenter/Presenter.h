// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <cstdint>
namespace acvr_publish {
struct EyeView {
 float position[3]={0,0,0},orientation[4]={0,0,0,1};
 float fov[4]={-.6f,.6f,.6f,-.6f}; // OpenXR radians: left,right,up,down
};
struct ProjectionPacket {
 EyeView eyes[2];
 std::int64_t frame_time=0; // predicted time used to render this exact image pair
 bool valid=false; // false publishes the existing screen-quad mode
};
// Screen-controller semantics; each emulator retains its own input mapping.
struct ControllerInput {
 float u=.5f,v=.5f;
 bool active=false,offScreen=true,trigger=false,start=false,coin=false,quitHeld=false;
};
float distance_metres();
// Host_Window from WinGL.h, with its owning context current. One presenter/session.
bool start(void* host_window);
void publish(unsigned left_fbo,unsigned right_fbo,unsigned width,unsigned height);
void publish_projection(unsigned left_fbo,unsigned right_fbo,unsigned width,unsigned height,const ProjectionPacket& views);
// Opt-in located LOCAL views. First call can return false until the next frame.
bool located_views(ProjectionPacket& views);
// False retains resources for retry on their owning GL context.
bool stop();
void queue_shot_pulse();
void update_fov(float left,float right,float bottom,float top);
ControllerInput controller_input(); // 250 ms lease; expired snapshots are neutral
}
