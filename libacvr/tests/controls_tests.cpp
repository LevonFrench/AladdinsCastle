// SPDX-License-Identifier: MIT
#include "controls.hpp"
#include "control_fixture.hpp"
#include "stick_chord.hpp"
#include <iostream>
#include <stdexcept>
#include <limits>
unsigned checks=0;
void check(bool ok) {++checks;if(!ok) throw std::runtime_error("controls check "+std::to_string(checks));}
std::vector<acvr_control_desc> declarations() {
    std::vector<acvr_control_desc> result(3);for(auto &d:result) {ACVR_INIT(&d);d.maximum=1;}
    result[0].kind=ACVR_CONTROL_GUN;result[1].kind=ACVR_CONTROL_AXIS;result[1].semantic=ACVR_AXIS_COVER_PEDAL;
    result[2].kind=ACVR_CONTROL_BUTTON;result[2].semantic=ACVR_BUTTON_COIN;return result;
}
void mapping_and_rearm() {
    acvr::ControlMapper mapper;std::string error;const auto source=control_fixture()+control_row("start","button","start","primary","press");
    check(mapper.prepare(source,declarations(),0,0,error));check(mapper.source()==source&&mapper.unavailable()==std::vector<std::string>{"start"});
    acvr::ControllerButtons h[2];bool tracked[]{true,true};acvr::MappedControls out;
    h[1].trigger=.7f;h[1].grip=.8f;h[1].secondary=true;h[0].primary=true;
    check(mapper.sample(h,tracked,ACVR_HAND_LEFT,out)==ACVR_OK && out.trigger[0] && out.offscreen_reload[0] && !out.reload[0]);
    check(out.axes.size()==1&&out.axes[0].value==1&&out.buttons.size()==1&&out.buttons[0].semantic==ACVR_BUTTON_COIN&&out.buttons[0].state==ACVR_INPUT_PRESSED);
    check(mapper.sample(h,tracked,ACVR_HAND_LEFT,out)==ACVR_OK&&out.buttons[0].state==0);
    mapper.cancel();check(mapper.sample(h,tracked,ACVR_HAND_LEFT,out)==ACVR_OK && !out.trigger[0] && out.axes.empty()&&out.buttons.empty());
    h[1]={};check(mapper.sample(h,tracked,ACVR_HAND_LEFT,out)==ACVR_OK);
    h[1].trigger=1;check(mapper.sample(h,tracked,ACVR_HAND_LEFT,out)==ACVR_OK&&out.trigger[0]);
    tracked[1]=false;check(mapper.sample(h,tracked,ACVR_HAND_LEFT,out)==ACVR_OK&&!out.trigger[0]);
    tracked[1]=true;check(mapper.sample(h,tracked,ACVR_HAND_LEFT,out)==ACVR_OK&&!out.trigger[0]);
    h[1]={};check(mapper.sample(h,tracked,ACVR_HAND_LEFT,out)==ACVR_OK);
    h[1].trigger=1;check(mapper.sample(h,tracked,ACVR_HAND_LEFT,out)==ACVR_OK&&out.trigger[0]);
    h[1].trigger=std::numeric_limits<float>::quiet_NaN();check(mapper.sample(h,tracked,0,out)==ACVR_BAD_ARGUMENT);
}
void toggle_and_runtime() {
    acvr::ControlMapper mapper;std::string error;
    const auto source=control_header()+control_row("cover","axis","cover_pedal","grip","toggle")+control_row("pause","runtime","pause","thumbstick_click","press");
    check(mapper.prepare(source,declarations(),0,acvr::action_bit(acvr::RuntimeAction::Pause),error));
    acvr::ControllerButtons h[2];bool tracked[]{true,true};acvr::MappedControls out;
    h[0].grip=1;h[0].stick_click=true;check(mapper.sample(h,tracked,0,out)==ACVR_OK&&out.axes[0].value==1&&out.actions.size()==1);
    check(mapper.sample(h,tracked,0,out)==ACVR_OK&&out.axes[0].value==1&&out.actions.empty());
    h[0]={};check(mapper.sample(h,tracked,0,out)==ACVR_OK&&out.axes[0].value==1);
    h[0].grip=1;check(mapper.sample(h,tracked,0,out)==ACVR_OK&&out.axes[0].value==0);
    mapper.cancel();h[0]={};check(mapper.sample(h,tracked,0,out)==ACVR_OK);check(mapper.sample(h,tracked,0,out)==ACVR_OK&&out.axes[0].value==0);
}
void invalid_and_collisions() {
    acvr::ControlMapper mapper;std::string error;check(mapper.prepare(control_fixture(),declarations(),0,0,error));const auto original=mapper.source();
    for(const auto &bad:std::vector<std::string>{"version='0.2'",control_fixture()+control_row("coin","button","coin","primary"),control_header()+control_row("bad","unknown","coin","primary")}) {
        check(!mapper.prepare(bad,declarations(),0,0,error)&&!error.empty()&&mapper.source()==original);
    }
    check(mapper.prepare(control_header()+control_row("reload","gun","reload","offscreen_trigger","press"),declarations(),0,0,error)&&mapper.unavailable()==std::vector<std::string>{"reload"});
    auto threshold=control_fixture();const auto end=threshold.find("mode='hold'}");threshold.insert(end+11,",threshold=0.8");
    check(mapper.prepare(threshold,declarations(),0,0,error)&&mapper.unavailable()==std::vector<std::string>{"reload"});
    check(mapper.prepare(control_fixture()+"\n[policy]\nhand_switch='off'\n",declarations(),0,0,error)&&mapper.hand_switch()==0);
    const auto source=control_fixture()+control_row("duplicate","button","coin","secondary","press");
    check(mapper.prepare(source,declarations(),0,0,error));acvr::ControllerButtons h[2];h[0].secondary=true;bool tracked[]{true,true};acvr::MappedControls out;
    check(mapper.sample(h,tracked,0,out)==ACVR_OK&&out.buttons.size()==1);
    check(mapper.prepare(control_fixture()+control_row("collision","button","coin","trigger"),declarations(),0,0,error));
    check(mapper.sample(h,tracked,0,out)==ACVR_BAD_ARGUMENT);
    check(mapper.prepare(control_header()+control_row("fire","gun","trigger","trigger")+control_row("left-coin","button","coin","trigger","press","left"),declarations(),0,0,error));
    check(mapper.can_use_hand(ACVR_HAND_RIGHT)&&!mapper.can_use_hand(ACVR_HAND_LEFT));
    const auto output=control_fixture()+"\n[[output]]\nid='kick'\nkind='solenoid'\nplayer=0\nslot=0\nchannel=7\nmotion='recoil'\namplitude=0.5\nduration_ms=45\n";
    check(mapper.prepare(output,declarations(),0,0,error)&&mapper.outputs().size()==1&&mapper.outputs()[0].channel==7);
}
void inverted_either_grip() {
    auto source=control_header()+control_row("cover","axis","cover_pedal","grip","hold","either");
    source.insert(source.find("mode='hold'}")+11,",invert=true");
    acvr::ControlMapper mapper;std::string error;check(mapper.prepare(source,declarations(),0,0,error));
    acvr::ControllerButtons h[2];bool tracked[]{true,true};acvr::MappedControls out;
    check(mapper.sample(h,tracked,0,out)==ACVR_OK&&out.axes[0].value==1);
    h[0].grip=1;check(mapper.sample(h,tracked,0,out)==ACVR_OK&&out.axes[0].value==0);
    h[0].grip=0;h[1].grip=1;check(mapper.sample(h,tracked,0,out)==ACVR_OK&&out.axes[0].value==0);
    tracked[0]=tracked[1]=false;check(mapper.sample(h,tracked,0,out)==ACVR_OK&&out.axes.empty());
}
// Universal stick-click layer, contract v1 U1-U4. Times are milliseconds.
struct Chord {
    acvr::StickChord c;uint64_t t=0;
    acvr::StickChordState at(uint64_t now,bool l,bool r) {t=now;return c.step(now,l,r);}
};
void chord_solo_and_tap() {
    Chord k;
    // Start is held back for the 150 ms window, then held for as long as the click is.
    check(!k.at(0,true,false).start&&!k.at(100,true,false).start);
    auto s=k.at(150,true,false);check(s.start&&!s.select&&!s.chord);
    check(k.at(1000,true,false).start);
    check(!k.at(1010,false,false).start);
    // Right click alone is Select (Coin).
    check(!k.at(2000,false,true).select);s=k.at(2150,false,true);check(s.select&&!s.start);
    check(!k.at(2300,false,false).select);
    // A tap shorter than the window still reaches the game as a short press.
    k.at(3000,false,true);s=k.at(3060,false,false);check(s.select&&!s.start);
    check(k.at(3100,false,false).select);check(!k.at(3200,false,false).select);
    // Left Start never leaks Select, and neither leaks anything during a chord.
    for(uint64_t now=4000;now<4100;now+=10) {s=k.at(now,true,true);check(!s.start&&!s.select&&s.chord);}
    check(!k.at(4200,true,true).start&&!k.at(4200,true,true).select);
}
void chord_recenter_and_quit() {
    // U3: both within the window, released within 1 s: one recentre pulse, no Start or Select.
    Chord k;bool leaked=false,pulses=false;unsigned count=0;
    for(uint64_t now=0;now<=400;now+=10) {const auto s=k.at(now,true,now>=100);leaked=leaked||s.start||s.select;count+=s.recenter;}
    check(!leaked&&count==0);
    auto s=k.at(410,false,false);check(s.recenter&&!s.quit&&!s.chord&&!s.start&&!s.select);
    check(!k.at(420,false,false).recenter);
    // Same instant also forms a chord.
    k.at(1000,true,true);check(k.at(1500,true,true).chord);
    // Released exactly at 1 s still recentres; one tick later it does not.
    s=k.at(2000,false,false);check(s.recenter);
    k.at(3000,true,true);s=k.at(4001,false,false);check(!s.recenter&&!s.quit);
    (void)pulses;
    // One click released early, the other held to the end: no stray Start or Select.
    k.at(5000,true,true);s=k.at(5300,true,false);check(!s.start&&!s.select&&s.chord);
    s=k.at(5800,false,false);check(s.recenter&&!s.start&&!s.select);
    // U4: both held 3 s fires quit once; the release afterwards sends no recentre.
    Chord q;unsigned quits=0,rings=0;float last=0;
    for(uint64_t now=0;now<=3500;now+=10) {const auto st=q.at(now,true,true);quits+=st.quit;rings+=st.quit_visible;last=st.quit_progress;check(!st.recenter&&!st.start&&!st.select);}
    check(quits==1&&rings>0&&last==1.f);
    s=q.at(3600,false,false);check(!s.recenter&&!s.quit&&!s.chord);
    // Just under 3 s never quits.
    Chord u;for(uint64_t now=0;now<3000;now+=10) check(!u.at(now,true,true).quit);
    check(!u.at(3000-1,true,true).quit);
    // Pinball adjust: moving the hands cancels the quit countdown for this hold only.
    Chord p;p.at(0,true,true);p.c.cancel_quit();check(!p.at(3200,true,true).quit);p.at(3300,false,false);
    p.at(4000,true,true);check(p.at(7000,true,true).quit);
    // One click let go mid-hold: no quit while the other stays held, and the release still sends no recentre.
    Chord o;o.at(0,true,true);o.at(1500,true,false);check(!o.at(3500,true,false).quit);
    s=o.at(3600,false,false);check(!s.quit&&!s.recenter&&!s.chord);
    // Re-pressing one click while the chord is still held changes nothing.
    Chord r;r.at(0,true,true);r.at(200,false,true);s=r.at(500,true,true);check(!s.start&&!s.select&&s.chord);
    // Second click after the window is two solo presses, never a chord.
    Chord w;w.at(0,true,false);s=w.at(200,true,true);check(s.start&&!s.chord&&!s.select);
    s=w.at(400,true,true);check(s.start&&s.select&&!s.chord);
}
int main() {try {mapping_and_rearm();toggle_and_runtime();invalid_and_collisions();inverted_either_grip();chord_solo_and_tap();chord_recenter_and_quit();}catch(const std::exception &e){std::cerr<<e.what()<<'\n';return 1;}std::cout<<checks<<" control checks passed\n";}
