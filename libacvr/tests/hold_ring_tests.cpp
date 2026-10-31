// SPDX-License-Identifier: MIT
// U4 hold ring: timing, release, recentre and cancel, driven by the real shared chord resolver, plus the pixel
// geometry and the GL drawing against a recording stand-in for OpenGL. No window, no GPU.
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <algorithm>
#include <vector>

// A recording stand-in for the few OpenGL 1.x entry points hold_ring_gl.hpp uses.
using GLint = int; using GLfloat = float; using GLboolean = unsigned char; using GLenum = unsigned; using GLbitfield = unsigned;
constexpr GLenum GL_SCISSOR_TEST = 0x0C11, GL_SCISSOR_BOX = 0x0C10, GL_COLOR_CLEAR_VALUE = 0x0C22, GL_COLOR_WRITEMASK = 0x0C23, GL_FRAMEBUFFER_SRGB = 0x8DB9;
constexpr GLbitfield GL_COLOR_BUFFER_BIT = 0x4000;
namespace fakegl {
struct Clear { int x, y, w, h; float rgb[3]; bool srgb; };
inline std::vector<Clear> clears;
inline bool scissor = false, srgb = true;
inline int box[4]{1, 2, 3, 4};
inline float colour[4]{.5f, .6f, .7f, .8f};
inline unsigned char mask[4]{1, 0, 1, 1};
}  // namespace fakegl
inline GLboolean glIsEnabled(GLenum c) { return c == GL_SCISSOR_TEST ? fakegl::scissor : fakegl::srgb; }
inline void glEnable(GLenum c) { (c == GL_SCISSOR_TEST ? fakegl::scissor : fakegl::srgb) = true; }
inline void glDisable(GLenum c) { (c == GL_SCISSOR_TEST ? fakegl::scissor : fakegl::srgb) = false; }
inline void glGetIntegerv(GLenum, GLint *v) { for (int i = 0; i < 4; ++i) v[i] = fakegl::box[i]; }
inline void glGetFloatv(GLenum, GLfloat *v) { for (int i = 0; i < 4; ++i) v[i] = fakegl::colour[i]; }
inline void glGetBooleanv(GLenum, GLboolean *v) { for (int i = 0; i < 4; ++i) v[i] = fakegl::mask[i]; }
inline void glColorMask(GLboolean a, GLboolean b, GLboolean c, GLboolean d) { fakegl::mask[0] = a; fakegl::mask[1] = b; fakegl::mask[2] = c; fakegl::mask[3] = d; }
inline void glScissor(GLint x, GLint y, int w, int h) { fakegl::box[0] = x; fakegl::box[1] = y; fakegl::box[2] = w; fakegl::box[3] = h; }
inline void glClearColor(GLfloat r, GLfloat g, GLfloat b, GLfloat a) { fakegl::colour[0] = r; fakegl::colour[1] = g; fakegl::colour[2] = b; fakegl::colour[3] = a; }
inline void glClear(GLbitfield bits) {
    if (bits != GL_COLOR_BUFFER_BIT || !fakegl::scissor) std::exit(3);
    fakegl::clears.push_back({fakegl::box[0], fakegl::box[1], fakegl::box[2], fakegl::box[3], {fakegl::colour[0], fakegl::colour[1], fakegl::colour[2]}, fakegl::srgb});
}

#include "hold_ring_gl.hpp"
#include "stick_chord.hpp"

