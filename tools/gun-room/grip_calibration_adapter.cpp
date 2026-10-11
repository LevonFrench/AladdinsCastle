// SPDX-License-Identifier: MIT
#include "grip_calibration_adapter.hpp"
#include <cmath>
namespace room {
bool selected_grip_sample(const acvr_controller_sample &controls,
                          const acvr_tracking &tracking,unsigned hand,int64_t wall_time_ns,
                          GripSample &out){
    if(hand>ACVR_HAND_LEFT||!controls.display_id||controls.display_id!=tracking.display_id||
       controls.sample_time_ns!=tracking.sample_time_ns)return false;
    const auto &selected=hand==ACVR_HAND_RIGHT?tracking.right:tracking.left;
    out={};out.display_id=controls.display_id;out.sample_time_ns=controls.sample_time_ns;
    out.wall_time_ns=wall_time_ns;out.focused=controls.focused!=0;
    out.tracked=(selected.grip_flags&3u)==3u&&(selected.aim_flags&3u)==3u;
    constexpr uint32_t mask=ACVR_CALIB_SECONDARY_ACTIVE|ACVR_CALIB_STICK_X_ACTIVE|
                            ACVR_CALIB_STICK_Y_ACTIVE|ACVR_CALIB_TRIGGER_ACTIVE;
    out.actions_active=(controls.active[hand]&mask)==mask;
    out.secondary_down=controls.secondary_down[hand]!=0;
    out.trigger_up=std::isfinite(controls.trigger[hand])&&
                   controls.trigger[hand]>=0.f&&controls.trigger[hand]<.1f;
    out.stick_x=controls.stick_x[hand];out.stick_y=controls.stick_y[hand];
    return true;
}
}
