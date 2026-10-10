// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include "acvr.h"
#include <array>
#include <cstdint>
namespace n22 {
struct VideoSnapshot;
enum class PolygonFadePolicy {Absent,Super22InputFold};
struct PolygonFadeState {
    PolygonFadePolicy policy=PolygonFadePolicy::Absent;
    uint64_t tick=0;
    std::array<uint8_t,3> rgb{255,255,255};
};
// Explicit pinned generic input order, not a hardware-parity assertion.
acvr_result copy_super22_polygon_fade(const VideoSnapshot &,PolygonFadeState &out);
acvr_result validate_polygon_fade(const PolygonFadeState &,uint64_t frame_tick);
// Caller validates policy/tick first. Enable is global, not per-channel.
bool polygon_fade_active(const PolygonFadeState &);
std::array<double,3> polygon_fade_factors(const PolygonFadeState &);
}