namespace {
int checks = 0, failed = 0;
void check(bool ok, const char *what, int line) {
    ++checks;
    if (!ok) { ++failed; std::fprintf(stderr, "FAILED line %d: %s\n", line, what); }
}
#define CHECK(x) check((x), #x, __LINE__)
bool close_to(float a, float b, float eps = .02f) { return std::fabs(a - b) <= eps; }

// The real resolver feeds the ring, a step every 10 ms like a display frame.
struct Rig {
    acvr::StickChord chord;
    acvr::HoldRing ring;
    acvr::HoldRingView last;
    acvr::StickChordState state;
    bool ever_visible = false, quit_seen = false;
    uint64_t now = 0;
    void run(uint64_t until, bool l, bool r) {
        for (; now <= until; now += 10) {
            state = chord.step(now, l, r);
            last = ring.update(now, state);
            ever_visible = ever_visible || last.visible;
            quit_seen = quit_seen || state.quit;
        }
    }
};

void timing_follows_the_hold() {
    Rig k;
    k.run(900, true, true);
    CHECK(!k.last.visible);                         // nothing for the first second
    k.run(1000, true, true);
    CHECK(k.last.visible && close_to(k.last.fill, 0.f));   // appears at 1 s, empty
    k.run(1500, true, true);
    CHECK(close_to(k.last.fill, .25f, .03f));
    k.run(2000, true, true);
    CHECK(close_to(k.last.fill, .5f, .03f) && !k.last.quit);
    k.run(2500, true, true);
    CHECK(close_to(k.last.fill, .75f, .03f));
    k.run(2990, true, true);
    CHECK(k.last.visible && k.last.fill < 1.f && !k.last.quit && !k.quit_seen);
    k.run(3000, true, true);
    CHECK(k.quit_seen && k.last.quit && close_to(k.last.fill, 1.f) && close_to(k.last.flash, 1.f, .01f));   // full and flashing at the quit
}

void flash_plays_once() {
    Rig k;
    k.run(3000, true, true);
    k.run(3350, true, true);
    CHECK(k.last.visible && k.last.flash > .3f && k.last.flash < .7f);   // half way through the 700 ms flash
    k.run(3800, true, true);
    CHECK(k.last.visible && k.last.quit && k.last.flash == 0.f);          // flash over, ring stays full while held
    // Released at the instant of the quit: the flash still plays out, then the ring goes.
    Rig q;
    q.run(3000, true, true);
    q.run(3100, false, false);
    CHECK(q.last.visible && q.last.flash > .5f);
    q.run(3800, false, false);
    CHECK(!q.last.visible);
    // A second hold flashes again.
    q.run(4000, true, true);
    q.run(7020, true, true);
    CHECK(q.last.visible && q.last.quit);
}

void release_cancels() {
    Rig k;
    k.run(2000, true, true);
    CHECK(k.last.visible);
    k.run(2010, false, false);
    CHECK(!k.last.visible && k.last.fill == 0.f && !k.quit_seen);
    // Pressing again starts from the beginning.
    k.run(2100, true, true);
    k.run(2900, true, true);
    CHECK(!k.last.visible);
    k.run(3300, true, true);
    CHECK(k.last.visible && k.last.fill > .05f && k.last.fill < .25f);
}

void recentre_chord_never_shows_the_ring() {
    for (uint64_t hold : {200u, 500u, 900u, 980u}) {
        Rig k;
        k.run(hold, true, true);
        k.run(hold + 10, false, false);
        CHECK(!k.ever_visible && k.state.recenter);
    }
    // One click down first, the other 100 ms later: still a chord, still no ring inside a second.
    Rig k;
    k.run(90, true, false);
    k.run(900, true, true);
    k.run(910, false, false);
    CHECK(!k.ever_visible);
}

void solo_clicks_never_show_the_ring() {
    Rig k;
    k.run(5000, true, false);
    CHECK(!k.ever_visible);
    k.run(10000, false, true);
    CHECK(!k.ever_visible);
}

void a_cancelled_hold_shows_nothing() {
    // Pinball table adjust: the hands move, the host calls cancel_quit().
    Rig k;
    k.run(1500, true, true);
    CHECK(k.last.visible);
    k.chord.cancel_quit();
    k.run(1510, true, true);
    CHECK(!k.last.visible);
    k.run(4000, true, true);
    CHECK(!k.last.visible && !k.quit_seen);
    k.run(4010, false, false);
    k.run(5000, true, true);
    k.run(6200, true, true);
    CHECK(k.last.visible);   // the next hold is back to normal
}

void progress_input_for_hosts_with_their_own_resolver() {
    acvr::HoldRing ring;
    CHECK(!ring.update(0, 0.f).visible);
    CHECK(!ring.update(10, .32f).visible);
    CHECK(ring.update(20, .34f).visible);
    CHECK(close_to(ring.update(30, .5f).fill, .25f, .01f));
    CHECK(ring.update(40, 1.f).quit);
    CHECK(!ring.update(2000, 0.f).visible);
    // A clock that goes backwards starts again instead of freezing a flash.
    acvr::HoldRing back;
    back.update(5000, 1.f);
    CHECK(!back.update(100, 0.f).visible);
    // Nonsense in is nothing out.
    CHECK(!ring.update(3000, -1.f).visible);
}

void geometry() {
    acvr::HoldRingView v;
    CHECK(acvr::hold_ring_rects(v, 100, 100, 50).n == 0);   // hidden: nothing
    v.visible = true; v.fill = .5f;
    auto r = acvr::hold_ring_rects(v, 100, 100, 50);
    CHECK(r.n == acvr::kHoldRingMaxRects);
    int lit = 0;
    for (int i = acvr::kHoldRingBeads; i < r.n; ++i) lit += r.r[i].rgb[0] > .9f;
    CHECK(lit == acvr::kHoldRingBeads / 2);
    // Every bead sits on the circle, and the first one is straight above the centre (12 o'clock, y up).
    const auto &top = r.r[acvr::kHoldRingBeads];
    CHECK(close_to(top.x + top.w * .5f, 100.f, .6f) && close_to(top.y + top.h * .5f, 150.f, .6f));
    for (int i = acvr::kHoldRingBeads; i < r.n; ++i) {
        const float x = r.r[i].x + r.r[i].w * .5f - 100.f, y = r.r[i].y + r.r[i].h * .5f - 100.f;
        CHECK(close_to(std::sqrt(x * x + y * y), 50.f, .6f));
    }
    // Clockwise: the 4th bead of 12 per quarter is at 3 o'clock (x right of centre).
    const auto &quarter = r.r[acvr::kHoldRingBeads + acvr::kHoldRingBeads / 4];
    CHECK(quarter.x + quarter.w * .5f > 140.f && close_to(quarter.y + quarter.h * .5f, 100.f, .6f));
    // Too small to read: nothing.
    CHECK(acvr::hold_ring_rects(v, 10, 10, 2).n == 0);
    // Green once it has fired, whitened by the flash.
    acvr::HoldRingView fired; fired.visible = fired.quit = true; fired.fill = 1.f;
    auto g = acvr::hold_ring_rects(fired, 100, 100, 50);
    CHECK(g.r[acvr::kHoldRingBeads].rgb[1] > g.r[acvr::kHoldRingBeads].rgb[0]);
    fired.flash = 1.f;
    g = acvr::hold_ring_rects(fired, 100, 100, 50);
    CHECK(g.r[acvr::kHoldRingBeads].rgb[0] > .99f && g.r[acvr::kHoldRingBeads].rgb[2] > .99f);
}

void placement() {
    // Identity clip matrix: the local point (0,0,0) is the middle of the image; the edge point is half a width away.
    const float identity[16]{1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1};
    const float centre[3]{0, 0, 0}, edge[3]{.25f, 0, 0};
    auto p = acvr::place_hold_ring(identity, centre, edge, 1000, 800);
    CHECK(p.ok && close_to(p.cx, 500.f, .01f) && close_to(p.cy, 400.f, .01f) && close_to(p.radius, 125.f, .01f));
    // Behind the eye: not placed.
    float behind[16]{1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, -1, 0, 0, 0, 0};
    CHECK(!acvr::place_hold_ring(behind, centre, edge, 1000, 800).ok);
}

void gl_drawing() {
    using namespace fakegl;
    acvr::HoldRingView v; v.visible = true; v.fill = .5f;
    // A hidden ring touches nothing at all.
    acvr::draw_hold_ring_gl(acvr::HoldRingView{}, 100, 100, 50, 400, 400);
    CHECK(clears.empty());
    scissor = false; srgb = true;
    acvr::draw_hold_ring_gl(v, 200, 200, 80, 400, 400);
    CHECK(int(clears.size()) == acvr::kHoldRingMaxRects);
    bool inside = true, linear = true;
    for (const auto &c : clears) {
        inside = inside && c.x >= 0 && c.y >= 0 && c.x + c.w <= 400 && c.y + c.h <= 400 && c.w > 0 && c.h > 0;
        linear = linear && !c.srgb;   // the sRGB conversion is off while clearing
    }
    CHECK(inside && linear);
    // The caller's state is back as it was.
    CHECK(!scissor && srgb);
    CHECK(box[0] == 1 && box[1] == 2 && box[2] == 3 && box[3] == 4);
    CHECK(close_to(colour[0], .5f, 1e-6f) && close_to(colour[3], .8f, 1e-6f));
    CHECK(mask[0] == 1 && mask[1] == 0 && mask[2] == 1 && mask[3] == 1);
    // Partly off the image: clipped, never negative or past the edge.
    clears.clear();
    acvr::draw_hold_ring_gl(v, 5, 395, 60, 400, 400);
    inside = true;
    for (const auto &c : clears) inside = inside && c.x >= 0 && c.y >= 0 && c.x + c.w <= 400 && c.y + c.h <= 400 && c.w > 0 && c.h > 0;
    CHECK(inside && !clears.empty() && int(clears.size()) < acvr::kHoldRingMaxRects);
    // A caller with the scissor test already on gets it back on.
    scissor = true; srgb = false; clears.clear();
    acvr::draw_hold_ring_gl(v, 200, 200, 80, 400, 400);
    CHECK(scissor && !srgb);
}
}  // namespace

int main() {
    timing_follows_the_hold();
    flash_plays_once();
    release_cancels();
    recentre_chord_never_shows_the_ring();
    solo_clicks_never_show_the_ring();
    a_cancelled_hold_shows_nothing();
    progress_input_for_hosts_with_their_own_resolver();
    geometry();
    placement();
    gl_drawing();
    if (failed) { std::fprintf(stderr, "%d of %d hold ring checks failed\n", failed, checks); return 1; }
    std::printf("%d hold ring checks passed\n", checks);
    return 0;
}
