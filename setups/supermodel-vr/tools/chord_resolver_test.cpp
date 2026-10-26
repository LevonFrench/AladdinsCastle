// SPDX-License-Identifier: GPL-2.0-or-later
// CPU-only check of the universal stick-click layer (ACVRChord.h). No headset, no emulator.
//   g++ -std=c++17 -O1 -I../patch chord_resolver_test.cpp -o chord_resolver_test
// The Model 2 viewer and the Flycast patch carry a byte-identical copy of the header; each can build this test
// with its own copy on the include path.
#include "ACVRChord.h"
#include <cstdio>
#include <cstdlib>
#include <vector>

using acvr_chord::Resolver;

#define CHECK(cond) do { if(!(cond)) { std::fprintf(stderr, "FAIL line %d: %s\n", __LINE__, #cond); std::exit(1); } } while(0)

struct Run {
  Resolver r;
  std::uint64_t t = 1000;
  unsigned startMs = 0, selectMs = 0, recentres = 0, quits = 0, chordMs = 0;
  std::uint64_t firstStart = 0, quitAt = 0;
  // Holds the given click state for ms, stepping every 5 ms
  void hold(bool left, bool right, unsigned ms) {
    for (unsigned i = 0; i < ms; i += 5) {
      const auto o = r.step(t, left, right);
      if (o.start) { if (!startMs) firstStart = t; startMs += 5; }
      if (o.select) selectMs += 5;
      if (o.recentre) ++recentres;
      if (o.quit) { ++quits; quitAt = t; }
      if (o.inChord) chordMs += 5;
      t += 5;
    }
  }
};

int main()
{
  { // a long left click: Start after the 150 ms wait, held as long as the click, never Select
    Run a; const auto t0 = a.t;
    a.hold(true, false, 600); a.hold(false, false, 300);
    CHECK(a.firstStart >= t0 + 150 && a.firstStart <= t0 + 160);
    CHECK(a.startMs >= 440 && a.startMs <= 460 && a.selectMs == 0 && a.recentres == 0 && a.quits == 0);
  }
  { // a long right click: Select only
    Run a; a.hold(false, true, 500); a.hold(false, false, 300);
    CHECK(a.selectMs >= 340 && a.startMs == 0);
  }
  { // a 40 ms tap is not lost: it comes out as a pulse after the wait
    Run a; a.hold(true, false, 40); a.hold(false, false, 400);
    CHECK(a.startMs >= 75 && a.startMs <= 90 && a.selectMs == 0);
  }
  { // both clicks within 60 ms, released together: recentre once, no Start or Select at all
    Run a; a.hold(true, false, 60); a.hold(true, true, 300); a.hold(false, false, 400);
    CHECK(a.recentres == 1 && a.startMs == 0 && a.selectMs == 0 && a.quits == 0);
  }
  { // both pressed in the same step
    Run a; a.hold(true, true, 200); a.hold(false, false, 400);
    CHECK(a.recentres == 1 && a.startMs == 0 && a.selectMs == 0);
  }
  { // a quick tap of one and then the other within the wait is still a chord
    Run a; a.hold(true, false, 40); a.hold(false, false, 40); a.hold(false, true, 120); a.hold(false, false, 300);
    CHECK(a.recentres == 1 && a.startMs == 0 && a.selectMs == 0);
  }
  { // letting go of one first, the other a little later (inside 1 s): still one recentre, nothing else
    Run a; a.hold(true, true, 200); a.hold(false, true, 300); a.hold(false, false, 300);
    CHECK(a.recentres == 1 && a.startMs == 0 && a.selectMs == 0);
  }
  { // second click after the wait: two separate single clicks, no chord
    Run a; a.hold(true, false, 400); a.hold(true, true, 500); a.hold(false, false, 300);
    CHECK(a.recentres == 0 && a.startMs >= 700 && a.selectMs >= 300);
  }
  { // chord held past 1 s but released before 3 s: nothing at all
    Run a; a.hold(true, true, 1500); a.hold(false, false, 400);
    CHECK(a.recentres == 0 && a.quits == 0 && a.startMs == 0 && a.selectMs == 0);
  }
  { // chord released over 1 s after the first press, one click at a time: nothing
    Run a; a.hold(true, true, 600); a.hold(false, true, 700); a.hold(false, false, 300);
    CHECK(a.recentres == 0 && a.quits == 0 && a.startMs == 0 && a.selectMs == 0);
  }
  { // both held for 3 s: quit once, at 3 s after both were down, and no recentre on release
    Run a; const auto t0 = a.t;
    a.hold(true, true, 4000); a.hold(false, false, 400);
    CHECK(a.quits == 1 && a.quitAt >= t0 + 3000 && a.quitAt <= t0 + 3010);
    CHECK(a.recentres == 0 && a.startMs == 0 && a.selectMs == 0);
  }
  { // 2.9 s then release: no quit
    Run a; a.hold(true, true, 2900); a.hold(false, false, 400);
    CHECK(a.quits == 0);
  }
  { // letting go of one and pressing it again restarts the 3 s count
    Run a; a.hold(true, true, 2000); a.hold(true, false, 100); a.hold(true, true, 2000); a.hold(false, false, 300);
    CHECK(a.quits == 0 && a.startMs == 0 && a.selectMs == 0);
  }
  { // clock going backwards or reset(): nothing stays held or pending
    Run a; a.hold(true, false, 100); a.r.reset();
    const auto o = a.r.step(a.t, false, false); CHECK(!o.start && !o.select && !o.inChord);
    a.hold(false, false, 400); CHECK(a.startMs == 0);
    Resolver b; b.step(5000, true, false); const auto p = b.step(100, false, false);
    CHECK(!p.start && !p.select);
  }
  { // progress goes 0 to 1 while both are held
    Resolver r; r.step(1000, true, true);
    const auto half = r.step(2500, true, true); CHECK(half.quitProgress > .45f && half.quitProgress < .55f && half.bothHeld);
    const auto done = r.step(4000, true, true); CHECK(done.quit && done.quitProgress == 1.0f);
    const auto after = r.step(4100, true, true); CHECK(!after.quit);
  }
  std::puts("PASS acvr_chord: single clicks, taps, chords, recentre window, 3 s quit, reset");
  return 0;
}
