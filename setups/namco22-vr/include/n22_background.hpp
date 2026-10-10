// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include "acvr.h"
#include <array>
#include <cstdint>
namespace n22 {
struct VideoSnapshot;
enum class BackgroundPolicy {Absent,Super22Mixer};
struct BackgroundState {
    BackgroundPolicy policy=BackgroundPolicy::Absent;
    uint64_t tick=0;
    std::array<uint8_t,3> rgb{};
};
// Reads only already copied mixer bytes. No native globals, files or fades/LUTs.
acvr_result copy_super22_background(const VideoSnapshot &,BackgroundState &out);
acvr_result validate_background(const BackgroundState &,uint64_t frame_tick);
// Caller validates first; explicitly absent state always returns black.
uint32_t background_rgb(const BackgroundState &);
}
