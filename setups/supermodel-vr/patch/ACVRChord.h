// SPDX-License-Identifier: GPL-2.0-or-later
// ACVR universal controls v1: the stick-click layer (AladdinsCastle).
// Left click = Start (U1), right click = Select/Coin (U2), both clicks released within 1 s = recentre (U3),
// both held for 3 s = quit (U4). Pure logic with an explicit clock, so it can be tested without a headset.
// The same file is copied into the Model 2 viewer, the Supermodel patch and the Flycast patch
// (setups/flycast-hotd2-vr/tools/test_chord_copies.py checks that the copies match).
//
// Chord resolution: a click press waits up to 150 ms for the other stick click.
//   Second click in time: a chord. No Start or Select is sent at all, from either click.
//   Otherwise: Start (or Select) is pressed for as long as the click stays held, then released. A tap shorter than
//   the wait is sent after the wait as a short pulse, so the game still sees it.
// A chord ends when both clicks are up. If that happens within 1 s of the first press, it recentres once.
// Both held together for 3 s quits once (and then does not recentre). Letting go of one click restarts that count.
#pragma once
#include <cstdint>

namespace acvr_chord {

constexpr std::uint64_t window_ms = 150;     // wait for the other click
constexpr std::uint64_t recentre_ms = 1000;  // both released within this long of the first press
constexpr std::uint64_t quit_ms = 3000;      // both held this long
constexpr std::uint64_t tap_ms = 80;         // length of the pulse for a tap shorter than the wait

struct Output {
  bool start = false;       // level: left click alone
  bool select = false;      // level: right click alone
  bool recentre = false;    // one step only
  bool quit = false;        // one step only
  bool inChord = false;     // a chord is in progress: neither click is a Start or Select
  bool bothHeld = false;    // inside a chord with both clicks down right now
  float quitProgress = 0;   // 0 to 1 while both are held
};

class Resolver {
public:
  // now: milliseconds on any monotonic clock. left/right: the stick clicks as read this step.
  Output step(std::uint64_t now, bool left, bool right) {
    if (m_started && now < m_last) reset();   // clock went backwards: start again
    m_started = true; m_last = now;
    Output out;
    const bool down[2] = { left, right };
    if (!m_chord) {
      for (int s = 0; s < 2; ++s) {
        if (!down[s] || m_prev[s]) continue;  // press edge only
        const int o = 1 - s;
        if (m_pending[o] && now - m_pendingSince[o] <= window_ms) {
          m_chord = true; m_chordStart = m_pendingSince[o];
          m_pending[0] = m_pending[1] = false; m_emit[0] = m_emit[1] = false;
          m_bothSince = 0; m_quitFired = false;
        } else {
          m_pending[s] = true; m_pendingSince[s] = now;
        }
      }
    }
    if (m_chord) {
      out.inChord = true;
      if (left && right) {
        if (!m_bothSince) m_bothSince = now ? now : 1;
        const std::uint64_t held = now - m_bothSince;
        out.bothHeld = true;
        out.quitProgress = held >= quit_ms ? 1.0f : float(held) / float(quit_ms);
        if (held >= quit_ms && !m_quitFired) { m_quitFired = true; out.quit = true; }
      } else {
        m_bothSince = 0;
      }
      if (!left && !right) {
        if (!m_quitFired && now - m_chordStart <= recentre_ms) out.recentre = true;
        m_chord = false; out.inChord = false; out.bothHeld = false; out.quitProgress = 0;
      }
    } else {
      for (int s = 0; s < 2; ++s) {
        if (m_pending[s] && now - m_pendingSince[s] >= window_ms) {
          m_pending[s] = false; m_emit[s] = true;
          m_tapUntil[s] = down[s] ? 0 : now + tap_ms;
        }
        if (m_emit[s]) {
          if (down[s] || now < m_tapUntil[s]) (s == 0 ? out.start : out.select) = true;
          else m_emit[s] = false;
        }
      }
    }
    m_prev[0] = left; m_prev[1] = right;
    return out;
  }
  // Focus lost, stale input, session ended: nothing stays pending or held.
  void reset() { *this = Resolver(); }

private:
  bool m_started = false, m_chord = false, m_quitFired = false;
  std::uint64_t m_last = 0, m_chordStart = 0, m_bothSince = 0;
  bool m_prev[2] = { false, false }, m_pending[2] = { false, false }, m_emit[2] = { false, false };
  std::uint64_t m_pendingSince[2] = { 0, 0 }, m_tapUntil[2] = { 0, 0 };
};

}  // namespace acvr_chord
