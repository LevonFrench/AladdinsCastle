// SPDX-License-Identifier: GPL-3.0-only
#include "n22_cpu_backend.hpp"
#include "n22_gl_renderer.hpp"
#include <cstring>
#include <limits>
#include <memory>

struct acvr_frame { n22::Frame scene; };
struct acvr_backend {
    n22::Frame staged;
    std::unique_ptr<acvr_frame> leased;
    std::unique_ptr<n22::GlRenderer> renderer;
    std::vector<acvr_game_camera> previous_cameras;
    uint64_t tick = 0, input_tick = 0;
    bool paused = false, staged_valid = false;
};
namespace {
template<class T> acvr_result valid(const T *p) {
    if(!p) return ACVR_BAD_ARGUMENT;
    return p->version==ACVR_STRUCT_VERSION && p->size>=sizeof(T)?ACVR_OK:ACVR_BAD_VERSION;
}
template<class T> void payload(T *out,const T &value) {
    // Keep caller size/version and any append-only tail, never assign entire records.
    std::memcpy(reinterpret_cast<unsigned char *>(out)+8,
                reinterpret_cast<const unsigned char *>(&value)+8,sizeof(T)-8);
}
bool lease(acvr_backend *b,const acvr_frame *f) { return b && f && b->leased.get()==f; }
acvr_result ACVR_CALL open(const acvr_open_info *in,acvr_backend **out,acvr_backend_info *info) {
    if(!out) return ACVR_BAD_ARGUMENT;
    *out=nullptr;
    if(auto r=valid(in);r!=ACVR_OK) return r;
    if(auto r=valid(info);r!=ACVR_OK) return r;
    if(!in->game_id_utf8 || std::strcmp(in->game_id_utf8,"synthetic-system22")!=0 ||
        (in->content_root_utf8 && *in->content_root_utf8) ||
        (in->storage_root_utf8 && *in->storage_root_utf8) ||
        (in->backend_options_utf8 && *in->backend_options_utf8)) return ACVR_UNSUPPORTED;
    if(auto r=valid(&in->graphics);r!=ACVR_OK) return r;
    if(in->graphics.api!=0) return ACVR_UNSUPPORTED;
    try {
        auto b=std::make_unique<acvr_backend>();
        acvr_backend_info value{}; value.capabilities=ACVR_CAP_RAYCAST;
        value.native_rate_num=59906; value.native_rate_den=1000;
        value.player_count=1; value.scene_units_per_metre=1;
        value.control_stride=sizeof(acvr_control_desc);
        payload(info,value); *out=b.release(); return ACVR_OK;
    } catch(...) { return ACVR_ERROR; }
}
void ACVR_CALL close(acvr_backend *b) {
    if(b && b->renderer) {
        if(!b->renderer->owner_thread()) return; // invalid cross-thread call; owner can retry
        b->renderer->shutdown();
    }
    delete b;
}
acvr_result ACVR_CALL pause(acvr_backend *b,uint32_t paused) {
    if(!b || paused>1) return ACVR_BAD_ARGUMENT;
    b->paused=paused!=0; b->input_tick=0; return ACVR_OK;
}
acvr_result ACVR_CALL inputs(acvr_backend *b,const acvr_inputs *in) {
    if(!b) return ACVR_BAD_ARGUMENT;
    if(auto r=valid(in);r!=ACVR_OK) return r;
    if(b->paused || b->leased) return ACVR_BAD_STATE;
    if(in->tick_id!=b->tick+1) return ACVR_BAD_ARGUMENT;
    // This harness declares no controls. Do not silently consume real gun events.
    if(in->gun_count || in->axis_count || in->button_count) return ACVR_UNSUPPORTED;
    b->input_tick=in->tick_id; return ACVR_OK;
}
acvr_result ACVR_CALL step(acvr_backend *b,const acvr_step_info *in,acvr_frame **out,acvr_frame_info *info) {
    if(!out) return ACVR_BAD_ARGUMENT;
    *out=nullptr;
    if(!b) return ACVR_BAD_ARGUMENT;
    if(auto r=valid(in);r!=ACVR_OK) return r;
    if(auto r=valid(info);r!=ACVR_OK) return r;
    if(b->paused || b->leased) return ACVR_BAD_STATE;
    if(in->tick_id!=b->tick+1 || b->input_tick!=in->tick_id ||
        in->tick_id-1>std::numeric_limits<uint64_t>::max()/1000000000000ULL ||
        in->simulation_time_ns!=static_cast<int64_t>((in->tick_id-1)*1000000000000ULL/59906ULL)) return ACVR_BAD_ARGUMENT;
    try {
        auto f=std::make_unique<acvr_frame>();
        if(b->staged_valid) f->scene=b->staged;
        if(f->scene.cameras.empty()) {
            f->scene.cameras=b->previous_cameras;
            for(auto &c:f->scene.cameras) c.flags|=ACVR_CAMERA_RETAINED;
        }
        f->scene.id=in->tick_id;
        b->previous_cameras=f->scene.cameras;
        acvr_frame_info value{}; value.frame_id=in->tick_id;
        value.camera_count=static_cast<uint32_t>(f->scene.cameras.size());
        value.primary_camera_id=value.camera_count?0:ACVR_NO_CAMERA;
        payload(info,value); b->tick=in->tick_id; b->staged_valid=false;
        b->leased=std::move(f); *out=b->leased.get(); return ACVR_OK;
    } catch(...) { return ACVR_ERROR; }
}
void ACVR_CALL release(acvr_backend *b,acvr_frame *f) {
    if(lease(b,f)) {
        if(b->renderer) {
            if(!b->renderer->owner_thread()) return;
            b->renderer->release_frame(f->scene.id);
        }
        b->leased.reset();
    }
}
acvr_result ACVR_CALL camera(acvr_backend *b,const acvr_frame *f,uint32_t id,acvr_game_camera *out) {
    if(!lease(b,f)) return ACVR_BAD_STATE;
    if(auto r=valid(out);r!=ACVR_OK) return r;
    if(id>=f->scene.cameras.size()) return ACVR_BAD_ARGUMENT;
    payload(out,f->scene.cameras[id]); return ACVR_OK;
}
acvr_result ACVR_CALL draw(acvr_backend *b,const acvr_frame *f,const acvr_draw_info *in) {
    if(!b || !b->renderer) return ACVR_UNSUPPORTED;
    if(!lease(b,f)) return ACVR_BAD_STATE;
    if(!in) return ACVR_BAD_ARGUMENT;
    try {return b->renderer->draw(f->scene,*in);} catch(...) {return ACVR_ERROR;}
}
acvr_result ACVR_CALL outputs(acvr_backend *b,acvr_outputs *out) {
    if(!b) return ACVR_BAD_ARGUMENT;
    if(auto r=valid(out);r!=ACVR_OK) return r;
    if(!out->capacity || !out->events || out->stride<sizeof(acvr_output_event) ||
        out->stride%alignof(acvr_output_event) ||
        out->capacity>std::numeric_limits<size_t>::max()/out->stride) return ACVR_BAD_ARGUMENT;
    out->count=0; out->dropped=0; return ACVR_OK;
}
acvr_result ACVR_CALL hit(acvr_backend *b,const acvr_frame *f,const acvr_ray *r,acvr_hit *out) {
    if(!lease(b,f)) return ACVR_BAD_STATE;
    if(!r || !out) return ACVR_BAD_ARGUMENT;
    return n22::raycast(f->scene,*r,*out);
}
}
extern "C" acvr_result ACVR_CALL acvr_backend_query(uint32_t abi,acvr_backend_api *out) {
    if(abi!=ACVR_ABI_VERSION) return ACVR_BAD_VERSION;
    if(auto r=valid(out);r!=ACVR_OK) return r;
    acvr_backend_api api{}; api.abi_version=ACVR_ABI_VERSION;
    api.supported_graphics=0;
    api.game_open=open; api.game_close=close; api.game_pause=pause; api.game_set_inputs=inputs;
    api.game_step=step; api.game_release_frame=release; api.game_camera=camera;
    api.game_draw_eye=draw; api.game_poll_outputs=outputs; api.game_raycast=hit;
    payload(out,api); return ACVR_OK;
}
namespace n22 {
acvr_result configure_gl_draw(acvr_backend *b,const acvr_graphics_device &device) {
    if(!b) return ACVR_BAD_ARGUMENT;
    if(b->leased || b->renderer) return ACVR_BAD_STATE;
    try {
        auto renderer=std::make_unique<GlRenderer>();
        const auto result=renderer->initialize(device);
        if(result!=ACVR_OK) return result;
        b->renderer=std::move(renderer);return ACVR_OK;
    } catch(...) {return ACVR_ERROR;}
}
GlDiagnostic gl_diagnostic(acvr_backend *b) {return b && b->renderer?b->renderer->diagnostic():GlDiagnostic{};}
acvr_result stage_cpu_scene(acvr_backend *b,const SceneInput &in) {
    if(!b) return ACVR_BAD_ARGUMENT;
    if(b->leased) return ACVR_BAD_STATE;
    try {
        Frame f;
        auto r=prepare(in,b->tick+1,f);
        if(r!=ACVR_OK) return r;
        b->staged=std::move(f); b->staged_valid=true; return ACVR_OK;
    } catch(...) { return ACVR_ERROR; }
}
acvr_result draw_cpu_frame(acvr_backend *b,const acvr_frame *f,const acvr_eye &e,Image &image,bool hud) {
    if(!lease(b,f)) return ACVR_BAD_STATE;
    try { return draw_cpu(f->scene,e,image,hud); } catch(...) { return ACVR_ERROR; }
}
acvr_result compose_cpu_frame(acvr_backend *b,const acvr_frame *f,const acvr_eye &e,const CompositionEyeSpans &spans,Image &image) {
    if(!lease(b,f)) return ACVR_BAD_STATE;
    return compose_cpu(f->scene,e,spans,image);
}
acvr_result inspect_fog_frame(acvr_backend *b,const acvr_frame *f,uint32_t triangle,uint32_t vertex,FogDecision &out) {
    if(!lease(b,f)) return ACVR_BAD_STATE;
    if(triangle>=f->scene.triangles.size()) return ACVR_BAD_ARGUMENT;
    const auto &q=f->scene.triangles[triangle].fog;
    if(auto r=validate_fog_binding(f->scene.fog,q,f->scene.id);r!=ACVR_OK) return r;
    return decide_fog(f->scene.fog,q,vertex,out);
}
}
