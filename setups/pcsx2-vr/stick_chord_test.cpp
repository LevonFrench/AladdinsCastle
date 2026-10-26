// SPDX-License-Identifier: GPL-3.0+
// CPU-only check of the universal-controls chord resolver (U1-U4).
// Build and run through stick_chord_test.py, which extracts the header from the patch.
#include "Input/PS2VRStickChord.h"
#include <cassert>
#include <cstdio>
#include <functional>
using namespace std::chrono;
using PS2VRGun::StickChord;
namespace {
struct Run {
 StickChord chord;
 StickChord::Time t0=StickChord::Clock::now();
 int ms=0,start_ms=0,select_ms=0,recentres=0,quits=0;
 // left/right press state as a function of time in ms; steps 10 ms.
 void Play(int until,const std::function<bool(int)>& left,const std::function<bool(int)>& right) {
  for(;ms<=until;ms+=10) {
   const auto out=chord.Update(left(ms),right(ms),t0+milliseconds(ms));
   start_ms+=out.start?10:0;select_ms+=out.select?10:0;recentres+=out.recentre;quits+=out.quit;
  }
 }
 bool StartAt(int at,const std::function<bool(int)>& left,const std::function<bool(int)>& right) {
  StickChord probe; // replay to a point and read the level
  bool level=false;
  for(int m=0;m<=at;m+=10)level=probe.Update(left(m),right(m),t0+milliseconds(m)).start;
  return level;
 }
};
const auto never=[](int){return false;};
auto held(int from,int to){return [=](int m){return m>=from && m<to;};}
}
int main(){
 { // U1: lone left hold = Start only after the 150 ms wait, held while the click is held, then released
  Run r;auto left=held(0,500);
  assert(!r.StartAt(140,left,never));assert(r.StartAt(160,left,never));assert(r.StartAt(490,left,never));assert(!r.StartAt(700,left,never));
  r.Play(1000,left,never);assert(r.recentres==0 && r.quits==0 && r.select_ms==0 && r.start_ms>=300);
 }
 { // U2: lone right click = Select, never Start
  Run r;r.Play(1000,never,held(0,400));assert(r.select_ms>=200 && r.start_ms==0 && r.recentres==0);
 }
 { // a tap shorter than the wait is still a pulse the game can see
  Run r;r.Play(1000,held(0,40),never);assert(r.start_ms>=90 && r.start_ms<=130);
 }
 { // U3: both clicks within 150 ms, released quickly: no Start/Select at all, one recentre
  Run r;r.Play(2000,held(0,400),held(100,450));assert(r.start_ms==0 && r.select_ms==0);assert(r.recentres==1 && r.quits==0);
 }
 { // chord boundary: 150 ms is a chord, 160 ms is two lone clicks
  Run a;a.Play(2000,held(0,500),held(150,550));assert(a.recentres==1 && a.start_ms==0 && a.select_ms==0);
  Run b;b.Play(2000,held(0,500),held(160,550));assert(b.recentres==0 && b.start_ms>0 && b.select_ms>0);
 }
 { // chord starts at the second press: a 1.2 s hold is neither recentre nor quit
  Run r;r.Play(4000,held(0,1500),held(0,1500));assert(r.recentres==0 && r.quits==0 && r.start_ms==0 && r.select_ms==0);
 }
 { // U4: held 3 s fires quit once and never recentres afterwards
  Run r;r.Play(6000,held(0,4500),held(50,4500));assert(r.quits==1 && r.recentres==0 && r.start_ms==0 && r.select_ms==0);
  Run s;s.Play(2900,held(0,5000),held(0,5000));assert(s.quits==0);
 }
 { // letting go of one click cancels the quit; both up inside 1 s still recentres
  Run r;r.Play(6000,held(0,3500),held(0,600));assert(r.quits==0 && r.recentres==0);
  Run s;s.Play(6000,held(0,700),held(0,500));assert(s.quits==0 && s.recentres==1);
 }
 { // after a chord a lone click works again
  Run r;r.Play(3000,[](int m){return m<300||(m>=1000&&m<1600);},[](int m){return m<300;});
  assert(r.recentres==1 && r.start_ms>=300 && r.select_ms==0);
 }
 { // Reset (focus loss) drops everything
  Run r;r.Play(300,held(0,1000),held(0,1000));r.chord.Reset();
  const auto out=r.chord.Update(false,false,r.t0+milliseconds(400));assert(!out.start&&!out.select&&!out.recentre&&!out.quit);
 }
 std::puts("PASS stick chord: U1 Start, U2 Select, U3 recentre, U4 quit, 150 ms window, tap pulse, cancel, reset");
}
