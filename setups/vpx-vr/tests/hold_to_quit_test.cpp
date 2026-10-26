// SPDX-License-Identifier: GPL-3.0-or-later
// CPU-only check of the VPX hold-to-quit logic (patch/acvr/HoldToQuit.h) on libacvr's chord timing. No headset, no VPX.
// From the repository root:
//   g++ -std=c++17 -Wall -Wextra -Werror -I libacvr/src -I setups/vpx-vr/patch/acvr setups/vpx-vr/tests/hold_to_quit_test.cpp -o hold_to_quit_test
#include "HoldToQuit.h"
#include <cstdio>
#include <cstdlib>

using acvr::HandSample;
using acvr::HoldToQuit;

#define CHECK(cond) do { if (!(cond)) { std::fprintf(stderr, "FAIL line %d: %s\n", __LINE__, #cond); std::exit(1); } } while (0)

namespace {
struct Run {
   HoldToQuit h;
   uint64_t t = 1000;
   unsigned quits = 0, cancels = 0, visibleMs = 0;
   uint64_t quitAt = 0;
   float maxTravel = 0.f;
   HandSample left { true, -0.20f, 1.0f, -0.40f }, right { true, 0.20f, 1.0f, -0.40f };
   // Holds the given clicks for ms, stepping every 10 ms (a 100 Hz frame), moving each hand by (dx, dy, dz) metres over the time.
   void hold(bool l, bool r, unsigned ms, float dx = 0.f, float dy = 0.f, float dz = 0.f, bool tracked = true)
   {
      const unsigned steps = ms / 10;
      for (unsigned i = 0; i < steps; ++i)
      {
         left.x -= dx / steps; right.x += dx / steps;
         left.y += dy / steps; right.y += dy / steps;
         left.z += dz / steps; right.z += dz / steps;
         HandSample a = left, b = right;
         a.valid = b.valid = tracked;
         const auto s = h.step(t, l, r, a, b);
         if (s.quit) { ++quits; quitAt = t; }
         if (s.cancelled_now) ++cancels;
         if (s.visible) visibleMs += 10;
         if (s.travel_m > maxTravel) maxTravel = s.travel_m;
         if (s.cancelled) CHECK(s.progress == 0.f && s.remaining_ms == 0 && !s.visible);
         if (!s.chord) CHECK(!s.cancelled && s.progress == 0.f);
         t += 10;
      }
   }
};
}

int main()
{
   { // both clicks together, hands still: one quit at 3 s, a countdown shown from 1 s
      Run a; const auto t0 = a.t;
      a.hold(true, true, 3500); a.hold(false, false, 200);
      CHECK(a.quits == 1);
      CHECK(a.quitAt >= t0 + 3000 && a.quitAt <= t0 + 3020);
      CHECK(a.cancels == 0);
      CHECK(a.visibleMs >= 2400);
   }
   { // hand jitter under 5 cm (4 cm apart in total, 2 cm per hand) still quits
      Run a;
      a.hold(true, true, 3500, 0.02f, 0.f, 0.f);
      CHECK(a.quits == 1 && a.cancels == 0);
      CHECK(a.maxTravel > 0.015f && a.maxTravel < 0.05f);
   }
   { // hands apart by 12 cm in the first second (6 cm per hand): table adjust, never quits
      Run a;
      a.hold(true, true, 1000, 0.06f, 0.f, 0.f); a.hold(true, true, 3000);
      CHECK(a.quits == 0 && a.cancels == 1);
      a.hold(false, false, 200);
      // The next still hold quits again: the cancel lasts only for the hold it happened in.
      a.hold(true, true, 3200);
      CHECK(a.quits == 1 && a.cancels == 1);
   }
   { // moving both hands up 6 cm (moving the table) cancels too
      Run a;
      a.hold(true, true, 2000, 0.f, 0.06f, 0.f); a.hold(true, true, 2000);
      CHECK(a.quits == 0 && a.cancels == 1);
   }
   { // a move just before the 3 s mark still cancels, in the same step
      Run a;
      a.hold(true, true, 2990); a.hold(true, true, 20, 0.f, 0.f, 0.08f); a.hold(true, true, 500);
      CHECK(a.quits == 0 && a.cancels == 1);
   }
   { // released before 3 s: no quit
      Run a;
      a.hold(true, true, 2500); a.hold(false, false, 300);
      CHECK(a.quits == 0);
   }
   { // second click 300 ms after the first: two single clicks (Start, Coin), not a chord, never a quit
      Run a;
      a.hold(true, false, 300); a.hold(true, true, 4000);
      CHECK(a.quits == 0 && a.visibleMs == 0);
   }
   { // second click within 150 ms: still a chord
      Run a;
      a.hold(false, true, 100); a.hold(true, true, 3500);
      CHECK(a.quits == 1);
   }
   { // hands not tracked: travel cannot be measured, so the hold quits as on any other system
      Run a;
      a.hold(true, true, 3500, 0.2f, 0.f, 0.f, false);
      CHECK(a.quits == 1 && a.cancels == 0);
   }
   { // tracking returns mid-hold: travel is measured from the first tracked sample
      Run a;
      a.hold(true, true, 500, 0.f, 0.f, 0.f, false); a.hold(true, true, 1000, 0.08f, 0.f, 0.f); a.hold(true, true, 2000);
      CHECK(a.quits == 0 && a.cancels == 1);
   }
   { // after a quit, keeping the clicks held does not quit again
      Run a;
      a.hold(true, true, 8000);
      CHECK(a.quits == 1);
   }
   std::puts("hold_to_quit_test: all checks passed");
   return 0;
}
