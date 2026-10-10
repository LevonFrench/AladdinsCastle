// SPDX-License-Identifier: MIT
#include "gun_model.hpp"
#include "gun_fixture.hpp"
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>

unsigned checks=0;
void check(bool ok) { ++checks; if(!ok) throw std::runtime_error("model check "+std::to_string(checks)); }
template<class T> T init() { T x{}; ACVR_INIT(&x); return x; }
acvr_pose pose() { auto p=init<acvr_pose>(); p.orientation_xyzw[3]=1; return p; }
const std::string metadata=R"(id="synthetic"
[motion.recoil]
node="body_mesh"
kind="slide"
axis=[0,0,1]
range=[0,0.015]
drive="recoil"
lod_nodes=["body_mesh","body_mesh_lod1"]
duration_ms=45
[unknown_extension]
future=true
)";
acvr_gun_slot_config config() {
    auto c=init<acvr_gun_slot_config>(); c.angle_xyzw[3]=1; c.show_gun=1;
    for(unsigned i=0;i<4;++i) {c.body_rgba[i]=.5f;c.accent_rgba[i]=1;}
    return c;
}
void load_and_motion() {
    Fixture f; acvr::GunInstance gun; std::string error;
    check(acvr::decode_gun_model(f.bytes(),metadata,"synthetic",gun.model,error)); gun.reset();
    check(gun.model.motions.size()==1 && gun.model.motions[0].nodes.size()==2);
    auto anchor=pose(),grip=pose(); anchor.position_m[0]=2; grip.position_m[1]=1;
    auto c=config(); acvr::GunDraw base,draw;
    check(gun.draw(anchor,grip,c,10,5,0,base)==ACVR_OK);
    check(base.scene_from_node[3][12]==20.1f && base.scene_from_node[3][13]==10);
    check(std::abs(base.muzzle.origin_scene[2]+2)<.001f && base.muzzle.direction_scene[2]==-1);
    check(base.body[0]==.5f && base.accent[0]==1 && base.visible);
    auto e=init<acvr_gun_event>(); e.sequence=1; e.node_utf8="body_mesh"; e.kind=ACVR_GUN_EVENT_RECOIL; e.value=1;
    check(gun.event(e,1000000)==ACVR_OK);
    check(gun.draw(anchor,grip,c,10,5,1000000,draw)==ACVR_OK);
    check(std::abs(draw.scene_from_node[3][14]-.15f)<.001f && std::abs(draw.scene_from_node[5][14]-.15f)<.001f);
    check(draw.muzzle.origin_scene[2]==base.muzzle.origin_scene[2]);
    check(gun.event(e,1000000)==ACVR_BAD_ARGUMENT); // no per-eye/replay duplicate
    check(gun.draw(anchor,grip,c,10,5,23500000,draw)==ACVR_OK && std::abs(draw.scene_from_node[3][14]-.075f)<.001f);
    check(gun.draw(anchor,grip,c,10,5,46000000,draw)==ACVR_OK && draw.scene_from_node[3][14]==0);
    ++e.sequence; check(gun.event(e,47000000)==ACVR_OK); gun.clear_motion();
    check(gun.draw(anchor,grip,c,10,5,48000000,draw)==ACVR_OK && draw.scene_from_node[3][14]==0);
    check(gun.event(e,48000000)==ACVR_BAD_ARGUMENT); // cancellation retains sequence fence
    ++e.sequence; e.value=2; check(gun.event(e,48000000)==ACVR_BAD_ARGUMENT);
    e.value=1; e.node_utf8="muzzle"; check(gun.event(e,48000000)==ACVR_BAD_ARGUMENT);
    e.node_utf8="body_mesh"; e.kind=ACVR_GUN_EVENT_AXIS; check(gun.event(e,48000000)==ACVR_BAD_ARGUMENT);
    c.show_gun=0;c.laser_mode=ACVR_LASER_LINE;
    check(gun.draw(anchor,grip,c,10,5,48000000,draw)==ACVR_OK && !draw.visible && draw.laser_mode==ACVR_LASER_LINE);
    anchor.orientation_xyzw[1]=anchor.orientation_xyzw[3]=float(std::sqrt(.5));
    check(gun.draw(anchor,grip,c,10,5,48000000,draw)==ACVR_OK && std::abs(draw.muzzle.direction_scene[0]+1)<.001f);
    check(std::abs(draw.muzzle.origin_scene[0]-18)<.001f && std::abs(draw.muzzle.origin_scene[1]-10.2f)<.001f);
    grip.orientation_xyzw[3]=2; check(gun.draw(anchor,grip,c,10,5,48000000,draw)==ACVR_BAD_ARGUMENT);
}
void invalid_metadata_and_files() {
    Fixture f; acvr::GunModel model; model.id="preserve"; std::string error;
    for(const auto &text:std::vector<std::string>{"id='wrong'","invalid [", "id='synthetic'\nmotion=[]",
        "id='synthetic'\n[motion.bad]\nnode='muzzle'\nkind='slide'\naxis=[0,0,1]\nrange=[0,1]\ndrive='recoil'",
        "id='synthetic'\n[motion.bad]\nnode='body_mesh'\nkind='slide'\naxis=[0,0,2]\nrange=[0,1]\ndrive='recoil'"}) {
        check(!acvr::decode_gun_model(f.bytes(),text,"synthetic",model,error)); check(model.id=="preserve"&&!error.empty());
    }
    const auto directory=std::filesystem::current_path()/"synthetic-gun-model-tests";
    std::filesystem::create_directories(directory);
    const auto glb=directory/std::filesystem::u8path("synthetic-\xc3\xa9.glb"), toml=directory/"synthetic.toml";
    { std::ofstream out(glb,std::ios::binary);const auto bytes=f.bytes();out.write(reinterpret_cast<const char *>(bytes.data()),std::streamsize(bytes.size())); }
    { std::ofstream out(toml);out<<metadata; }
    check(acvr::load_gun_model(glb.u8string(),toml.u8string(),"synthetic",model,error));
    check(!acvr::load_gun_model((directory/"absent.glb").string(),toml.string(),"synthetic",model,error));
    check(model.id=="synthetic");
    std::filesystem::remove(glb);std::filesystem::remove(toml);std::filesystem::remove(directory);
}
void logical_drives() {
    Fixture f;acvr::GunInstance gun;std::string error;
    auto trigger_metadata=metadata;const auto at=trigger_metadata.find("drive=\"recoil\"");trigger_metadata.replace(at,14,"drive=\"trigger\"");
    check(acvr::decode_gun_model(f.bytes(),trigger_metadata,"synthetic",gun.model,error));gun.reset();
    check(gun.drive("trigger",1,false,0)==ACVR_OK);auto p=pose();auto c=config();acvr::GunDraw out;
    check(gun.draw(p,p,c,10,5,0,out)==ACVR_OK && std::abs(out.scene_from_node[3][14]-.15f)<.001f);
    check(gun.drive("trigger",1,false,1000000)==ACVR_OK); // unchanged level does not consume another sequence
    auto e=init<acvr_gun_event>();e.sequence=2;e.node_utf8="body_mesh";e.kind=ACVR_GUN_EVENT_AXIS;e.value=.5f;
    check(gun.event(e,1000000)==ACVR_OK);
    check(gun.drive("trigger",0,false,2000000)==ACVR_OK && gun.draw(p,p,c,10,5,2000000,out)==ACVR_OK && out.scene_from_node[3][14]==0);
    check(gun.drive("trigger",2,false,2000000)==ACVR_BAD_ARGUMENT);
}
int main() {
    try { load_and_motion(); invalid_metadata_and_files(); logical_drives(); }
    catch(const std::exception &e) {std::cerr<<e.what()<<'\n';return 1;}
    std::cout<<checks<<" model checks passed\n";
}
