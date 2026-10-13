// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include "acvr.h"
#include <array>
#include <cstdint>
namespace n22 {
struct VideoSnapshot;
enum class FogPolicy {Absent,System22Constant,Super22Table};
struct FogState {
    FogPolicy policy=FogPolicy::Absent;
    uint64_t tick=0;
    std::array<uint16_t,8> attributes{};
    std::array<std::array<uint8_t,8192>,4> tables{};
    std::array<uint8_t,3> rgb{};
};
struct FogQuad {
    bool provided=false,has_native_depth=false,constant_provided=false;
    uint64_t tick=0;
    uint32_t colour_word=0,cz_adjust=0;
    uint8_t cz_type=0;
    // Original integer geo depth, independent of reconstructed scene/eye depth.
    std::array<int32_t,3> native_depth{};
    // Explicit System22 board-provider seam. -1 explicitly selects generic
    // unfogged fallback; absent data is UNSUPPORTED, never a guessed constant.
    int16_t constant_alpha=-1;
    std::array<uint8_t,3> constant_rgb{};
};
struct FogDecision {
    FogPolicy policy=FogPolicy::Absent;
    bool enabled=false;
    uint8_t alpha=255;
    int32_t bank=-1,delta=0;
    std::array<uint8_t,3> rgb{};
};
// Computed exactly once when the owned frame is prepared, before eye clipping.
// Alpha is the unfogged weight, never transparency or native priority alpha.
struct FogSamples {
    bool enabled=false;
    std::array<uint8_t,3> alpha{255,255,255};
    std::array<uint8_t,3> rgb{};
};
// Explicitly chosen Super22 policy only. No file I/O, global engine state,
// native endian conversion, board/game detection or pixel/render integration.
acvr_result copy_super22_fog(const VideoSnapshot &,FogState &out);
acvr_result validate_fog_binding(const FogState &,const FogQuad &,uint64_t frame_tick);
acvr_result decide_fog(const FogState &,const FogQuad &,uint32_t vertex,FogDecision &out);
}
