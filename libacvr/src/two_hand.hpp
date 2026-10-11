// SPDX-License-Identifier: MIT
#pragma once
#include "acvr.h"
#include <array>

namespace acvr {
// Pure stage-space grip solver. `second_grip_local` is the authored static
// bind-world translation from the gun's primary grip. No motion node may move
// that anchor. Returns false without mutating out if the off hand is not on it.
bool solve_two_hand_grip(const acvr_pose &main,const acvr_pose &off,
                         const float gun_angle_xyzw[4],
                         std::array<float,3> second_grip_local,float capture_radius_m,
                         acvr_pose &out) noexcept;
}
