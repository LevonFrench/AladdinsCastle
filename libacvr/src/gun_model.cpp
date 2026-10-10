// SPDX-License-Identifier: MIT
#include "gun_model.hpp"
#include "pose_math.hpp"
#include <toml++/toml.hpp>
#include <algorithm>
#include <cmath>
#include <fstream>
#include <filesystem>
#include <map>
#include <set>
#include <stdexcept>

namespace acvr {
namespace {
void require(bool ok,const char *message) { if(!ok) throw std::runtime_error(message); }
template<size_t N> std::array<float,N> numbers(toml::node_view<const toml::node> value) {
    const auto *array=value.as_array(); require(array && array->size()==N,"invalid motion vector");
    std::array<float,N> out{};
    for(size_t i=0;i<N;++i) {
        const auto n=(*array)[i].value<double>(); require(n && std::isfinite(*n),"invalid motion number");
        out[i]=float(*n); require(std::isfinite(out[i]),"motion number overflow");
    }
    return out;
}
Matrix identity() { Matrix m{}; for(unsigned i=0;i<4;++i) m[i*5]=1; return m; }
Matrix multiply(const Matrix &a,const Matrix &b) {
    Matrix m{};
    for(unsigned c=0;c<4;++c) for(unsigned r=0;r<4;++r) for(unsigned k=0;k<4;++k) m[c*4+r]+=a[k*4+r]*b[c*4+k];
    return m;
}
bool pose_ok(const acvr_pose &p) {
    if(p.size<sizeof(p)||p.version!=ACVR_STRUCT_VERSION) return false;
    float norm=0;
    for(float f:p.position_m) if(!std::isfinite(f)) return false;
    for(float f:p.orientation_xyzw) { if(!std::isfinite(f)) return false; norm+=f*f; }
    return std::abs(norm-1)<.001f;
}
Matrix pose_matrix(const acvr_pose &p) {
    const auto q=normalized_quaternion(p.orientation_xyzw);
    const double x=q[0],y=q[1],z=q[2],w=q[3];
    return {float(1-2*y*y-2*z*z),float(2*x*y+2*z*w),float(2*x*z-2*y*w),0,
        float(2*x*y-2*z*w),float(1-2*x*x-2*z*z),float(2*y*z+2*x*w),0,
        float(2*x*z+2*y*w),float(2*y*z-2*x*w),float(1-2*x*x-2*y*y),0,
        p.position_m[0],p.position_m[1],p.position_m[2],1};
}
std::vector<uint8_t> read(const std::string &path,size_t limit) {
    require(!path.empty(),"empty gun asset path");
    std::ifstream stream(std::filesystem::u8path(path),std::ios::binary|std::ios::ate); require(bool(stream),"cannot open gun asset");
    const auto length=stream.tellg(); require(length>0 && uint64_t(length)<=limit,"gun asset file size out of bounds");
    std::vector<uint8_t> bytes(static_cast<size_t>(length)); stream.seekg(0);
    stream.read(reinterpret_cast<char *>(bytes.data()),static_cast<std::streamsize>(bytes.size()));
    require(bool(stream),"cannot read complete gun asset"); return bytes;
}
}
bool decode_gun_model(const std::vector<uint8_t> &glb,std::string_view metadata,
                      std::string_view expected,GunModel &out,std::string &error) noexcept {
    try {
        GunModel model;
        if(!decode_gun_glb(glb,model.asset,error)) return false;
        require(!metadata.empty() && metadata.size()<=1024*1024,"metadata size out of bounds");
        const auto table=toml::parse(metadata); model.id=table["id"].value_or(std::string{});
        require(!model.id.empty() && model.id==expected,"gun metadata id mismatch");
        std::map<std::string,uint32_t> nodes;
        for(uint32_t i=0;i<model.asset.nodes.size();++i) nodes.emplace(model.asset.nodes[i].name,i);
        std::set<uint32_t> driven;
        const auto *motions=table["motion"].as_table();
        require(!table.contains("motion")||motions,"motion must be a table");
        if(motions) for(const auto &[key,value]:*motions) {
            require(model.motions.size()<64,"too many motions");
            const auto *entry=value.as_table(); require(entry,"motion entry must be a table");
            GunMotion m; m.id=std::string(key.str()); require(!m.id.empty(),"empty motion id"); m.node=(*entry)["node"].value_or(std::string{});
            require(nodes.count(m.node)!=0,"unknown motion node");
            m.drive=(*entry)["drive"].value_or(std::string{});
            require(m.drive=="trigger"||m.drive=="recoil"||m.drive=="pump"||m.drive=="selector"||m.drive=="yaw"||m.drive=="pitch"||
                    (m.drive.size()>7 && m.drive.substr(0,7)=="button:"),"unsupported motion drive");
            const auto kind=(*entry)["kind"].value_or(std::string{});
            require(kind=="rotate"||kind=="slide","invalid motion kind"); m.rotate=kind=="rotate";
            m.axis=numbers<3>((*entry)["axis"]); const auto range=numbers<2>((*entry)["range"]);
            require(std::abs(m.axis[0]*m.axis[0]+m.axis[1]*m.axis[1]+m.axis[2]*m.axis[2]-1)<.001f,"motion axis must be unit length");
            require(range[1]>range[0],"motion range must increase"); m.minimum=range[0]; m.maximum=range[1];
            if(m.drive=="yaw"||m.drive=="pitch") m.rest=std::clamp(-m.minimum/(m.maximum-m.minimum),0.f,1.f);
            const auto duration=(*entry)["duration_ms"].value<int64_t>();
            require(!entry->contains("duration_ms")||duration.has_value(),"invalid recoil duration");
            if(duration) { require(*duration>0 && *duration<=10000,"recoil duration out of range"); m.duration_ms=uint32_t(*duration); }
            const auto *aliases=(*entry)["lod_nodes"].as_array();
            require(!entry->contains("lod_nodes")||aliases,"lod_nodes must be an array");
            if(aliases) {
                require(!aliases->empty() && aliases->size()<=2,"invalid LOD motion aliases");
                for(const auto &alias:*aliases) {
                    const auto name=alias.value<std::string>(); require(name && nodes.count(*name),"unknown LOD motion alias");
                    m.nodes.push_back(nodes.at(*name));
                }
                require(std::find(m.nodes.begin(),m.nodes.end(),nodes.at(m.node))!=m.nodes.end(),"LOD aliases omit canonical node");
            } else m.nodes.push_back(nodes.at(m.node));
            for(const auto n:m.nodes) {
                require(driven.insert(n).second,"node has multiple motion drivers");
                // Every static reference and its ancestors are excluded, not only muzzle.
                for(uint32_t i=0;i<model.asset.nodes.size();++i) {
                    const auto &name=model.asset.nodes[i].name;
                    if(name=="grip"||name=="grip_two"||name=="muzzle"||name.rfind("sight_",0)==0||name.rfind("fx_",0)==0)
                        for(int32_t a=int32_t(i);a>=0;a=model.asset.nodes[uint32_t(a)].parent)
                            require(uint32_t(a)!=n,"motion would move a static aim/reference anchor");
                }
            }
            model.motions.push_back(std::move(m));
        }
        out=std::move(model); error.clear(); return true;
    } catch(const std::exception &e) { try { error=e.what(); } catch(...) {} return false; }
      catch(...) { try { error="gun metadata decode failed"; } catch(...) {} return false; }
}
bool load_gun_model(const std::string &path,const std::string &metadata,std::string_view id,GunModel &out,std::string &error) noexcept {
    try {
        const auto bytes=read(path,32*1024*1024), text=read(metadata,1024*1024);
        return decode_gun_model(bytes,std::string_view(reinterpret_cast<const char *>(text.data()),text.size()),id,out,error);
    } catch(const std::exception &e) { try { error=e.what(); } catch(...) {} return false; }
      catch(...) { try { error="gun model read failed"; } catch(...) {} return false; }
}
void GunInstance::reset() { clear_motion(); sequence=0; }
void GunInstance::clear_motion() {
    states.assign(model.motions.size(),{});
    for(size_t i=0;i<states.size();++i) states[i].value=model.motions[i].rest;
}
acvr_result GunInstance::event(const acvr_gun_event &e,int64_t now) {
    if(e.size<sizeof(e)||e.version!=ACVR_STRUCT_VERSION||!e.node_utf8||e.sequence<=sequence||now<0||
       !std::isfinite(e.value)||e.value<0||e.value>1||e.kind<ACVR_GUN_EVENT_RECOIL||e.kind>ACVR_GUN_EVENT_AXIS||
       states.size()!=model.motions.size()) return ACVR_BAD_ARGUMENT;
    const auto it=std::find_if(model.motions.begin(),model.motions.end(),[&](const GunMotion &m){return m.node==e.node_utf8;});
    if(it==model.motions.end() || (e.kind==ACVR_GUN_EVENT_RECOIL)!=(it->drive=="recoil")) return ACVR_BAD_ARGUMENT;
    if(e.duration_ms>10000 || (e.kind!=ACVR_GUN_EVENT_RECOIL && e.duration_ms)) return ACVR_BAD_ARGUMENT;
    auto &s=states[size_t(it-model.motions.begin())]; s.value=e.value; s.start=now; s.pulse=e.kind==ACVR_GUN_EVENT_RECOIL;
    s.duration=int64_t(e.duration_ms?e.duration_ms:it->duration_ms)*1000000;
    sequence=e.sequence; return ACVR_OK;
}
acvr_result GunInstance::drive(std::string_view name,float value,bool pulse,int64_t now) {
    if(states.size()!=model.motions.size()||now<0||!std::isfinite(value)||value<0||value>1) return ACVR_BAD_ARGUMENT;
    for(size_t i=0;i<model.motions.size();++i) if(model.motions[i].drive==name) {
        if(!pulse && !states[i].pulse && states[i].value==value) continue;
        if(sequence==UINT64_MAX) return ACVR_BAD_STATE;
        acvr_gun_event e{}; ACVR_INIT(&e);e.sequence=sequence+1;e.node_utf8=model.motions[i].node.c_str();e.value=value;
        e.kind=pulse?ACVR_GUN_EVENT_RECOIL:ACVR_GUN_EVENT_AXIS;
        const auto result=event(e,now);if(result!=ACVR_OK) return result;
    }
    return ACVR_OK;
}
acvr_result GunInstance::pulse_motion(std::string_view id,float value,uint32_t duration,int64_t now) {
    if(states.size()!=model.motions.size()||now<0||!duration||duration>10000||!std::isfinite(value)||value<0||value>1)
        return ACVR_BAD_ARGUMENT;
    const auto it=std::find_if(model.motions.begin(),model.motions.end(),[&](const GunMotion &m){return m.id==id;});
    if(it==model.motions.end()) return ACVR_BAD_ARGUMENT;
    if(sequence==UINT64_MAX) return ACVR_BAD_STATE;
    auto &s=states[size_t(it-model.motions.begin())];s.value=value;s.start=now;s.duration=int64_t(duration)*1000000;s.pulse=true;
    ++sequence;return ACVR_OK;
}
acvr_result GunOutputRouter::configure(const std::vector<GunOutputRoute> &input,const GunInstance &gun,uint32_t slot,uint32_t player) {
    if(input.size()>64) return ACVR_BAD_ARGUMENT;
    std::vector<Route> prepared;bool recoil=false,trigger=false;std::set<std::string> targets;
    for(const auto &c:input) {
        if(c.kind<ACVR_OUTPUT_SOLENOID||c.kind>ACVR_OUTPUT_FFB||c.slot!=slot||c.player!=player||
           !c.duration_ms||c.duration_ms>10000||!std::isfinite(c.amplitude)||c.amplitude<0||c.amplitude>1||
           !targets.insert(c.motion).second) return ACVR_BAD_ARGUMENT;
        const auto it=std::find_if(gun.model.motions.begin(),gun.model.motions.end(),[&](const GunMotion &m){return m.id==c.motion;});
        if(it==gun.model.motions.end()) return ACVR_BAD_ARGUMENT;
        recoil=recoil||it->drive=="recoil";trigger=trigger||it->drive=="trigger";Route r;r.config=c;prepared.push_back(std::move(r));
    }
    routes=std::move(prepared);recoil_=recoil;trigger_=trigger;return ACVR_OK;
}
void GunOutputRouter::cancel() {for(auto &r:routes) {r.high=false;r.armed=false;r.expires=0;}}
acvr_result GunOutputRouter::consume(const acvr_output_event &e,int64_t now,bool tracked,GunInstance &gun) {
    if(now<0||!std::isfinite(e.strength)) return ACVR_BAD_ARGUMENT;
    for(auto &r:routes) {
        const auto &c=r.config;
        if(e.kind!=c.kind||e.player!=c.player||e.channel!=c.channel) continue;
        if((e.kind==ACVR_OUTPUT_FFB&&(e.effect<ACVR_FFB_CONSTANT||e.effect>ACVR_FFB_STOP))||
           e.strength>1||e.strength<(e.kind==ACVR_OUTPUT_FFB?-1.f:0.f)) return ACVR_BAD_ARGUMENT;
        const float magnitude=e.kind==ACVR_OUTPUT_FFB&&e.effect==ACVR_FFB_STOP?0.f:std::abs(e.strength);
        if(r.expires&&now>=r.expires) {r.high=false;r.expires=0;}
        if(!magnitude) {r.high=false;r.armed=true;r.expires=0;continue;}
        if(!tracked) {r.high=true;r.armed=false;r.expires=0;continue;}
        const bool rising=!r.high;r.high=true;
        if(e.duration_ms) {
            const int64_t duration=int64_t(e.duration_ms)*1000000;
            if(now>INT64_MAX-duration) return ACVR_BAD_ARGUMENT;
            r.expires=now+duration;
        } else r.expires=0;
        if(rising&&r.armed&&r.last_tick!=e.tick_id) {
            const auto result=gun.pulse_motion(c.motion,magnitude*c.amplitude,c.duration_ms,now);
            if(result!=ACVR_OK) return result;
            r.last_tick=e.tick_id;
        }
    }
    return ACVR_OK;
}
acvr_result GunInstance::draw(const acvr_pose &anchor,const acvr_pose &grip,const acvr_gun_slot_config &config,
                             float scale,float distance,int64_t now,GunDraw &out) const {
    auto angle=grip; for(unsigned i=0;i<3;++i) angle.position_m[i]=0;
    std::copy_n(config.angle_xyzw,4,angle.orientation_xyzw);
    if(!pose_ok(anchor)||!pose_ok(grip)||!pose_ok(angle)||!std::isfinite(scale)||scale<=0||
       !std::isfinite(distance)||distance<=0||now<0||states.size()!=model.motions.size()) return ACVR_BAD_ARGUMENT;
    GunDraw result; result.asset=&model.asset; result.slot=config.slot; result.hand=config.hand;
    result.visible=config.show_gun!=0; result.laser_mode=config.laser_mode;
    std::copy_n(config.body_rgba,4,result.body.begin()); std::copy_n(config.accent_rgba,4,result.accent.begin());
    const auto placement=multiply(multiply(pose_matrix(anchor),pose_matrix(grip)),pose_matrix(angle));
    auto scene_placement=placement;
    // Scale every spatial row, including translation; affine bottom row stays 1.
    for(unsigned c=0;c<4;++c) for(unsigned r=0;r<3;++r) scene_placement[c*4+r]*=scale;
    const auto muzzle=multiply(scene_placement,model.asset.nodes.at(model.asset.muzzle).bind_world);
    ACVR_INIT(&result.muzzle);
    for(unsigned i=0;i<3;++i) { result.muzzle.origin_scene[i]=muzzle[12+i]; result.muzzle.direction_scene[i]=-muzzle[8+i]/scale; }
    result.muzzle.max_distance_scene=distance*scale;
    std::vector<Matrix> local; local.reserve(model.asset.nodes.size());
    for(const auto &node:model.asset.nodes) local.push_back(node.local);
    for(size_t i=0;i<model.motions.size();++i) {
        const auto &m=model.motions[i]; const auto &s=states[i]; float value=s.value;
        if(s.pulse) value=m.rest+(value-m.rest)*(now<s.start?0.f:std::max(0.f,1.f-float(now-s.start)/float(s.duration)));
        const float amount=m.minimum+(m.maximum-m.minimum)*value; auto delta=identity();
        if(m.rotate) {
            acvr_pose p{}; ACVR_INIT(&p); const auto sine=std::sin(amount*.5f);
            for(unsigned k=0;k<3;++k) p.orientation_xyzw[k]=m.axis[k]*sine;
            p.orientation_xyzw[3]=std::cos(amount*.5f); delta=pose_matrix(p);
        } else for(unsigned k=0;k<3;++k) delta[12+k]=m.axis[k]*amount;
        for(const auto n:m.nodes) local[n]=multiply(local[n],delta);
    }
    result.scene_from_node.resize(local.size()); std::vector<bool> done(local.size());
    // Decoder already proved an acyclic connected hierarchy. Parent order is arbitrary.
    size_t count=0;
    while(count<local.size()) for(size_t i=0;i<local.size();++i) if(!done[i]) {
        const auto p=model.asset.nodes[i].parent;
        if(p<0||done[uint32_t(p)]) {
            result.scene_from_node[i]=multiply(p<0?scene_placement:result.scene_from_node[uint32_t(p)],local[i]);
            for(float f:result.scene_from_node[i]) if(!std::isfinite(f)) return ACVR_BAD_ARGUMENT;
            done[i]=true; ++count;
        }
    }
    if(!std::isfinite(result.muzzle.max_distance_scene)) return ACVR_BAD_ARGUMENT;
    out=std::move(result); return ACVR_OK;
}
}
