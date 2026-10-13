// SPDX-License-Identifier: MIT
#pragma once
#include "acvr.h"

namespace acvr {
// Internal provider math, not a new public ABI. Angles are radians from the
// optical axis: left/down negative, right/up positive for a usual view.
struct EyeFov { float left, right, down, up; };
// Both poses use metres. The returned view matrix consumes scene units; near
// and far are supplied in metres and scaled exactly once. GL uses [-1,1] depth;
// Vulkan uses [0,1] depth and a flipped projection Y for a positive viewport.
// Other APIs are unsupported. Only the two matrices in eye are replaced.
acvr_result compose_eye(const acvr_pose &scene_from_stage,
                        const acvr_pose &stage_from_eye, float units_per_metre,
                        EyeFov fov, float near_m, float far_m,
                        uint32_t graphics_api, acvr_eye &eye) noexcept;
// Put the supplied head at (0,target_height_m,0), with its horizontal forward
// direction facing -Z. Retain physical pitch/roll in subsequent eye poses.
// A vertical forward direction cannot define yaw and returns BAD_ARGUMENT.
// Invalid input never changes the destination; reference-space resets and
// persistence remain provider responsibilities.
acvr_result recenter_anchor(const acvr_pose &stage_from_head,
                            float target_height_m, acvr_pose &scene_from_stage) noexcept;
}
