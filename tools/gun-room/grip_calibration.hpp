// SPDX-License-Identifier: MIT
#pragma once
#include <array>
#include <cstdint>

namespace room {
struct GripAngles {float yaw_deg=0,pitch_deg=0;};
bool angles_ok(GripAngles value);
std::array<float,4> grip_quaternion(GripAngles value); // XYZW: Y yaw then X pitch
struct GripPanelLayout {float x=0,y=0,scale=0,width=0,height=0;};
GripPanelLayout grip_panel_layout(uint32_t eye_width,uint32_t eye_height);

struct GripSample {
    uint64_t display_id=0;
    int64_t sample_time_ns=0, wall_time_ns=0;
    bool focused=false,tracked=false,actions_active=false,secondary_down=false,trigger_up=false;
    float stick_x=0,stick_y=0;
};
enum class GripFeedback {Ready,Editing,Saved,Cancelled,Unavailable,SaveFailed};
enum class GripEvent {None,Preview,Restore,Save};
class GripCalibration {
public:
    explicit GripCalibration(GripAngles initial={});
    GripEvent sample(const GripSample &);
    GripEvent no_sample(int64_t wall_time_ns);
    GripEvent unavailable();
    GripEvent cancel();
    void saved();
    void save_failed();
    GripAngles preview() const {return draft_;}
    GripAngles committed() const {return committed_;}
    GripFeedback feedback() const {return feedback_;}
    bool editing() const {return editing_;}
private:
    GripEvent restore(GripFeedback reason);
    GripAngles committed_{},draft_{};
    GripFeedback feedback_=GripFeedback::Ready;
    uint64_t last_display_=0;
    int64_t last_sample_=0,last_wall_=0,down_since_=0;
    bool editing_=false,down_=false,long_hold_=false,require_release_=true,seen_=false;
};
}
