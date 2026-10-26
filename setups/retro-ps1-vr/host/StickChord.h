// SPDX-License-Identifier: GPL-3.0-or-later
// The universal stick-click layer (universal controls contract U1-U4), the same in every mode and every game this host runs
// (gun, face buttons, pad, wheel, aim, the 3DS):
//   U1  left stick click alone   -> Start   (held for as long as the click is held)
//   U2  right stick click alone  -> Select  (likewise)
//   U3  both clicks together     -> recentre the screen (fires on release, when both were released within 1 s)
//   U4  both clicks held for 3 s -> quit and save
// A click waits up to 150 ms for the other stick's click before it is sent as Start or Select, so a chord never sends a
// stray Start or Select: the price is that a lone click reaches the game 150 ms late. Pure logic, no graphics, no clock of
// its own: the caller gives the time, so a test can drive it with synthetic times and the headset with the real one.
#pragma once
#include <algorithm>
#include <cstdint>

namespace chord {

constexpr std::uint64_t kWaitMs = 150;        // a click waits this long for the other one
constexpr std::uint64_t kRecentreMs = 1000;   // a chord released within this long recentres
constexpr std::uint64_t kQuitMs = 3000;       // both held this long quits
constexpr std::uint64_t kTapMs = 50;          // a lone click shorter than the wait is still sent pressed for this long

struct Output {
  bool start = false, select = false;   // levels: U1 and U2
  bool recentre = false;                // U3: true for one update
  bool quit = false;                    // U4: true for one update
  float quitProgress = 0;               // 0..1 of the 3 s hold while both are held
};

class Resolver {
public:
  // Call once per frame with the time in milliseconds and the two stick-click levels.
  Output update(std::uint64_t now, bool left, bool right)
  {
    Output o;
    const bool down[2] = { left, right };
    if (!m_chord) {
      const bool edge[2] = { down[0] && !m_prev[0], down[1] && !m_prev[1] };
      if (edge[0] && edge[1]) startChord(now, down);
      for (int s = 0; s < 2 && !m_chord; ++s) {
        if (!edge[s]) continue;
        const Side &other = m_side[1 - s];
        if (other.mode == Mode::pending && now - other.since < kWaitMs) startChord(now, down);
        else { m_side[s].mode = Mode::pending; m_side[s].since = now; }
      }
    }
    m_prev[0] = left; m_prev[1] = right;
    if (m_chord) {
      if (!(left && right)) m_quitBroken = true;   // one let go: the 3 s hold is over for this chord
      if (m_quitFired) o.quitProgress = 1;
      else if (!m_quitBroken) {
        const std::uint64_t held = now > m_chordSince ? now - m_chordSince : 0;
        o.quitProgress = std::min(1.f, (float)held / (float)kQuitMs);
        if (held >= kQuitMs) { o.quit = true; m_quitFired = true; }
      }
      if (!left && !right) {
        if (!m_quitFired && (now > m_chordSince ? now - m_chordSince : 0) <= kRecentreMs) o.recentre = true;
        m_chord = false;
      }
      return o;
    }
    bool level[2] = { false, false };
    for (int s = 0; s < 2; ++s) {
      Side &sd = m_side[s];
      if (sd.mode == Mode::pending && now - sd.since >= kWaitMs) { sd.mode = Mode::solo; sd.until = down[s] ? 0 : now + kTapMs; }
      if (sd.mode == Mode::solo) {
        if (down[s] || now < sd.until) level[s] = true; else sd.mode = Mode::idle;
      }
    }
    o.start = level[0]; o.select = level[1];
    return o;
  }

private:
  enum class Mode { idle, pending, solo };
  struct Side { Mode mode = Mode::idle; std::uint64_t since = 0, until = 0; };
  void startChord(std::uint64_t now, const bool down[2])
  {
    m_chord = true; m_chordSince = now; m_quitFired = false; m_quitBroken = !(down[0] && down[1]);
    m_side[0] = Side(); m_side[1] = Side();
  }
  Side m_side[2];
  bool m_prev[2] = { false, false };
  bool m_chord = false, m_quitFired = false, m_quitBroken = false;
  std::uint64_t m_chordSince = 0;
};

}  // namespace chord
