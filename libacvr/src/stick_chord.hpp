// SPDX-License-Identifier: MIT
#pragma once
#include <algorithm>
#include <cstdint>
namespace acvr {
// Universal stick-click layer (controls contract v1, U1-U4). Header-only and
// dependency-free so the runtime, the theatre presenter and host patches share
// one resolver. Feed it the RAW left and right stick-click levels once per
// display frame with a monotonic millisecond clock; use only its outputs.
//
//   left click alone            -> start   (U1)  held for as long as the click is held
//   right click alone           -> select  (U2)  coin / Minus / Back
//   both clicks, close together -> chord: no start or select at all
//     released within recenter_ms        -> recenter pulse (U3)
//     still held at quit_ms              -> quit pulse     (U4), and no recenter
//
// A click is held back for window_ms to see whether the other click follows.
// A tap shorter than window_ms still produces a start/select of min_pulse_ms.
struct StickChordTiming {
    uint32_t window_ms=150, recenter_ms=1000, quit_ms=3000, min_pulse_ms=50;
};
struct StickChordState {
    bool start=false, select=false;     // resolved levels (U1, U2)
    bool chord=false;                   // both clicks held as one chord
    bool recenter=false, quit=false;    // one-step pulses (U3, U4)
    float quit_progress=0;              // chord held time / quit_ms, 0..1
    bool quit_visible=false;            // chord outlived recenter_ms: show a ring or countdown
    bool quit_cancelled=false;          // cancel_quit() was called for this hold: the quit will not fire, show no ring
    uint32_t chord_ms=0;
};
class StickChord {
public:
    explicit StickChord(StickChordTiming timing={}) : t(timing) {}
    StickChordState step(uint64_t now,bool left,bool right) {
        now=std::max(now,last);last=now;
        const bool cur[2]{left,right};
        StickChordState out;
        for(int i=0;i<2;++i) if(cur[i]&&!prev[i]) {down_at[i]=now;decided[i]=false;}
        if(!chord_active) {
            if(cur[0]&&cur[1]&&!decided[0]&&!decided[1]&&
               (down_at[0]>down_at[1]?down_at[0]-down_at[1]:down_at[1]-down_at[0])<=t.window_ms) {
                chord_active=true;chord_start=std::max(down_at[0],down_at[1]);
                quit_fired=false;
                decided[0]=decided[1]=true;
                solo[0]=solo[1]=false;pulse_until[0]=pulse_until[1]=0;
            } else for(int i=0;i<2;++i) {
                if(cur[i]&&!decided[i]&&now-down_at[i]>=t.window_ms) {decided[i]=true;solo[i]=true;}
                // Released before the window closed with no chord: a tap.
                if(!cur[i]&&prev[i]&&!decided[i]) {decided[i]=true;solo[i]=false;pulse_until[i]=now+t.min_pulse_ms;}
                if(!cur[i]) solo[i]=false;
            }
        }
        if(chord_active) {
            const uint64_t held=now-chord_start;
            out.chord=true;out.chord_ms=held>UINT32_MAX?UINT32_MAX:uint32_t(held);
            out.quit_progress=t.quit_ms?std::min(1.f,float(held)/float(t.quit_ms)):1.f;
            out.quit_visible=held>=t.recenter_ms;
            out.quit_cancelled=quit_cancelled;
            if(!quit_fired&&!quit_cancelled&&held>=t.quit_ms) {out.quit=true;quit_fired=true;}
            if(!cur[0]&&!cur[1]) {
                // Both are up: this step is the release.
                if(!quit_fired&&held<=t.recenter_ms) out.recenter=true;
                chord_active=false;quit_cancelled=false;out.chord=false;out.quit_visible=false;out.quit_cancelled=false;out.quit_progress=0;out.chord_ms=0;
            }
        } else {
            out.start=(solo[0]&&cur[0])||now<pulse_until[0];
            out.select=(solo[1]&&cur[1])||now<pulse_until[1];
        }
        for(int i=0;i<2;++i) prev[i]=cur[i];
        return out;
    }
    // A host that lets the hands do something else while the chord is held
    // (pinball's table adjust) calls this when the hands move: no quit this hold.
    void cancel_quit() {quit_cancelled=true;}
    void reset() {*this=StickChord(t);}
private:
    StickChordTiming t;
    bool prev[2]{},decided[2]{},solo[2]{};
    uint64_t down_at[2]{},pulse_until[2]{},last=0,chord_start=0;
    bool chord_active=false,quit_fired=false,quit_cancelled=false;
};
}
