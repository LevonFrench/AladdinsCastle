// SPDX-License-Identifier: MIT
#include "controls.hpp"
#include <toml++/toml.hpp>
#include <algorithm>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <map>
#include <set>
#include <stdexcept>
#include <tuple>

namespace acvr {
namespace {
void require(bool ok,const char *message) {if(!ok) throw std::runtime_error(message);}
template<class T> T init() {T value{};ACVR_INIT(&value);return value;}
std::string text(const toml::table &t,const char *key) {const auto s=t[key].value<std::string>();require(s.has_value(),"required control text is missing");return *s;}
uint32_t index(const toml::table &t,const char *key) {const auto n=t[key].value<int64_t>();require(n&&*n>=0&&uint64_t(*n)<=UINT32_MAX,"invalid control index");return uint32_t(*n);}
bool identifier(const std::string &s) {
    if(s.empty()||s.front()=='-'||s.back()=='-') return false;
    bool dash=false;for(char c:s) {if(c=='-') {if(dash) return false;dash=true;} else {if(!((c>='a'&&c<='z')||(c>='0'&&c<='9'))) return false;dash=false;}}return true;
}
const std::vector<std::string> button_names{"","gear_low","gear_high","gear_n","gear_r","gear_1","gear_2","gear_3","gear_4","gear_5","gear_6","shift_up","shift_down","view","start","coin","handbrake","view_1","view_2","view_3","view_4"};
}
bool read_control_file(const std::string &path,std::string &out,std::string &error) noexcept {
    try {
        std::ifstream in(std::filesystem::u8path(path),std::ios::binary|std::ios::ate);require(bool(in),"cannot open resolved controls");
        const auto size=in.tellg();require(size>0&&size<=1024*1024,"resolved controls size out of bounds");
        std::string content(size_t(size),'\0');in.seekg(0);require(bool(in.read(content.data(),std::streamsize(content.size()))),"resolved controls read failed");
        out=std::move(content);error.clear();return true;
    } catch(const std::exception &e) {try {error=e.what();}catch(...){}return false;}
}
bool ControlMapper::prepare(std::string input,const std::vector<acvr_control_desc> &declared,uint32_t player,uint32_t actions,std::string &error) {
    try {
        require(!input.empty()&&input.size()<=1024*1024,"resolved controls size out of bounds");
        const auto document=toml::parse(input);require(text(document,"version")=="0.1"&&identifier(text(document,"id"))&&!text(document,"title").empty(),"invalid resolved control header");
        const auto *elements=document["element"].as_array();require(elements&&elements->size()<=256,"invalid control elements");
        ControlMapper candidate;std::set<std::string> ids;
        if(document.contains("gun_model")) {candidate.selected_model=text(document,"gun_model");require(identifier(candidate.selected_model),"invalid selected model");}
        if(document.contains("policy")) {
            const auto *policy=document["policy"].as_table();require(policy,"policy must be table");
            if(policy->contains("p1_hand")) {const auto hand=text(*policy,"p1_hand");require(hand=="left"||hand=="right","invalid primary hand");candidate.selected_hand=hand=="left"?ACVR_HAND_LEFT:ACVR_HAND_RIGHT;}
            if(policy->contains("hand_switch")) {const auto mode=text(*policy,"hand_switch");require(mode=="off"||mode=="trigger","invalid hand-switch policy");candidate.selected_switch=mode=="trigger"?1u:0u;}
        }
        for(const auto &value:*elements) {
            const auto *e=value.as_table();require(e,"control element must be a table");
            Binding b;b.id=text(*e,"id");require(identifier(b.id)&&ids.insert(b.id).second,"duplicate/invalid element ID");
            require(!text(*e,"label").empty()&&!text(*e,"part").empty(),"missing cabinet part/label");const auto node=text(*e,"node");if(!node.empty()) candidate.nodes.push_back(node);
            const auto slot=index(*e,"slot");b.player=index(*e,"player");require(slot<2,"invalid gun slot");
            const auto *in=(*e)["input"].as_table(),*binding=(*e)["binding"].as_table();require(in&&binding,"missing input/binding");
            const auto kind=text(*in,"kind"),semantic=text(*in,"semantic");
            bool supported=slot==0&&b.player==player;
            if(kind=="gun") {require(semantic=="trigger"||semantic=="reload","unknown gun semantic");b.kind=semantic=="trigger"?Kind::Fire:Kind::Reload;}
            else if(kind=="axis") {
                b.kind=Kind::Axis;const std::vector<std::string> names{"steering","accelerator","brake","lean","pedal_speed","stick_x","stick_y","lever","cover_pedal","rear_brake"};
                const auto it=std::find(names.begin(),names.end(),semantic);require(it!=names.end(),"unknown axis semantic");b.semantic=uint32_t(it-names.begin())+1;
                supported=supported&&b.semantic==ACVR_AXIS_COVER_PEDAL;
            }
            else if(kind=="button") {b.kind=Kind::Button;const auto it=std::find(button_names.begin()+1,button_names.end(),semantic);require(it!=button_names.end(),"unknown button semantic");b.semantic=uint32_t(it-button_names.begin());}
            else if(kind=="runtime") {
                b.kind=Kind::Runtime;const std::vector<std::string> names{"laser_toggle","recenter","pause","hand_switch","join"};
                const auto it=std::find(names.begin(),names.end(),semantic);require(it!=names.end(),"unknown runtime semantic");b.semantic=uint32_t(it-names.begin());supported=supported&&((actions&(1u<<b.semantic))!=0);
            } else require(false,"unknown input kind");
            if(b.kind==Kind::Axis||b.kind==Kind::Button) {
                const uint32_t wanted=b.kind==Kind::Axis?ACVR_CONTROL_AXIS:ACVR_CONTROL_BUTTON;
                supported=supported&&std::any_of(declared.begin(),declared.end(),[&](const acvr_control_desc &d){return d.kind==wanted&&d.semantic==b.semantic&&d.player==b.player&&d.minimum<=0&&d.maximum>=1;});
            }
            const auto hand=text(*binding,"hand");require(hand=="slot"||hand=="left"||hand=="right"||hand=="either","invalid binding hand");
            b.hand=hand=="slot"?0u:hand=="right"?1u:hand=="left"?2u:3u;
            const auto mode=text(*binding,"mode");require(mode=="hold"||mode=="press"||mode=="toggle","invalid binding mode");b.toggle=mode=="toggle";b.press=mode=="press";
            if(b.kind==Kind::Axis&&b.press) supported=false; // axis pulse queue is not implemented
            const auto control=text(*binding,"control");
            const std::vector<std::string> names{"trigger","grip","primary","secondary","thumbstick_click","menu_chord","offscreen_trigger"};
            const auto c=std::find(names.begin(),names.end(),control);
            if(c==names.end()) {require(control=="pump"||control=="slide"||control=="flick_up","unknown binding control");supported=false;}
            else b.control=Control(c-names.begin());
            if(binding->contains("threshold")) {const auto n=(*binding)["threshold"].value<double>();require(n&&std::isfinite(*n)&&*n>=0&&*n<=1,"invalid control threshold");b.threshold=float(*n);}
            if(binding->contains("invert")) {const auto n=(*binding)["invert"].value<bool>();require(n.has_value(),"invalid control invert");b.invert=*n;}
            if(b.control==Control::Offscreen) {require(b.kind==Kind::Reload,"offscreen binding must request reload");supported=supported&&mode=="press"&&!b.invert&&b.hand==0;}
            if(!supported) candidate.missing.push_back(b.id);else candidate.bindings.push_back(b);
        }
        std::vector<std::string> unavailable_offscreen;
        for(const auto &b:candidate.bindings) if(b.control==Control::Offscreen&&
            !std::any_of(candidate.bindings.begin(),candidate.bindings.end(),[&](const Binding &fire){return fire.kind==Kind::Fire&&fire.control==Control::Trigger&&fire.hand==0&&!fire.invert&&!fire.toggle&&fire.threshold==b.threshold;})) unavailable_offscreen.push_back(b.id);
        candidate.bindings.erase(std::remove_if(candidate.bindings.begin(),candidate.bindings.end(),[&](const Binding &b){return std::find(unavailable_offscreen.begin(),unavailable_offscreen.end(),b.id)!=unavailable_offscreen.end();}),candidate.bindings.end());
        candidate.missing.insert(candidate.missing.end(),unavailable_offscreen.begin(),unavailable_offscreen.end());
        if(const auto *parts=document["unmapped_part"].as_array()) {
            require(parts->size()<=256,"too many unresolved parts");
            for(const auto &value:*parts) {const auto *part=value.as_table();require(part,"unresolved part must be table");const auto node=text(*part,"node");if(!node.empty()) candidate.nodes.push_back(node);}
        } else require(!document.contains("unmapped_part"),"unresolved parts must be an array");
        if(const auto *outputs=document["output"].as_array()) {
            require(outputs->size()<=64,"too many output routes");std::set<std::string> output_ids;
            for(const auto &value:*outputs) {
                const auto *o=value.as_table();require(o,"output route must be table");const auto id=text(*o,"id");require(identifier(id)&&output_ids.insert(id).second,"invalid output ID");
                GunOutputRoute route;route.slot=index(*o,"slot");require(route.slot<2,"invalid output slot");route.player=index(*o,"player");route.channel=index(*o,"channel");route.motion=text(*o,"motion");
                const auto kind=text(*o,"kind");require(kind=="solenoid"||kind=="lamp"||kind=="ffb","invalid output kind");route.kind=kind=="solenoid"?ACVR_OUTPUT_SOLENOID:kind=="lamp"?ACVR_OUTPUT_LAMP:ACVR_OUTPUT_FFB;
                const auto amplitude=(*o)["amplitude"].value<double>();require(amplitude&&std::isfinite(*amplitude)&&*amplitude>=0&&*amplitude<=1,"invalid output amplitude");route.amplitude=float(*amplitude);
                route.duration_ms=index(*o,"duration_ms");require(route.duration_ms&&route.duration_ms<=10000,"invalid output duration");
                if(route.slot==0&&route.player==player) candidate.output_routes.push_back(std::move(route));
            }
        } else require(!document.contains("output"),"output must be an array");
        candidate.original=std::move(input);*this=std::move(candidate);error.clear();return true;
    } catch(const std::exception &e) {error=e.what();return false;}
}
void ControlMapper::cancel() {for(auto &b:bindings) {b.armed=false;b.previous=false;b.latched=false;}}
bool ControlMapper::can_use_hand(uint32_t primary) const {
    if(primary>ACVR_HAND_LEFT) return false;
    std::map<std::pair<uint32_t,uint32_t>,std::tuple<Kind,uint32_t,uint32_t,float,bool,bool,bool>> occupied;
    for(const auto &b:bindings) for(uint32_t hand=0;hand<2;++hand) {
        if(!(b.hand==3||(b.hand==0&&hand==primary)||(b.hand>0&&b.hand<3&&hand==b.hand-1))) continue;
        const auto signature=std::make_tuple(b.kind,b.semantic,b.player,b.threshold,b.invert,b.toggle,b.press);
        const auto key=std::make_pair(hand,uint32_t(b.control));
        const auto old=occupied.find(key);if(old!=occupied.end()&&old->second!=signature) return false;
        occupied[key]=signature;
    }
    return true;
}
acvr_result ControlMapper::sample(const ControllerButtons (&hands)[2],const bool (&tracked)[2],uint32_t primary,MappedControls &out) {
    if(!can_use_hand(primary)) return ACVR_BAD_ARGUMENT;
    for(const auto &h:hands) if(!std::isfinite(h.trigger)||h.trigger<0||h.trigger>1||!std::isfinite(h.grip)||h.grip<0||h.grip>1) return ACVR_BAD_ARGUMENT;
    MappedControls result;
    for(auto &b:bindings) {
        std::array<bool,2> use{};if(b.hand==0) use[primary]=true;else if(b.hand==3) use={true,true};else use[b.hand-1]=true;
        bool down=false,live=false;
        for(uint32_t h=0;h<2;++h) if(use[h]) {
            if(!tracked[h]) continue;
            live=true;const auto &s=hands[h];bool held=false;
            switch(b.control) {
                case Control::Trigger:case Control::Offscreen:held=s.trigger>=b.threshold;break;
                case Control::Grip:held=s.grip>=b.threshold;break;
                case Control::Primary:held=s.primary;break;case Control::Secondary:held=s.secondary;break;
                case Control::StickClick:held=s.stick_click;break;case Control::MenuChord:held=s.menu_chord;break;
            }
            down=down||held;
        }
        if(b.control==Control::Offscreen) {result.offscreen_reload[0]=live;continue;}
        if(!live) {b.armed=false;b.previous=false;b.latched=false;continue;}
        // "either" is one logical binding: combine tracked hands first, then
        // invert it. An idle second grip must not defeat the pressed grip.
        if(b.invert) down=!down;
        if(!b.armed) {if(!down) b.armed=true;b.previous=down;continue;}
        const bool rising=down&&!b.previous;b.previous=down;
        if(b.toggle&&rising) b.latched=!b.latched;
        const bool value=b.toggle?b.latched:down;
        if(b.kind==Kind::Fire) {if(b.press) result.trigger_press[0]=result.trigger_press[0]||rising;else result.trigger[0]=result.trigger[0]||value;}
        else if(b.kind==Kind::Reload) result.reload[0]=result.reload[0]||value;
        else if(b.kind==Kind::Runtime) {if(rising&&std::find(result.actions.begin(),result.actions.end(),RuntimeAction(b.semantic))==result.actions.end()) result.actions.push_back(RuntimeAction(b.semantic));}
        else if(b.kind==Kind::Axis) {
            const auto it=std::find_if(result.axes.begin(),result.axes.end(),[&](const acvr_axis_input &a){return a.semantic==b.semantic&&a.player==b.player;});
            if(it!=result.axes.end()) it->value=std::max(it->value,value?1.f:0.f);
            else {auto a=init<acvr_axis_input>();a.semantic=b.semantic;a.player=b.player;a.value=value?1.f:0.f;result.axes.push_back(a);}
        } else {
            const auto it=std::find_if(result.buttons.begin(),result.buttons.end(),[&](const acvr_button_input &a){return a.semantic==b.semantic&&a.player==b.player;});
            const uint32_t state=b.press?(rising?ACVR_INPUT_PRESSED:0u):(value?ACVR_INPUT_HELD:0u);
            if(it!=result.buttons.end()) it->state|=state;
            else {auto button=init<acvr_button_input>();button.semantic=b.semantic;button.player=b.player;button.state=state;result.buttons.push_back(button);}
        }
    }
    out=std::move(result);return ACVR_OK;
}
}
