// SPDX-License-Identifier: GPL-3.0-or-later
// CPU-only checks of the universal stick-click resolver (StickChord.h): U1 Start, U2 Select, U3 recentre, U4 quit.
//   g++ -std=c++17 -I host host/StickChordTest.cpp -o StickChordTest && StickChordTest
#include "StickChord.h"
#include <algorithm>
#include <cstdio>
#include <string>
#include <vector>

namespace {
int g_failed = 0;
void Check(bool ok, const std::string &what) { if (!ok) { ++g_failed; std::printf("FAIL: %s\n", what.c_str()); } }

// Drives the resolver on a 10 ms step. `events` are (time, left, right) level changes; runs until `end` ms.
struct Event { std::uint64_t at; bool left, right; };
struct Trace {
  std::uint64_t startFrom = 0, startTo = 0, selectFrom = 0, selectTo = 0;   // first and last time each level was true
  unsigned startMs = 0, selectMs = 0, recentres = 0, quits = 0;
  std::uint64_t recentreAt = 0, quitAt = 0; float maxProgress = 0;
};
Trace Run(const std::vector<Event> &events, std::uint64_t end)
{
  chord::Resolver r; Trace t; bool left = false, right = false; std::size_t next = 0;
  for (std::uint64_t now = 0; now <= end; now += 10) {
    while (next < events.size() && events[next].at <= now) { left = events[next].left; right = events[next].right; ++next; }
    const auto o = r.update(now, left, right);
    if (o.start) { if (!t.startMs) t.startFrom = now; t.startTo = now; t.startMs += 10; }
    if (o.select) { if (!t.selectMs) t.selectFrom = now; t.selectTo = now; t.selectMs += 10; }
    if (o.recentre) { ++t.recentres; t.recentreAt = now; }
    if (o.quit) { ++t.quits; t.quitAt = now; }
    t.maxProgress = std::max(t.maxProgress, o.quitProgress);
  }
  return t;
}
}  // namespace

int main()
{
  // U1: a left click alone is Start, sent after the 150 ms wait and held as long as the click is, then released.
  { const auto t = Run({ { 100, true, false }, { 900, false, false } }, 2000);
    Check(t.startFrom == 250 && t.startTo == 890, "U1: Start rises 150 ms after the click and follows its release");
    Check(!t.selectMs && !t.recentres && !t.quits, "U1: nothing else fires"); }
  // U2: a right click alone is Select.
  { const auto t = Run({ { 100, false, true }, { 700, false, false } }, 2000);
    Check(t.selectFrom == 250 && t.selectTo == 690 && !t.startMs, "U2: Select, not Start"); }
  // A tap shorter than the wait is still sent, for a short fixed time.
  { const auto t = Run({ { 100, true, false }, { 160, false, false } }, 2000);
    Check(t.startMs >= 40 && t.startMs <= 80 && t.startFrom == 250, "a 60 ms tap still reaches the game, 150 ms late, pressed ~50 ms"); }
  // U3: both together, released within a second: recentre once, no Start and no Select at all.
  { const auto t = Run({ { 100, true, false }, { 140, true, true }, { 600, false, false } }, 2000);
    Check(t.recentres == 1 && t.recentreAt == 600, "U3: recentre on release");
    Check(!t.startMs && !t.selectMs && !t.quits, "U3: a chord sends no Start or Select"); }
  // The second click within the 150 ms wait is a chord; at 150 ms it is not (Start has gone out).
  { const auto t = Run({ { 100, true, false }, { 240, true, true }, { 500, false, false } }, 2000);
    Check(t.recentres == 1 && !t.startMs, "second click 140 ms after the first is a chord"); }
  { const auto t = Run({ { 100, true, false }, { 250, true, true }, { 500, false, false } }, 2000);
    Check(!t.recentres && t.startMs > 0 && t.selectMs > 0, "second click at +150 ms is two separate clicks"); }
  // Both pressed in the same frame.
  { const auto t = Run({ { 100, true, true }, { 300, false, false } }, 2000);
    Check(t.recentres == 1 && !t.startMs && !t.selectMs, "both in one frame is a chord"); }
  // The first click released before the second arrives: still a chord (both within the wait).
  { const auto t = Run({ { 100, true, false }, { 130, false, false }, { 180, false, true }, { 400, false, false } }, 2000);
    Check(t.recentres == 1 && !t.startMs && !t.selectMs && !t.quits, "a fast first click, then the other, is a chord"); }
  // Held between 1 s and 3 s: neither U3 nor U4.
  { const auto t = Run({ { 100, true, true }, { 2000, false, false } }, 4000);
    Check(!t.recentres && !t.quits && !t.startMs && !t.selectMs, "held 1.9 s: nothing fires");
    Check(t.maxProgress > .5f && t.maxProgress < 1.f, "the quit progress rises while both are held"); }
  // Exactly the 1 s limit still recentres; just over does not.
  { const auto t = Run({ { 100, true, true }, { 1100, false, false } }, 2000);
    Check(t.recentres == 1, "released 1000 ms after the chord: recentre"); }
  { const auto t = Run({ { 100, true, true }, { 1110, false, false } }, 2000);
    Check(!t.recentres, "released 1010 ms after the chord: no recentre"); }
  // U4: both held for 3 s quits once, and the release afterwards does not recentre.
  { const auto t = Run({ { 100, true, true }, { 4000, false, false } }, 5000);
    Check(t.quits == 1 && t.quitAt == 3100, "U4: quit fires once, 3 s after the chord began");
    Check(!t.recentres && !t.startMs && !t.selectMs, "U4: no recentre, Start or Select"); }
  // One click let go before 3 s: no quit even if the other stays held past 3 s.
  { const auto t = Run({ { 100, true, true }, { 2000, true, false }, { 4000, false, false } }, 5000);
    Check(!t.quits && !t.recentres && !t.startMs && !t.selectMs, "letting one go cancels the quit countdown"); }
  // After a chord both sticks are free again: a later lone click is Start.
  { const auto t = Run({ { 100, true, true }, { 300, false, false }, { 600, true, false }, { 900, false, false } }, 2000);
    Check(t.recentres == 1 && t.startMs > 100, "a lone click after a chord is Start again"); }
  // Start held while Select is clicked later (outside the wait): independent.
  { const auto t = Run({ { 100, true, false }, { 600, true, true }, { 900, false, false } }, 2000);
    Check(t.startMs > 0 && t.selectMs > 0 && !t.recentres && !t.quits, "a click long after the other is not a chord"); }

  std::printf(g_failed ? "%d checks failed\n" : "all checks passed\n", g_failed);
  return g_failed ? 1 : 0;
}
