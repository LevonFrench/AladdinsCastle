// SPDX-License-Identifier: GPL-3.0-only
#include "n22_upstream_bridge.hpp"
#include "n22_source_hooks.h"
#include <cmath>
#include <cstring>
#include <stdexcept>

namespace n22 {
std::shared_ptr<const VideoSnapshot> copy_ss22_video(const ss22_regs &r,uint64_t tick,uint16_t output) {
    if(!r.poly_word || !r.pal || !r.mixer || !r.cgram || !r.textram || !r.spriteram || !r.vics ||
       r.spriteram_size!=bank_sizes[Sprites] || r.vics_size!=bank_sizes[Vics]) throw std::invalid_argument("Invalid source video banks");
    auto s=std::make_shared<VideoSnapshot>();s->tick=tick;s->output_bits=output;
    const void *src[BankCount]={r.pal,r.mixer,r.cgram,r.textram,r.spriteram,r.vics,r.spotram};
    for(size_t i=0;i<BankCount;++i) {
        s->banks[i].resize(bank_sizes[i]);
        if(src[i]) std::memcpy(s->banks[i].data(),src[i],bank_sizes[i]);
    }
    if(r.spotram) std::memcpy(s->spot_words.data(),r.spotram,sizeof(s->spot_words));
    s->polygon_words.resize(0x8000);
    for(int i=0;i<0x8000;++i) s->polygon_words[static_cast<size_t>(i)]=r.poly_word(i);
    std::memcpy(s->czattr.data(),r.czattr,sizeof(r.czattr));
    std::memcpy(s->tilemapattr.data(),r.tilemapattr,sizeof(r.tilemapattr));
    std::memcpy(s->vics_ctl.data(),r.vics_ctl,sizeof(r.vics_ctl));
    for(size_t i=0;i<4;++i) {
        if(!r.czram[i]) throw std::invalid_argument("Missing source CZ bank");
        std::memcpy(s->czram[i].data(),r.czram[i],sizeof(s->czram[i]));
    }
    s->walk=r.walk;s->spot_enabled=r.spotram && r.spot_enabled;return s;
}
namespace {
thread_local const VideoSnapshot *reading;
uint32_t owned_poly(int i) {return reading?reading->polygon_words[static_cast<uint32_t>(i)&0x7fff]:0;}
struct SourceState {
    Worker &worker;
    const std::function<void(NativeInput)> &apply;
    NativeInput input{};
    uint16_t output=0;
    std::shared_ptr<const VideoSnapshot> snapshot;
};
int begin(void *p) {
    auto &s=*static_cast<SourceState *>(p);
    try {
        if(!s.worker.begin(s.input)) return 0;
        s.snapshot.reset();s.output=0;s.apply(s.input);return 1;
    } catch(...) {return -1;}
}
int publish(void *p) {
    try {auto &s=*static_cast<SourceState *>(p);return s.worker.publish(s.snapshot)?1:-1;}
    catch(...) {return -1;}
}
int stopped(void *p) {try {return static_cast<SourceState *>(p)->worker.stop_requested()?1:0;} catch(...) {return 1;}}
int video(void *p,const void *regs) {
    auto &s=*static_cast<SourceState *>(p);
    try {
        if(!regs || s.snapshot) return -1;
        s.snapshot=copy_ss22_video(*static_cast<const ss22_regs *>(regs),s.input.tick,s.output);return 1;
    } catch(...) {return -1;}
}
void outputs(void *p,uint16_t bits) {static_cast<SourceState *>(p)->output=bits;}
}
void with_ss22_regs(const VideoSnapshot &s,const std::function<void(const ss22_regs &)> &fn) {
    if(reading || s.polygon_words.size()!=0x8000) throw std::invalid_argument("Invalid or nested owner preparation");
    for(size_t i=0;i<BankCount;++i) if(s.banks[i].size()!=bank_sizes[i]) throw std::invalid_argument("Invalid snapshot bank length");
    ss22_regs r{};r.walk=s.walk;r.poly_word=owned_poly;
    r.pal=s.banks[Palette].data();r.mixer=s.banks[Mixer].data();
    r.cgram=s.banks[Characters].data();r.textram=s.banks[Text].data();
    r.spriteram=s.banks[Sprites].data();r.spriteram_size=s.banks[Sprites].size();
    r.vics=s.banks[Vics].data();r.vics_size=s.banks[Vics].size();
    // ss22_prepare retains this pointer for replay: storage must live in the lease.
    r.spotram=s.spot_words.data();r.spot_enabled=s.spot_enabled;
    std::memcpy(r.czattr,s.czattr.data(),sizeof(r.czattr));
    std::memcpy(r.tilemapattr,s.tilemapattr.data(),sizeof(r.tilemapattr));
    std::memcpy(r.vics_ctl,s.vics_ctl.data(),sizeof(r.vics_ctl));
    for(size_t i=0;i<4;++i) r.czram[i]=s.czram[i].data();
    reading=&s;
    try {fn(r);} catch(...) {reading=nullptr;throw;}
    reading=nullptr;
}
CapturedQuad copy_geo_quad(const geo_quad &q,const geo_view *v) {
    CapturedQuad out;out.quad=q;
    if(v && !q.direct && v->zoom_shift>=-120 && v->zoom_shift<=120) {
        out.camera=*v;out.focal=std::ldexp(static_cast<float>(v->zoom_mant),-v->zoom_shift);
        out.cx=320.f+static_cast<float>(v->vx);out.cy=240.f+static_cast<float>(v->vy);
        out.has_camera=std::isfinite(out.focal) && out.focal>0;
    }
    return out;
}
acvr_result copy_geo_fog_triangle(const CapturedQuad &captured,uint64_t tick,std::array<uint32_t,3> indices,FogQuad &out) {
    const auto &q=captured.quad;
    if(!tick || q.nrv<3 || q.nrv>10 || q.cz_type<0 || q.cz_type>3) return ACVR_BAD_ARGUMENT;
    FogQuad copied;copied.provided=true;copied.has_native_depth=true;copied.tick=tick;
    copied.colour_word=q.color&0xffffff;copied.cz_adjust=static_cast<uint32_t>(q.cz_adjust)&0xffffff;
    copied.cz_type=static_cast<uint8_t>(q.cz_type);
    for(size_t i=0;i<3;++i) {
        if(indices[i]>=static_cast<uint32_t>(q.nrv)) return ACVR_BAD_ARGUMENT;
        copied.native_depth[i]=q.rv[indices[i]].z;
    }
    out=copied;return ACVR_OK;
}
std::vector<CapturedQuad> prepare_with_capture(const VideoSnapshot &s,
        const std::function<void(const ss22_regs &)> &prepare) {
    struct Sink {std::vector<CapturedQuad> quads;bool failed=false;} sink;
    acvr_ss22_hooks hooks{};hooks.user=&sink;
    hooks.capture_quad=[](void *user,const void *quad,const void *view) {
        auto &out=*static_cast<Sink *>(user);
        try {
            if(!quad) {out.failed=true;return;}
            out.quads.push_back(copy_geo_quad(*static_cast<const geo_quad *>(quad),static_cast<const geo_view *>(view)));
        } catch(...) {out.failed=true;}
    };
    if(acvr_ss22_bind_hooks(&hooks)!=ACVR_OK) throw std::invalid_argument("Owner capture hooks already bound");
    try {with_ss22_regs(s,prepare);} catch(...) {acvr_ss22_bind_hooks(nullptr);throw;}
    acvr_ss22_bind_hooks(nullptr);
    if(sink.failed) throw std::runtime_error("Geometry capture failed");
    return std::move(sink.quads);
}
acvr_result run_source_entry(Worker &worker,void (*entry)(void),const std::function<void(NativeInput)> &apply) {
    static std::mutex single_engine;
    std::unique_lock<std::mutex> instance(single_engine,std::try_to_lock);
    if(!instance.owns_lock()) return ACVR_BAD_STATE;
    SourceState state{worker,apply,{},0,{}};
    acvr_ss22_hooks hooks{&state,begin,publish,stopped,video,outputs,nullptr};
    if(acvr_ss22_bind_hooks(&hooks)!=ACVR_OK) return ACVR_BAD_STATE;
    auto result=acvr_ss22_run_entry(entry);
    acvr_ss22_bind_hooks(nullptr);return result;
}
}
