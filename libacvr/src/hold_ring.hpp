// SPDX-License-Identifier: MIT
#pragma once
// The U4 hold ring (universal controls contract v1): visible feedback while both stick clicks are held to quit.
//
// Pure logic and pure geometry, no graphics calls: a host feeds the 0..1 progress of the 3 s quit hold, gets a
// HoldRingView back, and draws it with whatever it has. hold_ring_gl.hpp is the ready-made GL drawing for hosts
// that draw their own eye images.
//
//   - Nothing is shown for the first second. A recentre chord (both clicks released within 1 s) must never
//     flash a ring, so the ring only appears once the hold can no longer be a recentre.
//   - From 1 s to 3 s the ring fills clockwise from 12 o'clock: fill goes 0 to 1 and is 1 exactly when the quit fires.
//   - Releasing early, or a host cancelling the quit (pinball table adjust), hides the ring at once.
//   - When the quit fires the ring is full and flashes once: `flash` is 1 at that moment and falls to 0 over flash_ms.
//     The flash plays out even if the clicks are released at that moment.
#include <algorithm>
#include <cmath>
#include <cstdint>
#include "stick_chord.hpp"

namespace acvr {

struct HoldRingTiming {
    uint32_t show_ms = 1000;    // the ring appears when the hold passes this (the recentre window)
    uint32_t quit_ms = 3000;    // the quit fires here; same value as StickChordTiming::quit_ms
    uint32_t flash_ms = 700;    // how long the one flash lasts
};

struct HoldRingView {
    bool visible = false;
    float fill = 0;     // 0 when it appears (1 s), 1 when the quit fires (3 s); sweeps clockwise from 12 o'clock
    float flash = 0;    // 1 at the moment the quit fires, down to 0 over flash_ms
    bool quit = false;  // the quit has fired: the ring is full
};

class HoldRing {
public:
    explicit HoldRing(HoldRingTiming timing = {}) : t(timing) {}
    // now_ms: any monotonic millisecond clock. quit_progress: 0..1 of the quit hold, 0 when the clicks are not held,
    // the hold was released or the quit was cancelled.
    HoldRingView update(uint64_t now_ms, float quit_progress) {
        HoldRingView v;
        if (now_ms < last_) reset();   // the clock went backwards: start again
        last_ = now_ms;
        const float quit = float(t.quit_ms ? t.quit_ms : 1u);
        const float held_ms = std::max(0.f, quit_progress) * quit;
        const bool shown = held_ms + .5f >= float(t.show_ms);
        const bool full = quit_progress >= .9999f;
        if (full && !fired_) { fired_ = true; fired_at_ = now_ms; }
        const bool flashing = fired_ && now_ms - fired_at_ < t.flash_ms;
        if (fired_ && !full && !flashing) fired_ = false;   // the hold is over and so is the flash: ready for the next one
        if (flashing) {
            v.visible = true; v.fill = 1.f; v.quit = true;
            v.flash = 1.f - float(now_ms - fired_at_) / float(t.flash_ms ? t.flash_ms : 1u);
        } else if (full) {
            v.visible = true; v.fill = 1.f; v.quit = true;
        } else if (shown) {
            const float span = std::max(1.f, quit - float(t.show_ms));
            v.visible = true; v.fill = std::min(1.f, std::max(0.f, (held_ms - float(t.show_ms)) / span));
        }
        return v;
    }
    // From the shared chord resolver. A hold the host cancelled shows nothing.
    HoldRingView update(uint64_t now_ms, const StickChordState &s) {
        return update(now_ms, s.quit_cancelled || !s.chord ? 0.f : s.quit_progress);
    }
    void reset() { fired_ = false; fired_at_ = 0; last_ = 0; }
private:
    HoldRingTiming t;
    bool fired_ = false;
    uint64_t fired_at_ = 0, last_ = 0;
};

// ---- geometry: pixel rectangles, so a host can fill them with a clear, a quad or a sprite -------------------------
// A ring of square "beads" round a circle (y up, origin at the bottom left, like GL framebuffers), painted in order:
// a dark outline under every bead, then the beads, dim when unlit and bright when lit.
struct HoldRingRect {
    float x = 0, y = 0, w = 0, h = 0;   // pixels; x, y is the bottom left corner
    float rgb[3]{};                      // display values (already in the colours the player should see)
};
constexpr int kHoldRingBeads = 48;
constexpr int kHoldRingMaxRects = kHoldRingBeads * 2;
struct HoldRingRects {
    HoldRingRect r[kHoldRingMaxRects];
    int n = 0;
};

// cx, cy: ring centre in pixels. radius: ring radius in pixels (to the middle of the bead).
inline HoldRingRects hold_ring_rects(const HoldRingView &v, float cx, float cy, float radius) {
    HoldRingRects out;
    if (!v.visible || !(radius >= 6.f)) return out;
    const float bead = std::max(3.f, std::floor(radius * .16f));
    const float grow = 1.f + .5f * std::max(0.f, v.flash);
    const float outline = 2.f;
    const int lit = std::min(kHoldRingBeads, std::max(0, int(std::floor(v.fill * kHoldRingBeads + .5f))));
    const float tau = 6.28318530718f;
    auto at = [&](int i, float &x, float &y) {
        const float a = float(i) * tau / float(kHoldRingBeads);   // clockwise from 12 o'clock
        x = cx + radius * std::sin(a); y = cy + radius * std::cos(a);
    };
    for (int i = 0; i < kHoldRingBeads; ++i) {
        float x, y; at(i, x, y);
        const float s = bead * (i < lit ? grow : 1.f) + 2.f * outline;
        HoldRingRect &q = out.r[out.n++];
        q.x = x - s * .5f; q.y = y - s * .5f; q.w = q.h = s; q.rgb[0] = q.rgb[1] = q.rgb[2] = .02f;
    }
    // Amber while it fills, green once it has fired, whitened by the flash.
    const float amber[3]{1.f, .78f, .18f}, green[3]{.25f, 1.f, .4f};
    for (int i = 0; i < kHoldRingBeads; ++i) {
        float x, y; at(i, x, y);
        const bool on = i < lit;
        const float s = bead * (on ? grow : 1.f);
        HoldRingRect &q = out.r[out.n++];
        q.x = x - s * .5f; q.y = y - s * .5f; q.w = q.h = s;
        for (int c = 0; c < 3; ++c) {
            if (!on) { q.rgb[c] = .28f; continue; }
            const float base = v.quit ? green[c] : amber[c];
            q.rgb[c] = base + (1.f - base) * std::min(1.f, std::max(0.f, v.flash));
        }
    }
    return out;
}

// Where a ring goes in an eye image. m: the 4x4 clip-from-local matrix, column-major (a view-projection times the
// anchor's own frame). centre and edge: two points in the anchor's local space, the edge one radius away from the
// centre, sideways. fb_w, fb_h: the eye image in pixels. ok is false when the centre is behind the eye.
struct HoldRingPlacement {
    float cx = 0, cy = 0, radius = 0;
    bool ok = false;
};
inline HoldRingPlacement place_hold_ring(const float m[16], const float centre[3], const float edge[3], int fb_w, int fb_h) {
    HoldRingPlacement p;
    auto project = [&](const float q[3], float &x, float &y) {
        const float cxp = m[0] * q[0] + m[4] * q[1] + m[8] * q[2] + m[12];
        const float cyp = m[1] * q[0] + m[5] * q[1] + m[9] * q[2] + m[13];
        const float cwp = m[3] * q[0] + m[7] * q[1] + m[11] * q[2] + m[15];
        if (!(cwp > 1e-4f)) return false;
        x = (cxp / cwp * .5f + .5f) * float(fb_w); y = (cyp / cwp * .5f + .5f) * float(fb_h);
        return true;
    };
    float ex = 0, ey = 0;
    if (!project(centre, p.cx, p.cy) || !project(edge, ex, ey)) return p;
    p.radius = std::sqrt((ex - p.cx) * (ex - p.cx) + (ey - p.cy) * (ey - p.cy));
    p.ok = p.radius >= 1.f && std::isfinite(p.cx) && std::isfinite(p.cy) && std::isfinite(p.radius);
    return p;
}

}  // namespace acvr
