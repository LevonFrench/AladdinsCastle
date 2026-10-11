// SPDX-License-Identifier: MIT
#pragma once
#include "grip_calibration.hpp"
#include "acvr.h"
namespace room {
bool selected_grip_sample(const acvr_controller_sample &controls,
                          const acvr_tracking &tracking,unsigned hand,int64_t wall_time_ns,
                          GripSample &out);
}
