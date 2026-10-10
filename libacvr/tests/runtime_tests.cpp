// SPDX-License-Identifier: MIT
#include "runtime_host.hpp"
#include <cmath>
#include <cstring>
#include <iostream>
#include <stdexcept>
#include <string>
#include <thread>
#ifdef ACVR_GUN_MODELS
#include "gun_fixture.hpp"
#include "control_fixture.hpp"
#include <filesystem>
#include <fstream>
#endif

template<class T> T init() { T x{}; ACVR_INIT(&x); return x; }
unsigned checks=0;
void check(bool value) { ++checks; if(!value) throw std::runtime_error("check " + std::to_string(checks)); }
struct Receipt {
    bool model_support=false,fail_model=false;
    bool controls_support=false;
    bool pause_support=false;
    unsigned pause_actions=0;
    bool omit_legacy_triggers=false;
    std::vector<std::string> unavailable;
    bool requires_depth=false;
    bool offscreen_reload=false,project_hit=false;
    float hit_x=.5f,hit_y=.5f;
    unsigned model_draws=0;
    std::vector<acvr::Matrix> model_nodes;
    acvr_ray model_ray{};
    std::vector<uint32_t> hand_changes;
    std::vector<acvr::GunOutputRoute> motion_routes;
    std::vector<float> output_levels;
    uint64_t drop_tick=0;
    unsigned opened=0,closed=0,released=0,draws=0,paused=0,cancelled=0,ends=0,submitted=0,outputs=0;
    bool leased=false, fail_right=false, fail_step=false, fail_flush=false, bad_outputs=false;
    uint32_t rate=60,den=1;
    std::vector<acvr_step_info> steps;
    std::vector<acvr_gun_input> guns;
    std::vector<acvr_button_input> buttons;
    std::vector<acvr_axis_input> axes;
    std::vector<acvr_ray> rays;
    std::vector<uint64_t> drawn_frames;
} *current;
struct acvr_frame { uint64_t id=0; };
struct acvr_backend { Receipt *r; acvr_frame frame; uint64_t sequence=0; bool pending=false; };
acvr_control_desc controls[3];
acvr_result ACVR_CALL open(const acvr_open_info *in,acvr_backend **out,acvr_backend_info *info) {
    check(in->graphics.api==0); check(std::strcmp(in->game_id_utf8,"synthetic")==0);
    ++current->opened; *out=new acvr_backend{current,{}};
    info->native_rate_num=current->rate; info->native_rate_den=current->den;
    info->player_count=info->gun_count=1; info->scene_units_per_metre=10;
    info->capabilities=ACVR_CAP_RAYCAST|ACVR_CAP_OUTPUTS|ACVR_CAP_PERSISTENCE;
    if(current->requires_depth) info->capabilities|=ACVR_CAP_REQUIRES_SHARED_DEPTH;
    for(auto &c:controls) c=init<acvr_control_desc>();
    controls[0].kind=ACVR_CONTROL_GUN;
    controls[1].kind=ACVR_CONTROL_BUTTON; controls[1].semantic=ACVR_BUTTON_COIN; controls[1].maximum=1;
    controls[2].kind=ACVR_CONTROL_AXIS; controls[2].semantic=ACVR_AXIS_COVER_PEDAL; controls[2].maximum=1;
    info->controls=controls; info->control_count=3; info->control_stride=sizeof(controls[0]);
    return ACVR_OK;
}
void ACVR_CALL close(acvr_backend *b) { ++b->r->closed; delete b; }
acvr_result ACVR_CALL pause(acvr_backend *b,uint32_t) { ++b->r->paused; return ACVR_OK; }
acvr_result ACVR_CALL inputs(acvr_backend *b,const acvr_inputs *in) {
    check(!b->r->leased); check(in->tick_id==b->r->steps.size()+1);
    check(in->gun_count==1 && in->axis_count==1 && in->button_count==1);
    b->r->guns.push_back(*in->guns); b->r->buttons.push_back(*in->buttons); b->r->axes.push_back(*in->axes);
    return ACVR_OK;
}
acvr_result ACVR_CALL step(acvr_backend *b,const acvr_step_info *in,acvr_frame **out,acvr_frame_info *info) {
    check(!b->r->leased); if(b->r->fail_step) return ACVR_ERROR;
    b->r->steps.push_back(*in); b->r->leased=true; b->frame.id=in->tick_id;
    *out=&b->frame; info->frame_id=in->tick_id; info->camera_count=1; info->primary_camera_id=0;
    b->pending=true; return ACVR_OK;
}
void ACVR_CALL release(acvr_backend *b,acvr_frame *f) { check(f==&b->frame && b->r->leased); b->r->leased=false; ++b->r->released; }
acvr_result ACVR_CALL camera(acvr_backend *b,const acvr_frame *,uint32_t id,acvr_game_camera *c) {
    check(b->r->leased && id==0); c->camera_id=0; c->raster_width=640; c->raster_height=480;
    c->focal_x_px=c->focal_y_px=320; c->centre_x_px=320; c->centre_y_px=240;
    c->viewport_px[2]=640; c->viewport_px[3]=480;
    for(unsigned i=0;i<4;++i) c->view_from_scene[i*5]=1;
    return ACVR_OK;
}
acvr_result ACVR_CALL raycast(acvr_backend *b,const acvr_frame *,const acvr_ray *r,acvr_hit *h) {
    check(b->r->leased); b->r->rays.push_back(*r);
    if(b->r->project_hit) {h->found=1;h->camera_id=0;h->flags=ACVR_HIT_GUN_COORDS;h->screen_x=b->r->hit_x;h->screen_y=b->r->hit_y;return ACVR_OK;}
    h->found=0; h->camera_id=ACVR_NO_CAMERA; return ACVR_OK; // exercise camera fallback
}
acvr_result ACVR_CALL draw(acvr_backend *b,const acvr_frame *f,const acvr_draw_info *d) {
    check(b->r->leased && d->frame_id==f->id); ++b->r->draws; b->r->drawn_frames.push_back(f->id);
    return b->r->fail_right && d->views->eye_index==1 ? ACVR_ERROR : ACVR_OK;
}
acvr_result ACVR_CALL outputs(acvr_backend *b,acvr_outputs *out) {
    if(b->r->bad_outputs) { out->count=0; return ACVR_MORE; }
    out->count=b->pending?1:0;
    if(b->pending) {
        auto &e=out->events[0]; e.sequence=++b->sequence; e.tick_id=b->frame.id;
        e.kind=ACVR_OUTPUT_SOLENOID; e.strength=1; e.duration_ms=45; b->pending=false;
        if(!b->r->output_levels.empty()) {e.strength=b->r->output_levels.at(size_t(b->frame.id-1));e.duration_ms=0;}
    }
    if(b->r->drop_tick&&b->frame.id>=b->r->drop_tick) out->dropped=1;
    return ACVR_OK;
}
acvr_result ACVR_CALL flush(acvr_backend *b) { check(!b->r->leased); return b->r->fail_flush?ACVR_ERROR:ACVR_OK; }
acvr_backend_api api() {
    auto a=init<acvr_backend_api>(); a.abi_version=ACVR_ABI_VERSION;
    a.game_open=open; a.game_close=close; a.game_pause=pause; a.game_set_inputs=inputs; a.game_step=step;
    a.game_release_frame=release; a.game_camera=camera; a.game_raycast=raycast; a.game_draw_eye=draw;
    a.game_poll_outputs=outputs; a.game_flush_persistent=flush; return a;
}
acvr_pose pose() { auto p=init<acvr_pose>(); p.orientation_xyzw[3]=1; return p; }
acvr_tracking tracking() {
    auto t=init<acvr_tracking>(); t.head=pose(); t.head_flags=3;
    t.right=init<acvr_hand_tracking>(); t.left=init<acvr_hand_tracking>();
    t.right.grip=t.right.aim=t.left.grip=t.left.aim=pose();
    t.right.grip_flags=t.right.aim_flags=t.left.grip_flags=t.left.aim_flags=3;
    t.right.hand=ACVR_HAND_RIGHT; t.left.hand=ACVR_HAND_LEFT; return t;
}
struct Sample { int64_t time; bool trigger=false,coin=false,focused=true,render=true,tracked=true; float pedal=0; bool left_trigger=false,reload=false,primary=false; };
struct Recorded final:acvr::Host {
    acvr_pose anchor=pose();
    Receipt &r; std::vector<Sample> samples; size_t index=0; acvr_eye eyes[2];
    Recorded(Receipt &receipt,std::vector<Sample> values):r(receipt),samples(std::move(values)) {
        for(unsigned i=0;i<2;++i) { eyes[i]=init<acvr_eye>(); eyes[i].eye_index=i; }
    }
    acvr_graphics_device device() const override { return init<acvr_graphics_device>(); }
    bool headless() const noexcept override { return true; }
    acvr_result configure(float units,uint32_t mode) override {
        check(units==10 && mode==ACVR_ANCHOR_RAIL); return ACVR_OK;
    }
    acvr_result begin(acvr::Display &d) override {
        if(index==samples.size()) return ACVR_STOPPED;
        auto s=samples[index++]; d.tracking=tracking(); d.tracking.sample_time_ns=d.tracking.predicted_display_time_ns=s.time;
        d.focused=s.focused; d.should_render=s.render; d.trigger[0]=s.trigger; d.scene_from_stage=anchor;
        d.reload[0]=s.reload;
        d.controllers[0].trigger=s.trigger?1.f:0.f;d.controllers[1].trigger=s.left_trigger?1.f:0.f;
        for(auto &controller:d.controllers) {controller.grip=s.pedal;controller.secondary=s.coin;controller.primary=s.primary;}
        if(!s.tracked) d.tracking.right.aim_flags=d.tracking.right.grip_flags=d.tracking.left.grip_flags=0;
        if(r.model_support) {d.tracking.left.grip.position_m[0]=1;d.tracking.left.aim.position_m[0]=9;d.trigger[1]=s.left_trigger;}
        if(r.omit_legacy_triggers) d.trigger[0]=d.trigger[1]=false;
        auto b=init<acvr_button_input>(); b.semantic=ACVR_BUTTON_COIN; b.state=s.coin?ACVR_INPUT_HELD:0; d.buttons.push_back(b);
        auto a=init<acvr_axis_input>(); a.semantic=ACVR_AXIS_COVER_PEDAL; a.value=s.pedal; d.axes.push_back(a);
        for(unsigned i=0;i<2;++i) { d.eyes[i]=init<acvr_draw_info>(); d.eyes[i].target=init<acvr_render_target>();
            d.eyes[i].view_count=1; d.eyes[i].view_stride=sizeof(acvr_eye); d.eyes[i].views=&eyes[i]; }
        return ACVR_OK;
    }
    acvr_result end(bool rendered) override { ++r.ends; if(rendered) ++r.submitted; return ACVR_OK; }
    void output(const acvr_output_event &) override { ++r.outputs; }
    void cancel_effects() noexcept override { ++r.cancelled; }
    bool supports_guns() const noexcept override {return r.model_support;}
    bool supports_controller_samples() const noexcept override {return r.controls_support;}
    uint32_t supported_runtime_actions() const noexcept override {return r.pause_support?acvr::action_bit(acvr::RuntimeAction::Pause):0;}
    acvr_result runtime_action(acvr::RuntimeAction action) override {
        if(action!=acvr::RuntimeAction::Pause||!r.pause_support) return ACVR_UNSUPPORTED;
        ++r.pause_actions;return ACVR_OK;
    }
    void unavailable_controls(const std::vector<std::string> &ids) override {r.unavailable=ids;}
    bool offscreen_reload(uint32_t slot) const noexcept override {return slot==0&&r.offscreen_reload;}
    std::vector<acvr::GunOutputRoute> gun_output_routes() const override {return r.motion_routes;}
    void gun_hand_changed(uint32_t slot,uint32_t hand) override {check(slot==0);r.hand_changes.push_back(hand);}
    acvr_result draw_gun(const acvr_draw_info &info,const acvr::GunDraw &gun) override {
        check(r.drawn_frames.back()==info.frame_id); // world eye was drawn first
        check(gun.asset && gun.slot==0 && gun.lod==0 && gun.body[0]==.5f);
        ++r.model_draws; r.model_nodes=gun.scene_from_node; r.model_ray=gun.muzzle;
        return r.fail_model?ACVR_ERROR:ACVR_OK;
    }
};
acvr_runtime_config config() { auto c=init<acvr_runtime_config>(); c.game_id_utf8="synthetic"; c.max_catchup_ticks=4; c.fallback_aim_distance_m=5; c.anchor_mode=ACVR_ANCHOR_RAIL; return c; }
acvr_runtime *create(Receipt &r,std::vector<Sample> samples,uint32_t budget=4) {
    current=&r; auto c=config(); c.max_catchup_ticks=budget; auto a=api(); acvr_runtime *runtime=nullptr;
    check(acvr::create_with_host(&c,&a,std::make_unique<Recorded>(r,std::move(samples)),&runtime)==ACVR_OK); return runtime;
}
void timing_and_edges() {
    Receipt r;
    auto p=create(r,{{0},{4000000,true,true},{8000000,false,false},{17000000},{34000000},{70000000}});
    for(int i=0;i<6;++i) check(acvr_runtime_tick(p)==ACVR_OK);
    check(r.steps.size()==5 && r.draws==12 && r.outputs==5);
    check(r.steps[1].simulation_time_ns==16666666 && r.steps[3].simulation_time_ns==50000000);
    check(r.guns[0].camera_id==ACVR_NO_CAMERA && (r.guns[0].flags&ACVR_GUN_OFFSCREEN));
    check(r.guns[1].trigger==(ACVR_INPUT_HELD|ACVR_INPUT_PRESSED));
    check(r.guns[2].trigger==ACVR_INPUT_RELEASED && r.guns[3].trigger==0);
    check(r.buttons[1].state==(ACVR_INPUT_HELD|ACVR_INPUT_PRESSED) && r.buttons[2].state==ACVR_INPUT_RELEASED);
    check(r.guns[1].aim_frame_id==1 && r.guns[1].screen_x==.5f && !(r.guns[1].flags&ACVR_GUN_OFFSCREEN));
    check(std::abs(r.rays[0].max_distance_scene-50)<.001f);
    check(r.drawn_frames[0]==r.drawn_frames[1] && r.drawn_frames[0]==r.drawn_frames[4]);
    struct Extended { acvr_tracking t; uint64_t sentinel[2]; } ext{tracking(),{123,456}};
    ext.t.size=sizeof(ext); ext.t.right.aim.size+=8;
    check(acvr_runtime_get_tracking(p,&ext.t)==ACVR_BAD_ARGUMENT);
    ext.t.right.aim.size=sizeof(acvr_pose);
    check(acvr_runtime_get_tracking(p,&ext.t)==ACVR_OK && ext.t.display_id==6 && ext.t.size==sizeof(ext));
    check(ext.sentinel[0]==123 && ext.sentinel[1]==456);
    check(acvr_runtime_tick(p)==ACVR_STOPPED); check(acvr_runtime_destroy(p)==ACVR_OK);
    check(r.closed==1 && r.released==r.steps.size());
}
void rational_and_budget() {
    Receipt r; r.rate=59906; r.den=1000;
    auto p=create(r,{{0},{100000000},{101000000},{400000000}},2);
    for(int i=0;i<4;++i) check(acvr_runtime_tick(p)==ACVR_OK);
    check(r.steps.size()==6); // 1, capped 2, backlog 2, reset one after interruption
    for(size_t i=0;i<r.steps.size();++i) check(r.steps[i].simulation_time_ns==int64_t(i*1000000000000ULL/59906));
    check(acvr_runtime_destroy(p)==ACVR_OK);
}
void pause_loss_and_zero_layers() {
    Receipt r;
    auto p=create(r,{{0},{17000000,true,true,false},{34000000,true,true,true,false},{51000000,false,false,true,false},
                     {68000000,true,true,true,true,false,1},{85000000,true,true},{102000000,false,false},{119000000,true,true}});
    for(int i=0;i<8;++i) check(acvr_runtime_tick(p)==ACVR_OK);
    check(r.ends==8 && r.submitted==5 && r.steps.size()==7);
    check(r.guns[1].trigger==0); // focus resume requires release before rearm
    check(r.guns[3].trigger==0 && r.axes[3].value==0 && !(r.guns[3].flags&ACVR_GUN_TRACKED));
    check(r.guns[4].trigger==0); // tracking recovery does not fire a held trigger
    check(r.guns.back().trigger==(ACVR_INPUT_HELD|ACVR_INPUT_PRESSED));
    check(acvr_runtime_set_paused(p,1)==ACVR_OK); check(acvr_runtime_set_paused(p,2)==ACVR_BAD_ARGUMENT);
    check(acvr_runtime_destroy(p)==ACVR_OK);
}
void failures_and_ownership() {
    {
        Receipt r;r.requires_depth=true;auto p=create(r,{{0}});
        check(acvr_runtime_tick(p)==ACVR_UNSUPPORTED && r.draws==0 && r.submitted==0 && r.ends==1);
        check(acvr_runtime_destroy(p)==ACVR_OK);
    }
    for(int kind=0;kind<4;++kind) {
        Receipt r; r.fail_right=kind==0; r.fail_step=kind==1; r.bad_outputs=kind==2; r.fail_flush=kind==3;
        auto p=create(r,{{0}});
        const auto result=acvr_runtime_tick(p);
        check((kind==3 && result==ACVR_OK) || (kind<3 && result<0));
        check(r.ends==1 && r.submitted==(kind==3?1u:0u));
        acvr_result other=ACVR_OK; std::thread thread([&]{other=acvr_runtime_set_paused(p,1);}); thread.join();
        check(other==ACVR_BAD_STATE);
        check(acvr_runtime_destroy(p)==(kind==3?ACVR_ERROR:ACVR_OK)); check(r.closed==1);
    }
    auto c=config(); auto a=api(); acvr_runtime *p=reinterpret_cast<acvr_runtime *>(uintptr_t(1));
    check(acvr_runtime_create(&c,&a,&p)==ACVR_UNSUPPORTED && p==nullptr);
    Receipt r; current=&r; c.gun_slot_count=1;
    check(acvr::create_with_host(&c,&a,std::make_unique<Recorded>(r,std::vector<Sample>{}),&p)==ACVR_UNSUPPORTED && r.opened==0);
    c=config(); a.game_step=nullptr;
    check(acvr::create_with_host(&c,&a,std::make_unique<Recorded>(r,std::vector<Sample>{}),&p)==ACVR_BAD_ARGUMENT);
    check(acvr_runtime_destroy(nullptr)==ACVR_OK);
}
void muzzle_math() {
    auto grip=pose(),mount=pose(); grip.position_m[0]=1; mount.position_m[2]=-.2f;
    const float identity[4]{0,0,0,1}; auto ray=init<acvr_ray>();
    check(acvr::muzzle_ray(grip,mount,identity,10,5,ray)==ACVR_OK);
    check(ray.origin_scene[0]==10 && std::abs(ray.origin_scene[2]+2)<.0001f && ray.direction_scene[2]==-1);
    const float yaw[4]{0,float(std::sqrt(.5)),0,float(std::sqrt(.5))};
    check(acvr::muzzle_ray(grip,mount,yaw,10,5,ray)==ACVR_OK);
    check(std::abs(ray.origin_scene[0]-8)<.0001f && std::abs(ray.direction_scene[0]+1)<.0001f);
    grip.orientation_xyzw[3]=2; check(acvr::muzzle_ray(grip,mount,identity,10,5,ray)==ACVR_BAD_ARGUMENT);
}
void anchored_aim() {
    auto anchor=pose(),grip=pose(),mount=pose();
    anchor.position_m[0]=2; anchor.position_m[1]=3;
    anchor.orientation_xyzw[1]=anchor.orientation_xyzw[3]=float(std::sqrt(.5));
    grip.position_m[0]=1; mount.position_m[2]=-.2f;
    const float identity[4]{0,0,0,1}; auto ray=init<acvr_ray>();
    check(acvr::anchored_muzzle_ray(anchor,grip,mount,identity,10,5,ray)==ACVR_OK);
    check(std::abs(ray.origin_scene[0]-18)<.001f && ray.origin_scene[1]==30 && std::abs(ray.origin_scene[2]+10)<.001f);
    check(std::abs(ray.direction_scene[0]+1)<.001f && std::abs(ray.direction_scene[2])<.001f && ray.max_distance_scene==50);
    Receipt r; current=&r; auto c=config(); auto a=api(); acvr_runtime *p=nullptr;
    auto host=std::make_unique<Recorded>(r,std::vector<Sample>{{0},{17000000},{34000000}});
    auto *raw=host.get(); host->anchor=anchor;
    check(acvr::create_with_host(&c,&a,std::move(host),&p)==ACVR_OK);
    check(acvr_runtime_tick(p)==ACVR_OK && acvr_runtime_tick(p)==ACVR_OK);
    check(r.rays.size()==1 && r.rays[0].origin_scene[0]==20 && r.rays[0].origin_scene[1]==30);
    check(std::abs(r.rays[0].direction_scene[0]+1)<.001f);
    auto t=tracking(); check(acvr_runtime_get_tracking(p,&t)==ACVR_OK && t.right.aim.position_m[0]==0);
    raw->anchor.orientation_xyzw[3]=2; // invalid anchor must not step or submit
    check(acvr_runtime_tick(p)==ACVR_BAD_ARGUMENT && r.steps.size()==2 && r.submitted==2);
    check(acvr_runtime_destroy(p)==ACVR_OK);
}
void long_replay_and_invalid_samples() {
    Receipt r; std::vector<Sample> frames;
    for(int64_t i=0;i<=900;++i) frames.push_back({i*1000000000LL/90});
    auto p=create(r,frames); auto out=tracking();
    check(acvr_runtime_get_tracking(p,&out)==ACVR_BAD_STATE);
    for(size_t i=0;i<frames.size();++i) check(acvr_runtime_tick(p)==ACVR_OK);
    check(r.steps.size()==601 && r.outputs==601 && r.draws==1802);
    check(r.steps.back().simulation_time_ns==10000000000LL);
    check(acvr_runtime_destroy(p)==ACVR_OK);
    for(const auto &samples:std::vector<std::vector<Sample>>{{{0},{0}},{{0},{-1}},{{0,false,false,true,true,true,2}}}) {
        Receipt bad; p=create(bad,samples);
        for(size_t i=0;i+1<samples.size();++i) check(acvr_runtime_tick(p)==ACVR_OK);
        check(acvr_runtime_tick(p)==ACVR_BAD_ARGUMENT);
        check(acvr_runtime_tick(p)==ACVR_BAD_STATE);
        check(acvr_runtime_destroy(p)==ACVR_OK);
    }
    Receipt lost;
    p=create(lost,{{0},{4000000,true,false,true,true,false},{8000000,true},{17000000,true},{34000000,false},{51000000,true}});
    for(int i=0;i<6;++i) check(acvr_runtime_tick(p)==ACVR_OK);
    check(lost.guns[1].trigger==0 && lost.guns.back().trigger==(ACVR_INPUT_HELD|ACVR_INPUT_PRESSED));
    check(acvr_runtime_destroy(p)==ACVR_OK);
}
void reload_edges() {
    Receipt r;r.offscreen_reload=r.project_hit=true;r.hit_x=1.2f;
    auto p=create(r,{{0},{17000000,true},{21000000,true},{34000000,true},{51000000},{68000000,true}});
    check(acvr_runtime_tick(p)==ACVR_OK && !(r.guns.back().flags&ACVR_GUN_RELOAD));
    check(acvr_runtime_tick(p)==ACVR_OK && (r.guns.back().flags&ACVR_GUN_RELOAD) && r.guns.back().trigger==0);
    check(acvr_runtime_tick(p)==ACVR_OK && r.steps.size()==2); // display replay has no new reload
    r.hit_x=.5f;check(acvr_runtime_tick(p)==ACVR_OK && !(r.guns.back().flags&ACVR_GUN_RELOAD) && r.guns.back().trigger==0); // moving onto screen while held cannot fire
    check(acvr_runtime_tick(p)==ACVR_OK && r.guns.back().trigger==0);
    check(acvr_runtime_tick(p)==ACVR_OK && r.guns.back().trigger==(ACVR_INPUT_HELD|ACVR_INPUT_PRESSED));
    check(acvr_runtime_destroy(p)==ACVR_OK);
    Receipt disabled;disabled.project_hit=true;disabled.hit_x=1.2f;
    p=create(disabled,{{0},{17000000,true}});
    check(acvr_runtime_tick(p)==ACVR_OK && acvr_runtime_tick(p)==ACVR_OK);
    check(!(disabled.guns.back().flags&ACVR_GUN_RELOAD) && disabled.guns.back().trigger==(ACVR_INPUT_HELD|ACVR_INPUT_PRESSED));
    check(acvr_runtime_destroy(p)==ACVR_OK);
    Receipt explicit_reload;
    std::vector<Sample> samples{{0},{4000000},{8000000},{17000000},{34000000},{51000000},{68000000},{85000000}};
    samples[1].reload=true; // short press is retained until next native tick
    samples[4].reload=true;samples[4].tracked=false;samples[5].reload=true;samples[7].reload=true;
    p=create(explicit_reload,samples);
    for(unsigned i=0;i<4;++i) check(acvr_runtime_tick(p)==ACVR_OK);
    check(explicit_reload.guns.size()==2 && (explicit_reload.guns.back().flags&ACVR_GUN_RELOAD));
    check(acvr_runtime_tick(p)==ACVR_OK && !(explicit_reload.guns.back().flags&ACVR_GUN_RELOAD));
    check(acvr_runtime_tick(p)==ACVR_OK && !(explicit_reload.guns.back().flags&ACVR_GUN_RELOAD));
    check(acvr_runtime_tick(p)==ACVR_OK && acvr_runtime_tick(p)==ACVR_OK && (explicit_reload.guns.back().flags&ACVR_GUN_RELOAD));
    check(acvr_runtime_destroy(p)==ACVR_OK);
}
#ifdef ACVR_GUN_MODELS
void mapped_pause_stops_current_frame() {
    const auto path=std::filesystem::current_path()/"synthetic-pause-controls.toml";
    {std::ofstream out(path);out<<control_header()<<control_row("fire","gun","trigger","trigger")
        <<control_row("coin","button","coin","secondary")<<control_row("pause","runtime","pause","primary","press");}
    auto c=config();const auto name=path.u8string();c.merged_controls_path_utf8=name.c_str();auto a=api();acvr_runtime *p=nullptr;
    Receipt r;r.controls_support=r.pause_support=true;current=&r;
    std::vector<Sample> samples{{0},{17000000,true,true},{34000000,true,true},{51000000,true,true},{68000000},{85000000,true,true}};
    samples[1].primary=samples[2].primary=samples[3].primary=true;
    check(acvr::create_with_host(&c,&a,std::make_unique<Recorded>(r,samples),&p)==ACVR_OK);
    check(acvr_runtime_tick(p)==ACVR_OK&&r.steps.size()==1&&r.submitted==1);
    const auto outputs_before=r.outputs,draws_before=r.draws;
    check(acvr_runtime_tick(p)==ACVR_OK&&r.pause_actions==1);
    check(r.steps.size()==1&&r.outputs==outputs_before&&r.draws==draws_before&&r.submitted==1&&r.ends==2);
    check(acvr_runtime_tick(p)==ACVR_OK&&r.steps.size()==1&&r.pause_actions==1&&r.ends==3);
    check(acvr_runtime_set_paused(p,0)==ACVR_OK);
    check(acvr_runtime_tick(p)==ACVR_OK&&r.steps.size()==2&&r.pause_actions==1);
    check(r.guns.back().trigger==0&&r.buttons.back().state==0);
    check(acvr_runtime_tick(p)==ACVR_OK&&acvr_runtime_tick(p)==ACVR_OK);
    check(r.guns.back().trigger==(ACVR_INPUT_HELD|ACVR_INPUT_PRESSED)&&r.buttons.back().state==(ACVR_INPUT_HELD|ACVR_INPUT_PRESSED));
    check(acvr_runtime_destroy(p)==ACVR_OK);std::filesystem::remove(path);
}
void alternate_button_bindings() {
    const auto path=std::filesystem::current_path()/"synthetic-alternate-controls.toml";
    {std::ofstream out(path);out<<control_header()<<control_row("hold-coin","button","coin","secondary")<<control_row("press-coin","button","coin","primary","press");}
    auto c=config();const auto name=path.u8string();c.merged_controls_path_utf8=name.c_str();auto a=api();acvr_runtime *p=nullptr;
    Receipt r;r.controls_support=true;current=&r;
    std::vector<Sample> samples{{0},{17000000},{34000000},{51000000},{68000000},{85000000}};
    samples[1].coin=samples[2].coin=true;samples[1].primary=samples[4].primary=samples[5].primary=true;
    check(acvr::create_with_host(&c,&a,std::make_unique<Recorded>(r,samples),&p)==ACVR_OK);
    check(acvr_runtime_tick(p)==ACVR_OK);
    check(acvr_runtime_tick(p)==ACVR_OK&&r.buttons.back().state==(ACVR_INPUT_HELD|ACVR_INPUT_PRESSED));
    check(acvr_runtime_tick(p)==ACVR_OK&&r.buttons.back().state==ACVR_INPUT_HELD);
    check(acvr_runtime_tick(p)==ACVR_OK&&r.buttons.back().state==ACVR_INPUT_RELEASED);
    check(acvr_runtime_tick(p)==ACVR_OK&&r.buttons.back().state==(ACVR_INPUT_HELD|ACVR_INPUT_PRESSED));
    check(acvr_runtime_tick(p)==ACVR_OK&&r.buttons.back().state==ACVR_INPUT_RELEASED);
    check(acvr_runtime_destroy(p)==ACVR_OK);std::filesystem::remove(path);
}
void configured_controls() {
    const auto dir=std::filesystem::current_path()/"synthetic-runtime-controls";std::filesystem::create_directories(dir);
    const auto path=dir/std::filesystem::u8path("controls-\xc3\xa9.toml");
    {std::ofstream out(path);out<<control_fixture()<<control_row("start","button","start","primary","press")<<"\n[policy]\np1_hand='left'\n";}
    auto c=config();auto name=path.u8string();c.merged_controls_path_utf8=name.c_str();auto a=api();acvr_runtime *p=nullptr;
    Receipt r;r.controls_support=r.project_hit=true;r.hit_x=1.2f;current=&r;
    std::vector<Sample> samples{{0},{17000000},{21000000},{34000000},{51000000},{68000000}};
    for(unsigned i:{1u,2u,3u,5u}) {samples[i].left_trigger=true;samples[i].pedal=1;samples[i].coin=true;}
    check(acvr::create_with_host(&c,&a,std::make_unique<Recorded>(r,samples),&p)==ACVR_OK);
    name.clear();std::filesystem::remove(path); // prepared config is copied, no per-frame file access
    check(r.unavailable==std::vector<std::string>{"start"});
    check(acvr_runtime_tick(p)==ACVR_OK);
    check(acvr_runtime_tick(p)==ACVR_OK && (r.guns.back().flags&ACVR_GUN_RELOAD) && r.guns.back().trigger==0);
    check(r.axes.back().value==1 && r.buttons.back().state==(ACVR_INPUT_HELD|ACVR_INPUT_PRESSED));
    check(acvr_runtime_tick(p)==ACVR_OK && r.steps.size()==2);
    r.hit_x=.5f;check(acvr_runtime_tick(p)==ACVR_OK && r.guns.back().trigger==0 && !(r.guns.back().flags&ACVR_GUN_RELOAD));
    check(r.buttons.back().state==ACVR_INPUT_RELEASED); // press mode releases on next native tick while physical button remains held
    check(acvr_runtime_tick(p)==ACVR_OK && r.axes.back().value==0 && r.buttons.back().state==0);
    check(acvr_runtime_tick(p)==ACVR_OK && r.guns.back().trigger==(ACVR_INPUT_HELD|ACVR_INPUT_PRESSED));
    check(acvr_runtime_destroy(p)==ACVR_OK);
    std::filesystem::remove(dir);
}
void configured_model() {
    const auto dir=std::filesystem::current_path()/"synthetic-runtime-model-tests";
    std::filesystem::create_directories(dir); const auto path=dir/"model.glb",meta=dir/"model.toml";
    {const auto bytes=Fixture{}.bytes();std::ofstream f(path,std::ios::binary);f.write(reinterpret_cast<const char *>(bytes.data()),std::streamsize(bytes.size()));}
    {std::ofstream f(meta);f<<"id='synthetic'\n[motion.recoil]\nnode='body_mesh'\nkind='slide'\naxis=[0,0,1]\nrange=[0,0.015]\ndrive='recoil'\nlod_nodes=['body_mesh','body_mesh_lod1']\n";}
    auto slot=init<acvr_gun_slot_config>(); auto file=path.string(),metadata=meta.string();
    slot.model_id_utf8="synthetic";slot.model_path_utf8=file.c_str();slot.metadata_path_utf8=metadata.c_str();
    slot.grip_node_utf8="grip";slot.muzzle_node_utf8="muzzle";slot.hand=ACVR_HAND_LEFT;slot.show_gun=1;slot.angle_xyzw[3]=1;
    for(unsigned i=0;i<4;++i) {slot.body_rgba[i]=.5f;slot.accent_rgba[i]=1;}
    auto c=config();c.gun_slots=&slot;c.gun_slot_count=1;c.gun_slot_stride=sizeof(slot);
    c.gun_policy=init<acvr_gun_policy>();c.gun_policy.p1_hand=ACVR_HAND_LEFT;
    Receipt r;r.model_support=true;current=&r;auto a=api();acvr_runtime *p=nullptr;
    std::vector<Sample> samples{{0},{17000000,true},{34000000,true,false,true,true,false},{51000000,true},{68000000,false},{85000000,true}};
    for(auto &s:samples) {s.left_trigger=s.trigger;s.trigger=false;}
    check(acvr::create_with_host(&c,&a,std::make_unique<Recorded>(r,samples),&p)==ACVR_OK);
    // Config strings and tint must already be copied; files need not stay open.
    slot.body_rgba[0]=0;file.clear();metadata.clear();std::filesystem::remove(path);std::filesystem::remove(meta);std::filesystem::remove(dir);
    check(acvr_runtime_tick(p)==ACVR_OK && r.model_draws==2);
    check(r.model_ray.origin_scene[0]==10 && std::abs(r.model_ray.origin_scene[2]+2)<.001f);
    auto e=init<acvr_gun_event>();e.sequence=1;e.kind=ACVR_GUN_EVENT_RECOIL;e.node_utf8="body_mesh";e.value=1;e.tick_id=1;
    e.tick_id=2;check(acvr_runtime_gun_event(p,&e)==ACVR_BAD_ARGUMENT);e.tick_id=1;
    check(acvr_runtime_gun_event(p,&e)==ACVR_OK);
    check(acvr_runtime_gun_event(p,&e)==ACVR_BAD_ARGUMENT);
    check(acvr_runtime_tick(p)==ACVR_OK && r.model_draws==4);
    check(r.rays[0].origin_scene[0]==10 && r.rays[0].origin_scene[2]==r.model_ray.origin_scene[2]);
    check(r.model_nodes[3][14]>0 && r.guns.back().trigger==(ACVR_INPUT_HELD|ACVR_INPUT_PRESSED));
    check(acvr_runtime_tick(p)==ACVR_OK && r.model_draws==4 && !(r.guns.back().flags&ACVR_GUN_TRACKED));
    ++e.sequence;check(acvr_runtime_gun_event(p,&e)==ACVR_BAD_STATE);
    check(acvr_runtime_tick(p)==ACVR_OK && r.model_draws==6 && r.guns.back().trigger==0 && r.model_nodes[3][14]==0);
    check(acvr_runtime_tick(p)==ACVR_OK);r.fail_model=true;
    check(acvr_runtime_tick(p)==ACVR_ERROR && r.submitted==5);
    check(acvr_runtime_gun_event(p,&e)==ACVR_BAD_STATE);
    check(acvr_runtime_destroy(p)==ACVR_OK && r.closed==1);
}
void mapped_model_motion_and_handoff() {
    const auto dir=std::filesystem::current_path()/"synthetic-mapped-model";std::filesystem::create_directories(dir);
    const auto path=dir/"model.glb",meta=dir/"model.toml",controls_path=dir/"controls.toml";
    {const auto bytes=Fixture{}.bytes();std::ofstream f(path,std::ios::binary);f.write(reinterpret_cast<const char *>(bytes.data()),std::streamsize(bytes.size()));}
    {std::ofstream f(meta);f<<"id='synthetic'\n[motion.trigger]\nnode='body_mesh'\nkind='slide'\naxis=[0,0,1]\nrange=[0,0.015]\ndrive='trigger'\n";}
    {std::ofstream f(controls_path);f<<control_header()<<control_row("fire","gun","trigger","primary");}
    auto slot=init<acvr_gun_slot_config>();auto file=path.string(),metadata=meta.string(),controls_file=controls_path.string();
    slot.model_id_utf8="synthetic";slot.model_path_utf8=file.c_str();slot.metadata_path_utf8=metadata.c_str();slot.grip_node_utf8="grip";slot.muzzle_node_utf8="muzzle";slot.show_gun=1;slot.angle_xyzw[3]=1;
    for(unsigned i=0;i<4;++i) {slot.body_rgba[i]=.5f;slot.accent_rgba[i]=1;}
    auto c=config();c.gun_slots=&slot;c.gun_slot_count=1;c.gun_slot_stride=sizeof(slot);c.gun_policy=init<acvr_gun_policy>();c.gun_policy.hand_switch=1;c.merged_controls_path_utf8=controls_file.c_str();
    Receipt r;r.model_support=r.controls_support=r.omit_legacy_triggers=true;current=&r;auto a=api();acvr_runtime *p=nullptr;
    std::vector<Sample> samples{{0},{17000000},{34000000},{51000000},{68000000},{85000000},{102000000}};
    samples[1].primary=samples[2].primary=samples[6].primary=true;samples[4].left_trigger=true;
    check(acvr::create_with_host(&c,&a,std::make_unique<Recorded>(r,samples),&p)==ACVR_OK);
    check(acvr_runtime_tick(p)==ACVR_OK && r.model_nodes[3][14]==0);
    check(acvr_runtime_tick(p)==ACVR_OK && std::abs(r.model_nodes[3][14]-.15f)<.001f && (r.guns.back().trigger&ACVR_INPUT_PRESSED));
    check(acvr_runtime_tick(p)==ACVR_OK && acvr_runtime_tick(p)==ACVR_OK && r.model_nodes[3][14]==0);
    check(acvr_runtime_tick(p)==ACVR_OK && r.hand_changes.back()==ACVR_HAND_LEFT && r.guns.back().trigger==0);
    check(acvr_runtime_tick(p)==ACVR_OK && acvr_runtime_tick(p)==ACVR_OK && (r.guns.back().trigger&ACVR_INPUT_PRESSED));
    check(std::abs(r.model_nodes[3][14]-.15f)<.001f && r.model_ray.origin_scene[0]==10);
    check(acvr_runtime_destroy(p)==ACVR_OK);
    // Resolved settings cannot contradict the configured slot, and named parts
    // must actually exist when a model is loaded.
    const auto fire=control_header()+control_row("fire","gun","trigger","primary");
    {std::ofstream f(controls_path);f<<fire<<"\n[policy]\nhand_switch='off'\n";}
    p=nullptr;check(acvr::create_with_host(&c,&a,std::make_unique<Recorded>(r,samples),&p)==ACVR_BAD_ARGUMENT&&!p);
    auto named=fire;named.replace(named.find("node=''"),7,"node='missing_part'");
    {std::ofstream f(controls_path);f<<named;}
    check(acvr::create_with_host(&c,&a,std::make_unique<Recorded>(r,samples),&p)==ACVR_BAD_ARGUMENT&&!p);
    named.replace(named.find("missing_part"),12,"body_mesh");
    {std::ofstream f(controls_path);f<<named;}
    check(acvr::create_with_host(&c,&a,std::make_unique<Recorded>(r,samples),&p)==ACVR_OK&&p);
    check(acvr_runtime_destroy(p)==ACVR_OK);
    std::filesystem::remove(path);std::filesystem::remove(meta);std::filesystem::remove(controls_path);std::filesystem::remove(dir);
}
void handoff_and_recoil() {
    const auto dir=std::filesystem::current_path()/"synthetic-runtime-handoff-tests";
    std::filesystem::create_directories(dir);const auto path=dir/"model.glb",meta=dir/"model.toml";
    {const auto bytes=Fixture{}.bytes();std::ofstream f(path,std::ios::binary);f.write(reinterpret_cast<const char *>(bytes.data()),std::streamsize(bytes.size()));}
    {std::ofstream f(meta);f<<"id='synthetic'\n[motion.recoil]\nnode='body_mesh'\nkind='slide'\naxis=[0,0,1]\nrange=[0,0.015]\ndrive='recoil'\n";}
    auto slot=init<acvr_gun_slot_config>();auto file=path.string(),metadata=meta.string();
    slot.model_id_utf8="synthetic";slot.model_path_utf8=file.c_str();slot.metadata_path_utf8=metadata.c_str();
    slot.grip_node_utf8="grip";slot.muzzle_node_utf8="muzzle";slot.show_gun=1;slot.angle_xyzw[3]=1;
    for(unsigned i=0;i<4;++i) {slot.body_rgba[i]=.5f;slot.accent_rgba[i]=1;}
    auto c=config();c.gun_slots=&slot;c.gun_slot_count=1;c.gun_slot_stride=sizeof(slot);c.gun_policy=init<acvr_gun_policy>();c.gun_policy.hand_switch=1;
    std::vector<Sample> samples{{0},{17000000,true},{21000000,true},{34000000,true},{51000000},{68000000},{85000000},
        {102000000,true,false,false},{119000000,true},{136000000},{153000000,true}};
    samples[3].left_trigger=samples[4].left_trigger=samples[6].left_trigger=true;
    samples[7].left_trigger=samples[8].left_trigger=true;
    Receipt r;r.model_support=true;current=&r;auto a=api();acvr_runtime *p=nullptr;
    check(acvr::create_with_host(&c,&a,std::make_unique<Recorded>(r,samples),&p)==ACVR_OK);
    check(acvr_runtime_tick(p)==ACVR_OK && r.hand_changes==std::vector<uint32_t>{ACVR_HAND_RIGHT});
    check(acvr_runtime_tick(p)==ACVR_OK && std::abs(r.model_nodes[3][14]-.15f)<.001f);
    check(acvr_runtime_tick(p)==ACVR_OK && r.steps.size()==2 && r.model_nodes[3][14]<.15f && r.model_nodes[3][14]>.13f);
    check(acvr_runtime_tick(p)==ACVR_OK && r.hand_changes==std::vector<uint32_t>({ACVR_HAND_RIGHT,ACVR_HAND_LEFT}));
    check(r.guns.back().trigger==ACVR_INPUT_RELEASED && r.model_ray.origin_scene[0]==10 && r.model_nodes[3][14]==0);
    check(acvr_runtime_tick(p)==ACVR_OK && r.guns.back().trigger==0);
    check(acvr_runtime_tick(p)==ACVR_OK && r.guns.back().trigger==0);
    check(acvr_runtime_tick(p)==ACVR_OK && r.guns.back().trigger==(ACVR_INPUT_HELD|ACVR_INPUT_PRESSED));
    check(std::abs(r.model_nodes[3][14]-.15f)<.001f && r.model_draws==14);
    check(acvr_runtime_tick(p)==ACVR_OK && acvr_runtime_tick(p)==ACVR_OK);
    check(r.hand_changes.size()==2 && r.guns.back().trigger==0); // held other trigger on focus recovery must not switch/fire
    check(acvr_runtime_tick(p)==ACVR_OK && acvr_runtime_tick(p)==ACVR_OK);
    check(r.hand_changes.size()==3 && r.hand_changes.back()==ACVR_HAND_RIGHT && r.guns.back().trigger==0);
    check(acvr_runtime_destroy(p)==ACVR_OK);
    std::filesystem::remove(path);std::filesystem::remove(meta);std::filesystem::remove(dir);
}
void runtime_output_routes() {
    const auto dir=std::filesystem::current_path()/"synthetic-output-route-tests";
    std::filesystem::create_directories(dir);const auto path=dir/"model.glb",meta=dir/"model.toml";
    {const auto bytes=Fixture{}.bytes();std::ofstream f(path,std::ios::binary);f.write(reinterpret_cast<const char *>(bytes.data()),std::streamsize(bytes.size()));}
    {std::ofstream f(meta);f<<"id='synthetic'\n[motion.slide-output]\nnode='body_mesh'\nkind='slide'\naxis=[0,0,1]\nrange=[0,0.015]\ndrive='recoil'\n";}
    auto slot=init<acvr_gun_slot_config>();auto file=path.string(),metadata=meta.string();
    slot.model_id_utf8="synthetic";slot.model_path_utf8=file.c_str();slot.metadata_path_utf8=metadata.c_str();
    slot.grip_node_utf8="grip";slot.muzzle_node_utf8="muzzle";slot.show_gun=1;slot.angle_xyzw[3]=1;
    for(unsigned i=0;i<4;++i) {slot.body_rgba[i]=.5f;slot.accent_rgba[i]=1;}
    auto c=config();c.gun_slots=&slot;c.gun_slot_count=1;c.gun_slot_stride=sizeof(slot);c.gun_policy=init<acvr_gun_policy>();
    Receipt r;r.model_support=true;r.output_levels={0,.4f,.4f,0,.4f,.4f,.4f,0,.4f,.4f,0,.4f};r.drop_tick=10;
    acvr::GunOutputRoute route;route.kind=ACVR_OUTPUT_SOLENOID;route.motion="slide-output";route.amplitude=.5f;route.duration_ms=100;r.motion_routes={route};
    current=&r;auto a=api();acvr_runtime *p=nullptr;
    std::vector<Sample> samples{{0},{17000000,true},{21000000,true},{34000000,true},{51000000},{68000000,true},
        {85000000,true,false,true,true,false},{102000000,true},{119000000},{136000000,true},{153000000,true},{170000000},{187000000,true}};
    check(acvr::create_with_host(&c,&a,std::make_unique<Recorded>(r,samples),&p)==ACVR_OK);
    r.motion_routes[0].amplitude=1; // runtime retained its own prepared route copy
    check(acvr_runtime_tick(p)==ACVR_OK && r.model_nodes[3][14]==0);
    check(acvr_runtime_tick(p)==ACVR_OK && std::abs(r.model_nodes[3][14]-.03f)<.001f); // output amplitude, not fallback .15
    check(acvr_runtime_tick(p)==ACVR_OK && r.steps.size()==2 && r.model_nodes[3][14]<.03f);
    check(acvr_runtime_tick(p)==ACVR_OK && std::abs(r.model_nodes[3][14]-.0249f)<.001f); // held output did not retrigger
    check(acvr_runtime_tick(p)==ACVR_OK && acvr_runtime_tick(p)==ACVR_OK && std::abs(r.model_nodes[3][14]-.03f)<.001f);
    const auto before=r.outputs;
    check(acvr_runtime_tick(p)==ACVR_OK && r.outputs==before); // untracked hand cannot restart haptics
    check(acvr_runtime_tick(p)==ACVR_OK && r.model_nodes[3][14]==0); // held level after recovery remains suppressed
    check(acvr_runtime_tick(p)==ACVR_OK && acvr_runtime_tick(p)==ACVR_OK && std::abs(r.model_nodes[3][14]-.03f)<.001f);
    check(acvr_runtime_tick(p)==ACVR_OK && r.model_nodes[3][14]==0); // output overflow cancels visual route
    check(acvr_runtime_tick(p)==ACVR_OK && acvr_runtime_tick(p)==ACVR_OK && std::abs(r.model_nodes[3][14]-.03f)<.001f);
    check(acvr_runtime_destroy(p)==ACVR_OK);
    r.motion_routes[0].motion="unknown";p=nullptr;
    check(acvr::create_with_host(&c,&a,std::make_unique<Recorded>(r,std::vector<Sample>{}),&p)==ACVR_BAD_ARGUMENT && !p && r.closed==2);
    std::filesystem::remove(path);std::filesystem::remove(meta);std::filesystem::remove(dir);
}
#endif
int main() {
    try { timing_and_edges(); rational_and_budget(); pause_loss_and_zero_layers(); failures_and_ownership(); muzzle_math(); anchored_aim(); long_replay_and_invalid_samples(); reload_edges();
#ifdef ACVR_GUN_MODELS
        alternate_button_bindings();
        mapped_pause_stops_current_frame();
        configured_controls();
        configured_model();
        mapped_model_motion_and_handoff();
        handoff_and_recoil();
        runtime_output_routes();
#endif
    }
    catch(const std::exception &e) { std::cerr<<e.what()<<'\n'; return 1; }
    std::cout<<checks<<" runtime checks passed\n"; return 0;
}
