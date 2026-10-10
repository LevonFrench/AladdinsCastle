// SPDX-License-Identifier: MIT
#include "runtime_host.hpp"
#include <cmath>
#include <cstring>
#include <iostream>
#include <stdexcept>
#include <string>
#include <thread>

template<class T> T init() { T x{}; ACVR_INIT(&x); return x; }
unsigned checks=0;
void check(bool value) { ++checks; if(!value) throw std::runtime_error("check " + std::to_string(checks)); }
struct Receipt {
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
    }
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
struct Sample { int64_t time; bool trigger=false,coin=false,focused=true,render=true,tracked=true; float pedal=0; };
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
        if(!s.tracked) d.tracking.right.aim_flags=0;
        auto b=init<acvr_button_input>(); b.semantic=ACVR_BUTTON_COIN; b.state=s.coin?ACVR_INPUT_HELD:0; d.buttons.push_back(b);
        auto a=init<acvr_axis_input>(); a.semantic=ACVR_AXIS_COVER_PEDAL; a.value=s.pedal; d.axes.push_back(a);
        for(unsigned i=0;i<2;++i) { d.eyes[i]=init<acvr_draw_info>(); d.eyes[i].target=init<acvr_render_target>();
            d.eyes[i].view_count=1; d.eyes[i].view_stride=sizeof(acvr_eye); d.eyes[i].views=&eyes[i]; }
        return ACVR_OK;
    }
    acvr_result end(bool rendered) override { ++r.ends; if(rendered) ++r.submitted; return ACVR_OK; }
    void output(const acvr_output_event &) override { ++r.outputs; }
    void cancel_effects() noexcept override { ++r.cancelled; }
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
int main() {
    try { timing_and_edges(); rational_and_budget(); pause_loss_and_zero_layers(); failures_and_ownership(); muzzle_math(); anchored_aim(); long_replay_and_invalid_samples(); }
    catch(const std::exception &e) { std::cerr<<e.what()<<'\n'; return 1; }
    std::cout<<checks<<" runtime checks passed\n"; return 0;
}
