// SPDX-License-Identifier: MIT
// The emulator-host side of the U4 hold ring (hold_ring_host.hpp): where the ring goes in a flat picture and in an
// eye image, the quit held back for the flash, and the plain both-held progress DolphinXR's Exit hotkey follows.
// Driven together with the real HoldRing, a step every 10 ms like a display frame. No window, no GPU.
#include <cmath>
#include <cstdint>
#include <cstdio>

#include "hold_ring_host.hpp"

namespace {
int checks = 0, failed = 0;
void check(bool ok, const char *what, int line) {
    ++checks;
    if (!ok) { ++failed; std::fprintf(stderr, "FAILED line %d: %s\n", line, what); }
}
#define CHECK(x) check((x), #x, __LINE__)
bool close_to(float a, float b, float eps = .5f) { return std::fabs(a - b) <= eps; }

void spots() {
    // Flat picture: the middle, 12 % of the shorter side (acvr-ps1 uses 12 % of the screen height).
    const acvr::HoldRingSpot s = acvr::hold_ring_screen_spot(640, 480);
    CHECK(close_to(s.cx, 320) && close_to(s.cy, 240) && close_to(s.radius, 57.6f, .01f));
    const acvr::HoldRingSpot tall = acvr::hold_ring_screen_spot(480, 640);
    CHECK(close_to(tall.radius, 57.6f, .01f));
    CHECK(acvr::hold_ring_screen_spot(0, 480).radius == 0.f);

    // A symmetric field of view: straight ahead is the middle.
    const acvr::HoldRingSpot sym = acvr::hold_ring_eye_spot(-1.f, 1.f, -1.f, 1.f, 2000, 2000);
    CHECK(close_to(sym.cx, 1000) && close_to(sym.cy, 1000) && close_to(sym.radius, 240.f, .01f));

    // A Quest-like left eye: wider to the left (outwards), a little more down than up. Straight ahead lies right of
    // the middle and above it; the right eye mirrors it, so both rings sit at the same far point.
    const float l = std::tan(-0.942478f), r = std::tan(0.698132f), d = std::tan(-0.837758f), u = std::tan(0.785398f);
    const acvr::HoldRingSpot left = acvr::hold_ring_eye_spot(l, r, d, u, 1800, 1900);
    const acvr::HoldRingSpot right = acvr::hold_ring_eye_spot(-r, -l, d, u, 1800, 1900);
    CHECK(left.cx > 900.f + 50.f && right.cx < 900.f - 50.f);
    CHECK(close_to(left.cx, 1800.f - right.cx, .01f));
    CHECK(left.cy > 950.f && close_to(left.cy, right.cy, .01f));
    CHECK(close_to(left.cx, -l / (r - l) * 1800.f, .01f));

    // Degenerate fields of view fall back to the middle.
    const acvr::HoldRingSpot flat = acvr::hold_ring_eye_spot(0.f, 0.f, -1.f, 1.f, 100, 100);
    CHECK(close_to(flat.cx, 50) && close_to(flat.cy, 50));
    const acvr::HoldRingSpot nan = acvr::hold_ring_eye_spot(std::nanf(""), 1.f, -1.f, 1.f, 100, 100);
    CHECK(close_to(nan.cx, 50) && close_to(nan.cy, 50));
    const acvr::HoldRingSpot outside = acvr::hold_ring_eye_spot(.2f, 1.f, -1.f, 1.f, 100, 100);   // straight ahead is off the image
    CHECK(close_to(outside.cx, 50) && close_to(outside.cy, 50));
}

// A host that quits by itself: the resolver's quit edge goes through QuitAfterFlash, the ring follows the progress.
struct Host {
    acvr::StickChord chord;
    acvr::HoldRing ring;
    acvr::QuitAfterFlash quit;
    acvr::HoldRingView view;
    uint64_t now = 0, fired_at = 0, quit_at = 0;
    int quits = 0;
    bool flash_seen = false;
    void run(uint64_t until, bool l, bool r) {
        for (; now <= until; now += 10) {
            const acvr::StickChordState s = chord.step(now, l, r);
            view = ring.update(now, s);
            if (s.quit && !fired_at) fired_at = now;
            if (view.flash > .5f) flash_seen = true;
            if (quit.update(now, s.quit)) { ++quits; quit_at = now; }
        }
    }
};

void quit_after_flash() {
    {   // held: the ring flashes at 3 s and the host quits 700 ms later, once
        Host h;
        h.run(5000, true, true);
        CHECK(h.fired_at >= 3000 && h.fired_at <= 3020);
        CHECK(h.quits == 1);
        CHECK(h.quit_at >= h.fired_at + 700 && h.quit_at <= h.fired_at + 710);
        CHECK(h.flash_seen);
    }
    {   // released right after the quit fired: the quit still goes, after the flash
        Host h;
        h.run(3020, true, true);
        CHECK(h.fired_at != 0 && h.quits == 0 && h.quit.pending());
        h.run(4500, false, false);
        CHECK(h.quits == 1 && !h.quit.pending());
    }
    {   // released before 3 s: no quit at all, the ring gone
        Host h;
        h.run(2500, true, true);
        CHECK(h.view.visible);
        h.run(6000, false, false);
        CHECK(h.quits == 0 && !h.view.visible && !h.quit.pending());
    }
    {   // one click lets go at 2 s: the ring hides at once and no quit comes
        Host h;
        h.run(2000, true, true);
        CHECK(h.view.visible);
        h.run(2010, true, false);
        CHECK(!h.view.visible);
        h.run(6000, true, false);
        CHECK(h.quits == 0);
    }
    {   // focus lost during the flash: flush quits at once, and only once
        acvr::QuitAfterFlash q;
        CHECK(!q.flush());
        CHECK(!q.update(100, true) && q.pending());
        CHECK(q.flush() && !q.flush() && !q.update(2000, true));
    }
    {   // the clock going backwards never strands a fired quit
        acvr::QuitAfterFlash q;
        CHECK(!q.update(5000, true));
        CHECK(q.update(10, false));
    }
    {   // reset (a new session) arms it again
        acvr::QuitAfterFlash q(100);
        CHECK(!q.update(0, true) && q.update(100, false));
        q.reset();
        CHECK(!q.update(500, false) && !q.pending());
        CHECK(!q.update(600, true) && q.update(700, false));
    }
}

void both_held() {
    acvr::BothHeldProgress p;
    acvr::HoldRing ring;
    uint64_t now = 0;
    float progress = 0;
    acvr::HoldRingView v;
    bool shown_before_1s = false;
    // Left alone first, the right joins 500 ms later (no chord window: DolphinXR's hold(left & right, 3.0)).
    for (; now < 500; now += 10) { progress = p.update(now, false); v = ring.update(now, progress); }
    for (; now <= 500 + 3000; now += 10) {
        progress = p.update(now, true);
        v = ring.update(now, progress);
        if (now < 1500 && v.visible) shown_before_1s = true;
        if (now == 2500) CHECK(close_to(v.fill, .5f, .01f));
    }
    CHECK(!shown_before_1s);
    CHECK(progress == 1.f && v.quit && v.flash > .99f);
    // released: progress drops to zero at once, the flash plays out and then the ring is gone
    progress = p.update(now, false);
    CHECK(progress == 0.f);
    v = ring.update(now, progress);
    CHECK(v.visible && v.flash > 0.f);
    v = ring.update(now + 800, 0.f);
    CHECK(!v.visible);
    // a fresh hold counts from zero again
    CHECK(p.update(10000, true) == 0.f && close_to(p.update(11500, true), .5f, .001f));
    // the clock going backwards restarts the count instead of jumping to full
    CHECK(p.update(100, true) == 0.f);
}
}  // namespace

int main() {
    spots();
    quit_after_flash();
    both_held();
    if (failed) {
        std::fprintf(stderr, "%d of %d checks failed\n", failed, checks);
        return 1;
    }
    std::printf("PASS hold ring host: spots, quit after the flash, both-held progress (%d checks)\n", checks);
    return 0;
}
