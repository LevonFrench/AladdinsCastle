// SPDX-License-Identifier: MIT
#include "controls.hpp"
#include "control_fixture.hpp"
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
    const auto source=control_header()+control_row("cover","axis","cover_pedal","grip","toggle")+control_row("pause","runtime","pause","menu_chord","press");
    check(mapper.prepare(source,declarations(),0,acvr::action_bit(acvr::RuntimeAction::Pause),error));
    acvr::ControllerButtons h[2];bool tracked[]{true,true};acvr::MappedControls out;
    h[0].grip=1;h[0].menu_chord=true;check(mapper.sample(h,tracked,0,out)==ACVR_OK&&out.axes[0].value==1&&out.actions.size()==1);
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
int main() {try {mapping_and_rearm();toggle_and_runtime();invalid_and_collisions();inverted_either_grip();}catch(const std::exception &e){std::cerr<<e.what()<<'\n';return 1;}std::cout<<checks<<" control checks passed\n";}
