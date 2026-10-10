// SPDX-License-Identifier: GPL-3.0-only
#include "n22_cpu_backend.hpp"
#include "n22_worker.hpp"
#include <algorithm>
#include <iostream>
#include <limits>
#include <stdexcept>
namespace {
int checks=0;
void check(bool b,const char *message) {++checks;if(!b) throw std::runtime_error(message);}
n22::VideoSnapshot snapshot(uint64_t tick=1) {
    n22::VideoSnapshot s;s.tick=tick;s.banks[n22::Mixer].resize(n22::bank_sizes[n22::Mixer]);
    s.banks[n22::Mixer][5]=0x12;s.banks[n22::Mixer][6]=0x34;s.banks[n22::Mixer][7]=0x56;
    s.czattr[4]=0x6464;s.czattr[6]=0xe4;
    for(size_t b=0;b<4;++b) for(size_t i=0;i<256;++i) s.czram[b][b%2?255-i:i]=static_cast<uint16_t>(i*32);
    return s;
}
n22::FogQuad quad(uint64_t tick=1) {
    n22::FogQuad q;q.provided=q.has_native_depth=true;q.tick=tick;q.native_depth={0,256,std::numeric_limits<int32_t>::max()};return q;
}
n22::FogDecision decide(const n22::FogState &s,const n22::FogQuad &q,uint32_t v=0) {
    n22::FogDecision out;check(n22::decide_fog(s,q,v,out)==ACVR_OK,"fog helper decision");return out;
}
void decode_tables() {
    auto source=snapshot();n22::FogState state;
    check(n22::copy_super22_fog(source,state)==ACVR_OK && state.tick==1 && state.policy==n22::FogPolicy::Super22Table,"explicit Super22 snapshot decode");
    check(state.rgb==std::array<uint8_t,3>{0x12,0x34,0x56} && state.attributes==source.czattr,"owned RGB and host-order attributes");
    for(size_t b=0;b<4;++b) {
        bool same=true;for(size_t j=0;j<8192;++j) {
            const int expected=b%2?std::max(0,254-int(j/32)):std::min(255,int(j/32)+1);
            same=same && state.tables[b][j]==expected;
        }
        check(same,"fixed linear/reverse CZ interval expectations across whole bank");
    }
    source.czram[0].fill(32);source.czram[0][0]=16;source.czram[0][1]=16;source.czram[0][2]=8;source.czram[0][3]=32;
    check(n22::copy_super22_fog(source,state)==ACVR_OK,"non-increasing CZ fixture");
    check(std::all_of(state.tables[0].begin(),state.tables[0].begin()+16,[](uint8_t x){return x==0;}) &&
          std::all_of(state.tables[0].begin()+16,state.tables[0].end(),[](uint8_t x){return x==3;}),"equal/descending entries skip prev/extrema update before next rise");
    source.czram[0].fill(0);source.czram[0][1]=0xffff;
    check(n22::copy_super22_fog(source,state)==ACVR_OK && std::all_of(state.tables[0].begin(),state.tables[0].end(),[](uint8_t x){return x==1;}),"CZ values above8192 clamp before interval fill");
    for(auto &bank:source.czram) bank.fill(0);
    check(n22::copy_super22_fog(source,state)==ACVR_OK && state.tables[0][0]==255 && state.tables[0][8191]==255 &&
          state.tables[1][0]==0 && state.tables[1][8191]==0,"all-zero bank extrema behavior follows reverse setting");
    const auto old=state;source.banks[n22::Mixer].pop_back();
    check(n22::copy_super22_fog(source,state)==ACVR_BAD_ARGUMENT && state.tables==old.tables && state.tick==old.tick,"bad mixer length does not replace existing decoded state");
    source=snapshot(0);check(n22::copy_super22_fog(source,state)==ACVR_BAD_ARGUMENT,"unbound snapshot tick rejected");
}
void gates_delta_depth() {
    n22::FogState state;auto source=snapshot();n22::copy_super22_fog(source,state);auto q=quad();
    for(uint8_t type=0;type<4;++type) {q.cz_type=type;const auto out=decide(state,q);check(out.bank==type && out.rgb==state.rgb,"all cz_type bank routes and colour");}
    q.cz_type=0;state.tables[0].fill(10);state.tables[0][1]=20;state.tables[0][8191]=255;
    q.native_depth={255,256,std::numeric_limits<int32_t>::max()};
    check(decide(state,q,0).alpha==245 && decide(state,q,1).alpha==235 && decide(state,q,2).alpha==0,"native integer 255/256 threshold and upper depth clamp");
    q.native_depth[0]=-256;check(decide(state,q).alpha==245,"negative native depth clamps index zero without scene-depth conversion");
    q.colour_word=0x80;check(decide(state,q).alpha==245,"raw colour bit7 does not disable Super22 fog");
    q.colour_word=0x8000;check(decide(state,q).alpha==255 && !decide(state,q).enabled,"raw colour bit15 disables fog");
    q.colour_word=0;q.cz_adjust=0x800000;check(decide(state,q).alpha==255,"cz_adjust bit23 independent disable");q.cz_adjust=0;
    state.attributes[4]&=static_cast<uint16_t>(~4u);check(decide(state,q).alpha==255 && decide(state,q).bank==0,"selected bank enable gate");state.attributes[4]|=4;
    struct Case {uint16_t word;int32_t delta;};
    for(const auto c:std::array<Case,7>{{{0x0080,128},{0x8000,-256},{0x8080,-128},{0x80ff,-1},{0x807f,-129},{0x00ff,255},{0x7f00,0}}}) {
        state.attributes[0]=c.word;check(decide(state,q).delta==c.delta,"word-sign low-byte delta fixed expectations");
    }
    state.attributes[0]=0;state.tables[0][0]=0;check(decide(state,q).alpha==255,"zero fog factor endpoint");
    state.tables[0][0]=1;check(decide(state,q).alpha==254,"factor1 gives alpha254");
    state.tables[0][0]=254;check(decide(state,q).alpha==1,"factor254 gives alpha1");
    state.tables[0][0]=255;check(decide(state,q).alpha==0,"factor255 gives full fog");
    state.attributes[0]=1;check(decide(state,q).alpha==0,"above255 factor saturates");
    state.attributes[0]=0x8000;check(decide(state,q).alpha==255,"nonpositive sum gives unfogged endpoint");
    n22::FogDecision retained;retained.delta=123;q.tick=2;
    check(n22::decide_fog(state,q,0,retained)==ACVR_BAD_ARGUMENT && retained.delta==123,"tick mismatch does not replace decision");q.tick=1;
    check(n22::decide_fog(state,q,3,retained)==ACVR_BAD_ARGUMENT,"vertex index bounded");
    q.has_native_depth=false;check(n22::decide_fog(state,q,0,retained)==ACVR_BAD_ARGUMENT,"table policy requires explicit original native depth");q.has_native_depth=true;
    q.cz_type=4;check(n22::decide_fog(state,q,0,retained)==ACVR_BAD_ARGUMENT,"cz_type bounded");
}
void explicit_policies() {
    n22::FogState absent;check(decide(absent,{}).policy==n22::FogPolicy::Absent && !decide(absent,{}).enabled,"explicit absent state is distinguishable");
    auto q=quad();n22::FogDecision out;
    check(n22::decide_fog(absent,q,0,out)==ACVR_UNSUPPORTED,"supplied metadata cannot invent absent-state policy");
    n22::FogState constant;constant.policy=n22::FogPolicy::System22Constant;constant.tick=1;
    check(n22::decide_fog(constant,q,0,out)==ACVR_UNSUPPORTED,"System22 board constant never guessed from CZ state or depth");
    q.constant_provided=true;q.constant_alpha=37;q.constant_rgb={9,8,7};q.colour_word=0x8000;
    check(decide(constant,q).alpha==37 && decide(constant,q).bank==-1 && decide(constant,q).rgb==q.constant_rgb,"explicit constant seam has no inferred Super22 gate or bank");
    q.constant_alpha=-1;check(decide(constant,q).alpha==255,"explicit generic constant fallback -1");
    q.constant_alpha=0;check(decide(constant,q).alpha==0,"explicit constant full-fog endpoint");
    q.cz_adjust=0x800000;check(decide(constant,q).alpha==255,"generic quad disable also applies to supplied constants");q.cz_adjust=0;
    q.constant_alpha=256;check(n22::decide_fog(constant,q,0,out)==ACVR_BAD_ARGUMENT,"invalid constant alpha rejected");
}
n22::SceneInput fog_scene(const n22::FogState &state) {
    auto in=n22::synthetic_cube();in.fog=state;
    for(auto &p:in.polygons) {p.fog=quad(state.tick);p.fog.native_depth={255,256,8192};}
    return in;
}
void lease_ownership() {
    auto source=snapshot();n22::FogState state;n22::copy_super22_fog(source,state);
    state.tables[0].fill(10);state.tables[0][1]=20;
    auto in=fog_scene(state);in.polygons[0].layer=n22::Layer::Hud;in.hud_depth_scene=7;
    n22::Frame frame;check(n22::prepare(in,1,frame)==ACVR_OK,"scene preparation owns state and native quad depths");
    check(frame.triangles[0].vertices[0].z==-7 && frame.triangles[0].fog.native_depth[0]==255,"HUD scene depth differs from preserved native fog depth");
    const auto old=decide(frame.fog,frame.triangles[1].fog);
    source.czram[0].fill(0);source.banks[n22::Mixer][5]=0;in.fog.tables[0].fill(0);in.polygons[1].fog.native_depth[0]=999999;
    check(decide(frame.fog,frame.triangles[1].fog).alpha==old.alpha && frame.fog.rgb[0]==0x12,"source/input mutation cannot alter owned frame fog");
    auto mismatch=fog_scene(state);mismatch.polygons[0].fog.tick=2;
    check(n22::prepare(mismatch,1,frame)==ACVR_BAD_ARGUMENT && frame.id==1,"mismatched captured tick fails atomically");
    auto baseline=n22::synthetic_cube();auto data=fog_scene(state);n22::Frame plain,with_state;
    for(auto &p:data.polygons) p.fog.colour_word=0x8000; // explicit disabled fog
    check(n22::prepare(baseline,1,plain)==ACVR_OK && n22::prepare(data,1,with_state)==ACVR_OK,"comparison scenes differ only in fog inspection data");
    n22::Image before(128,64),after(128,64);const auto eye=n22::desktop_eye(0,0,4,64,64);
    check(n22::draw_cpu(plain,eye,before)==ACVR_OK && n22::draw_cpu(with_state,eye,after)==ACVR_OK &&
          before.rgb==after.rgb && before.depth==after.depth,"disabled fog preserves existing CPU pixels/depth");
    acvr_backend_api api{};ACVR_INIT(&api);acvr_backend_query(1,&api);
    acvr_open_info open{};ACVR_INIT(&open);ACVR_INIT(&open.graphics);open.game_id_utf8="synthetic-system22";
    acvr_backend_info meta{};ACVR_INIT(&meta);acvr_backend *b=nullptr;api.game_open(&open,&b,&meta);
    in=fog_scene(state);check(n22::stage_cpu_scene(b,in)==ACVR_OK,"backend stages same-tick owned fog metadata");in.fog.tables[0].fill(0);
    acvr_inputs inputs{};ACVR_INIT(&inputs);inputs.tick_id=1;api.game_set_inputs(b,&inputs);
    acvr_step_info step{};ACVR_INIT(&step);step.tick_id=1;acvr_frame_info info{};ACVR_INIT(&info);acvr_frame *f=nullptr;
    check(api.game_step(b,&step,&f,&info)==ACVR_OK,"usual immutable lease publication");
    n22::FogDecision first,replay;
    check(n22::inspect_fog_frame(b,f,0,0,first)==ACVR_OK && first.alpha==245 &&
          n22::inspect_fog_frame(b,f,0,0,replay)==ACVR_OK && replay.alpha==first.alpha,"lease decision replay without snapshot reads/decode");
    check(n22::inspect_fog_frame(b,f,0,1,replay)==ACVR_OK && replay.alpha==235,"second vertex uses its original native depth");
    check(n22::stage_cpu_scene(b,in)==ACVR_BAD_STATE,"active fog lease cannot be replaced");
    check(n22::inspect_fog_frame(b,f,99,0,replay)==ACVR_BAD_ARGUMENT,"leased triangle index bounded");
    api.game_release_frame(b,f);check(n22::inspect_fog_frame(b,nullptr,0,0,replay)==ACVR_BAD_STATE,"no decision after lease release");api.game_close(b);
}
}
int main() {
    try {decode_tables();gates_delta_depth();explicit_policies();lease_ownership();std::cout<<checks<<" owned fog-state checks passed (synthetic CPU only)\n";return 0;}
    catch(const std::exception &e) {std::cerr<<e.what()<<"\n";return 1;}
}
