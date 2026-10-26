// SPDX-License-Identifier: MIT
#pragma once
// Small pieces an emulator host needs to show the U4 hold ring (hold_ring.hpp) in its own VR eye images. Pure logic,
// no graphics calls, so the emulator patches (Supermodel, Flycast, PCSX2, DolphinXR) can copy it and the libacvr
// tests can check it.
//
//   - Where the ring goes: the middle of a flat picture (a screen or quad layer, as in acvr-ps1), or straight ahead
//     in an eye image drawn for an OpenXR field of view (headsets use asymmetric fields of view, so the image middle
//     is a few degrees off straight ahead, outwards for each eye; rings there would make the eyes diverge).
//   - Holding the quit back until the ring's one flash has played, as acvr-ps1 does.
//   - The progress of a plain "both clicks held 3 s" hold, for hosts whose quit has no chord window (DolphinXR's
//     Exit hotkey, hold(left & right, 3.0)).
// windows.h defines min and max as macros unless NOMINMAX is set, which breaks std::min and std::max; emulator
// sources often include it first, so they are set aside for this header and put back at its end.
#pragma push_macro("min")
#pragma push_macro("max")
#undef min
#undef max
#include <algorithm>
#include <cmath>
#include <cstdint>
#include "hold_ring.hpp"

namespace acvr {

// A ring's centre and radius in pixels of an image, y up from the bottom left (GL). A host whose images have y down
// (Direct3D) flips: top = height - (y + h).
struct HoldRingSpot {
    float cx = 0, cy = 0, radius = 0;
};

// The ring's size in every host: 12 % of the picture's shorter side (acvr-ps1: 12 % of the screen height).
constexpr float kHoldRingRadiusOfShortSide = .12f;

// The middle of a flat picture.
inline HoldRingSpot hold_ring_screen_spot(int w, int h) {
    HoldRingSpot s;
    if (w <= 0 || h <= 0) return s;
    s.cx = float(w) * .5f; s.cy = float(h) * .5f;
    s.radius = float(std::min(w, h)) * kHoldRingRadiusOfShortSide;
    return s;
}

// Straight ahead in an eye image of w x h pixels drawn for an OpenXR field of view, given as the tangents of its four
// angles (tan(angleLeft) and tan(angleDown) are negative). Falls back to the middle for a degenerate field of view.
inline HoldRingSpot hold_ring_eye_spot(float tan_left, float tan_right, float tan_down, float tan_up, int w, int h) {
    HoldRingSpot s = hold_ring_screen_spot(w, h);
    const float sx = tan_right - tan_left, sy = tan_up - tan_down;
    if (s.radius <= 0 || !std::isfinite(sx) || !std::isfinite(sy) || !(sx > 1e-4f) || !(sy > 1e-4f)) return s;
    const float u = -tan_left / sx, v = -tan_down / sy;   // 0..1 across the image when straight ahead is inside it
    if (!(u > 0.f && u < 1.f && v > 0.f && v < 1.f)) return s;
    s.cx = u * float(w); s.cy = v * float(h);
    return s;
}

// Holds a host's own quit back until the ring's flash has played (HoldRingTiming::flash_ms after the quit fired), so
// the player sees the full ring flash before the picture goes. Feed it every frame.
class QuitAfterFlash {
public:
    explicit QuitAfterFlash(uint32_t delay_ms = HoldRingTiming{}.flash_ms) : delay_(delay_ms) {}
    // quit_fired: the host's quit edge (or level). True once, delay_ms after the first frame it was true.
    bool update(uint64_t now_ms, bool quit_fired) {
        if (done_) return false;
        if (quit_fired && !armed_) { armed_ = true; at_ = now_ms; }
        if (armed_ && (now_ms < at_ || now_ms - at_ >= delay_)) { done_ = true; return true; }
        return false;
    }
    // The ring cannot be shown any more (focus or input lost): a quit that has fired goes now. True once.
    bool flush() {
        if (!armed_ || done_) return false;
        done_ = true;
        return true;
    }
    bool pending() const { return armed_ && !done_; }
    void reset() { armed_ = done_ = false; at_ = 0; }
private:
    uint32_t delay_;
    bool armed_ = false, done_ = false;
    uint64_t at_ = 0;
};

// 0..1 progress of "both clicks held for hold_ms", restarting whenever either click is up. For hosts whose quit is a
// plain hold with no chord window, so the ring follows exactly the same count as the quit.
class BothHeldProgress {
public:
    explicit BothHeldProgress(uint32_t hold_ms = HoldRingTiming{}.quit_ms) : hold_(hold_ms ? hold_ms : 1u) {}
    float update(uint64_t now_ms, bool both_down) {
        if (!both_down) { held_ = false; return 0.f; }
        if (!held_ || now_ms < since_) { held_ = true; since_ = now_ms; }
        return std::min(1.f, float(now_ms - since_) / float(hold_));
    }
    void reset() { held_ = false; since_ = 0; }
private:
    uint32_t hold_;
    bool held_ = false;
    uint64_t since_ = 0;
};

}  // namespace acvr

#pragma pop_macro("max")
#pragma pop_macro("min")
