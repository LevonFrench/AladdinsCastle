// license:GPLv3+
// SPDX-License-Identifier: GPL-3.0-or-later
// AladdinsCastle hold-to-quit for Visual Pinball X in a headset (universal controls U4, with the pinball rule U3p).
//
// Hold both stick clicks for 3 s and the table closes the ordinary way (VPX's own Exit Game), so PinMAME
// writes its NVRAM and VPX saves its settings as on any other exit. In pinball the same two clicks, held, also
// let the hands scale, turn and move the table; so if either hand travels more than 5 cm from where it was when
// the chord began, this hold no longer quits. Let go of both clicks and the next hold starts fresh.
//
// The chord timing is libacvr's (stick_chord.hpp, copied beside this file by apply_acvr.py): the second click
// must come within 150 ms of the first, and the quit fires once at 3 s. Pure logic with an explicit clock and
// hand positions, so tests/hold_to_quit_test.cpp checks it without a headset.
#pragma once
#include <cmath>
#include <cstdint>
#include "stick_chord.hpp"

namespace acvr {

// One hand's position in metres, in a space fixed to the room (not to the head).
struct HandSample {
   bool valid = false;
   float x = 0.f, y = 0.f, z = 0.f;
};

struct HoldToQuitStatus {
   bool chord = false;          // both stick clicks are held as one chord
   bool quit = false;           // one step only: quit now
   bool cancelled = false;      // this hold will not quit any more (a hand moved too far)
   bool cancelled_now = false;  // one step only: the hold was cancelled in this step
   bool visible = false;        // the chord has outlived the recentre time and still counts: show the countdown
   float progress = 0.f;        // 0..1 towards the quit (0 once cancelled)
   uint32_t remaining_ms = 0;   // time left before the quit (0 once cancelled)
   float travel_m = 0.f;        // the larger hand travel in this hold, in metres
};

class HoldToQuit {
public:
   static constexpr float kMaxTravelM = 0.05f;

   explicit HoldToQuit(StickChordTiming timing = {}, float max_travel_m = kMaxTravelM)
      : m_chord(timing), m_timing(timing), m_maxTravel(max_travel_m) {}

   HoldToQuitStatus step(uint64_t now_ms, bool left_click, bool right_click, const HandSample& left_hand, const HandSample& right_hand)
   {
      HoldToQuitStatus out;
      const HandSample* hands[2] = { &left_hand, &right_hand };
      // Measure the travel before the chord step, so a move in the same step as the 3 s mark still cancels.
      if (m_active)
      {
         for (int i = 0; i < 2; ++i)
         {
            if (!hands[i]->valid)
               continue;
            if (!m_base[i].valid)
            {
               m_base[i] = *hands[i]; // the first tracked sample of this hold is where the hand started
               continue;
            }
            const float dx = hands[i]->x - m_base[i].x, dy = hands[i]->y - m_base[i].y, dz = hands[i]->z - m_base[i].z;
            const float d = std::sqrt(dx * dx + dy * dy + dz * dz);
            if (d > m_travel)
               m_travel = d;
         }
         if (!m_cancelled && m_travel > m_maxTravel)
         {
            m_chord.cancel_quit();
            m_cancelled = true;
            out.cancelled_now = true;
         }
      }

      const StickChordState s = m_chord.step(now_ms, left_click, right_click);
      out.quit = s.quit;
      if (s.chord && !m_active)
      {
         // A new chord: its start positions are the hands as they are now.
         m_active = true;
         m_cancelled = false;
         m_travel = 0.f;
         m_base[0] = left_hand;
         m_base[1] = right_hand;
      }
      else if (!s.chord)
      {
         m_active = false;
         m_cancelled = false;
         m_travel = 0.f;
         m_base[0] = m_base[1] = HandSample {};
      }

      out.chord = s.chord;
      out.cancelled = s.chord && m_cancelled;
      out.travel_m = m_travel;
      if (s.chord && !m_cancelled)
      {
         out.progress = s.quit_progress;
         out.visible = s.quit_visible;
         out.remaining_ms = s.chord_ms >= m_timing.quit_ms ? 0u : m_timing.quit_ms - s.chord_ms;
      }
      return out;
   }

   void reset()
   {
      m_chord.reset();
      m_active = m_cancelled = false;
      m_travel = 0.f;
      m_base[0] = m_base[1] = HandSample {};
   }

private:
   StickChord m_chord;
   StickChordTiming m_timing;
   float m_maxTravel;
   bool m_active = false, m_cancelled = false;
   float m_travel = 0.f;
   HandSample m_base[2];
};

}
