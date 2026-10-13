// SPDX-License-Identifier: MIT
#include "gun_model.hpp"
#include "hand_assembly.hpp"
#include "gun_fixture.hpp"
#include <algorithm>
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
    const auto exact=draw;
    anchor.orientation_xyzw[1]=anchor.orientation_xyzw[3]=std::sqrt(.5004f);
    check(gun.draw(anchor,grip,c,10,5,48000000,draw)==ACVR_OK);
    for(unsigned i=0;i<16;++i) check(std::abs(draw.scene_from_node[3][i]-exact.scene_from_node[3][i])<.00001f);
    for(unsigned i=0;i<3;++i) check(std::abs(draw.muzzle.origin_scene[i]-exact.muzzle.origin_scene[i])<.00001f&&std::abs(draw.muzzle.direction_scene[i]-exact.muzzle.direction_scene[i])<.00001f);
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
void routed_outputs() {
    Fixture f;acvr::GunInstance gun;std::string error;
    auto text=metadata;const auto at=text.find("motion.recoil");text.replace(at,13,"motion.slide-output");
    check(acvr::decode_gun_model(f.bytes(),text,"synthetic",gun.model,error));gun.reset();
    acvr::GunOutputRoute route;route.kind=ACVR_OUTPUT_SOLENOID;route.channel=7;route.motion="slide-output";route.amplitude=.5f;route.duration_ms=100;
    acvr::GunOutputRouter router;check(router.configure({route},gun,0,0)==ACVR_OK && router.owns_recoil());
    auto bad=route;bad.motion="body_mesh";check(router.configure({bad},gun,0,0)==ACVR_BAD_ARGUMENT && router.owns_recoil());
    check(router.configure({route,route},gun,0,0)==ACVR_BAD_ARGUMENT);
    auto e=init<acvr_output_event>();e.kind=ACVR_OUTPUT_SOLENOID;e.channel=7;e.tick_id=1;e.strength=1;
    auto p=pose();auto c=config();acvr::GunDraw draw;
    check(router.consume(e,0,true,gun)==ACVR_OK);
    check(gun.draw(p,p,c,10,5,0,draw)==ACVR_OK && std::abs(draw.scene_from_node[3][14]-.075f)<.001f);
    ++e.tick_id;check(router.consume(e,50000000,true,gun)==ACVR_OK);
    check(gun.draw(p,p,c,10,5,50000000,draw)==ACVR_OK && std::abs(draw.scene_from_node[3][14]-.0375f)<.001f); // held level did not retrigger
    e.strength=0;check(router.consume(e,60000000,true,gun)==ACVR_OK);
    e.strength=1;check(router.consume(e,60000000,true,gun)==ACVR_OK);
    e.strength=0;check(router.consume(e,70000000,true,gun)==ACVR_OK);
    e.strength=1;check(router.consume(e,70000000,true,gun)==ACVR_OK); // same native tick never retriggers
    check(gun.draw(p,p,c,10,5,70000000,draw)==ACVR_OK && std::abs(draw.scene_from_node[3][14]-.0675f)<.001f);
    router.cancel();gun.clear_motion();++e.tick_id;
    check(router.consume(e,80000000,true,gun)==ACVR_OK);
    check(gun.draw(p,p,c,10,5,80000000,draw)==ACVR_OK && draw.scene_from_node[3][14]==0);
    e.strength=0;check(router.consume(e,90000000,true,gun)==ACVR_OK);
    e.strength=1;++e.tick_id;check(router.consume(e,100000000,false,gun)==ACVR_OK);
    ++e.tick_id;check(router.consume(e,110000000,true,gun)==ACVR_OK);
    check(gun.draw(p,p,c,10,5,110000000,draw)==ACVR_OK && draw.scene_from_node[3][14]==0);
    e.strength=0;check(router.consume(e,120000000,true,gun)==ACVR_OK);
    e.strength=1;e.duration_ms=10;++e.tick_id;check(router.consume(e,130000000,true,gun)==ACVR_OK);
    ++e.tick_id;check(router.consume(e,150000000,true,gun)==ACVR_OK); // previous explicit-duration level expired
    check(gun.draw(p,p,c,10,5,150000000,draw)==ACVR_OK && std::abs(draw.scene_from_node[3][14]-.075f)<.001f);
    e.strength=2;check(router.consume(e,160000000,true,gun)==ACVR_BAD_ARGUMENT);
    check(gun.pulse_motion("missing",1,45,0)==ACVR_BAD_ARGUMENT);
    check(gun.pulse_motion("slide-output",1,0,0)==ACVR_BAD_ARGUMENT);
    route.kind=ACVR_OUTPUT_FFB;check(router.configure({route},gun,0,0)==ACVR_OK);gun.clear_motion();
    e.kind=ACVR_OUTPUT_FFB;e.effect=ACVR_FFB_CONSTANT;e.strength=-.5f;e.duration_ms=0;++e.tick_id;
    check(router.consume(e,170000000,true,gun)==ACVR_OK);
    check(gun.draw(p,p,c,10,5,170000000,draw)==ACVR_OK && std::abs(draw.scene_from_node[3][14]-.0375f)<.001f);
    e.effect=ACVR_FFB_STOP;e.strength=0;check(router.consume(e,180000000,true,gun)==ACVR_OK);
}
void mounted_neutral_rest() {
    Fixture f;acvr::GunInstance gun;std::string error;
    const std::string text="id='synthetic'\n[motion.swivel]\nnode='body_mesh'\nkind='rotate'\naxis=[0,1,0]\nrange=[-0.7,0.7]\ndrive='yaw'\n";
    check(acvr::decode_gun_model(f.bytes(),text,"synthetic",gun.model,error));gun.reset();
    auto p=pose();auto c=config();acvr::GunDraw base,moved;
    check(gun.draw(p,p,c,1,5,0,base)==ACVR_OK && std::abs(base.scene_from_node[3][0]-1)<.0001f);
    check(gun.drive("yaw",1,false,0)==ACVR_OK && gun.draw(p,p,c,1,5,0,moved)==ACVR_OK);
    check(std::abs(moved.scene_from_node[3][0]-std::cos(.7f))<.0001f && moved.muzzle.direction_scene[2]==base.muzzle.direction_scene[2]);
    gun.clear_motion();check(gun.draw(p,p,c,1,5,1,moved)==ACVR_OK && moved.scene_from_node==base.scene_from_node);
    check(gun.pulse_motion("swivel",1,100,1)==ACVR_OK);
    check(gun.draw(p,p,c,1,5,100000001,moved)==ACVR_OK && moved.scene_from_node==base.scene_from_node);
}
void alternate_primary_mount() {
    Fixture f;acvr::GunInstance gun;std::string error;
    check(acvr::decode_gun_model(f.bytes(),metadata,"synthetic",gun.model,error));
    acvr::GunNode second;second.name="grip_two";second.parent=0;
    for(unsigned i=0;i<4;++i)second.local[i*5]=second.bind_world[i*5]=1;
    second.local[12]=second.bind_world[12]=-.26f;
    gun.model.asset.nodes.push_back(second);gun.reset();
    std::array<float,3> relative{};
    check(acvr::gun_relative_node_position(gun.model.asset,"grip_two","grip",relative));
    check(std::abs(relative[0]-.26f)<.0001f&&relative[1]==0&&relative[2]==0);
    check(!acvr::gun_relative_node_position(gun.model.asset,"grip_two","missing",relative));
    auto p=pose();auto c=config();acvr::GunDraw draw;
    check(gun.draw(p,p,c,1,5,0,draw,{"grip_two"})==ACVR_OK);
    check(std::abs(draw.scene_from_node.back()[12])<.0001f);
    check(std::abs(draw.scene_from_node[0][12]-.26f)<.0001f);
    check(gun.draw(p,p,c,1,5,0,draw,{"missing"})==ACVR_BAD_ARGUMENT);
    // The glove can attach to grip while the gun mounts at grip_two plus an
    // authored rotated/translated local frame. Two-hand aiming uses that same
    // frame and the support attachment offset, not either bare node origin.
    const acvr::GunMount primary{"grip_two",{0,0,.1f},{0,.70710678f,0,.70710678f}};
    const acvr::GunMount support{"grip",{.04f,0,0},{0,0,0,1}};
    check(acvr::gun_relative_mount_position(gun.model.asset,primary,support,relative));
    check(std::abs(relative[0]-.1f)<.0001f&&std::abs(relative[2]-.3f)<.0001f);
    check(gun.draw(p,p,c,1,5,0,draw,primary)==ACVR_OK);
    check(std::abs(draw.scene_from_node.back()[12]-.1f)<.0001f);
    check(std::abs(draw.scene_from_node.back()[8]+1.f)<.0001f);
    check(std::abs(draw.scene_from_node[0][12]-relative[0])<.0001f&&
          std::abs(draw.scene_from_node[0][14]+.04f-relative[2])<.0001f);
    auto invalid=primary;invalid.rotation_xyzw={0,0,0,2};
    check(!acvr::gun_relative_mount_position(gun.model.asset,invalid,support,relative));
    check(gun.draw(p,p,c,1,5,0,draw,invalid)==ACVR_BAD_ARGUMENT);
}
void authored_hand_contract() {
    const std::string data=R"({"schema":"hand-assembly/1","units":"metres","axes":{"up":"+Y","forward":"-Z","handedness":"right"},"quaternion_order":"xyzw","hand_root":"grip","models":{"synthetic":{"hands":{"right":{"asset":"assets/hands/grip-hand-right.glb","node":"grip","translation":[0,0,0],"rotation_xyzw":[0,0,0,1]},"left":{"asset":"assets/hands/grip-hand-left.glb","node":"grip","translation":[0,0,0],"rotation_xyzw":[0,0,0,1]}},"primary_mount_by_hand":{"right":{"node":"grip","translation":[0,0,0],"rotation_xyzw":[0,0,0,1]},"left":{"node":"grip","translation":[0,0,0],"rotation_xyzw":[0,0,0,1]}},"label_anchors":{"grip_two":{"node":"grip_two","local_offset":[0,0,0],"bind_position":[0,0,-0.3]}}}}})";
    acvr::HandAssembly assembly;std::string error;
    check(acvr::decode_hand_assembly(data,"synthetic",assembly,error));
    check(assembly.hands[0].node=="grip"&&assembly.labels.size()==1&&assembly.labels[0].semantic=="grip_two");
    auto bad=nlohmann::json::parse(data);bad["models"]["synthetic"]["hands"]["right"]["asset"]="../escape.glb";
    check(!acvr::decode_hand_assembly(bad.dump(),"synthetic",assembly,error));
    check(assembly.hands[0].asset=="assets/hands/grip-hand-right.glb");
    acvr::GunAsset hand_asset;acvr::GunNode grip;grip.name="grip";hand_asset.nodes.push_back(grip);
    acvr::HandMount mount;acvr::HandMount::FingerPose finger;finger.node="grip";
    mount.finger_pose.push_back(finger);
    check(acvr::validate_hand_pose_nodes(mount,hand_asset,error));
    mount.finger_pose[0].node="typo_pivot";
    check(!acvr::validate_hand_pose_nodes(mount,hand_asset,error)&&error.find("finger-pose")!=std::string::npos);
    acvr::Matrix identity{};for(unsigned i=0;i<4;++i)identity[i*5]=1;
    std::vector<acvr::Matrix> posed;
    check(!acvr::pose_hand_nodes(hand_asset,mount,{}, {},identity,posed));
    const std::string defaults="version='0.1'\n[[rule]]\ngun_model='synthetic'\ncontrol_set='gun-synthetic'\n";
    std::string set_id;
    check(acvr::select_shared_control_set(defaults,"synthetic",set_id,error)&&set_id=="gun-synthetic");
    check(!acvr::select_shared_control_set(defaults,"other",set_id,error));
    check(!acvr::select_shared_control_set("version='0.1'\n[[rule]]\ngun_model='synthetic'\ncontrol_set='../escape'\n",
                                           "synthetic",set_id,error));
    assembly.labels.push_back({"button_a","button_a",{}, {}});
    const std::string controls="version='0.1'\ngun_model='synthetic'\n"
        "[[element]]\nslot=0\nnode='grip_two'\npart='support grip'\nlabel='Cover'\n"
        "binding={control='grip'}\n[[unmapped_part]]\nslot=0\nnode='button_a'\nlabel='A'\n";
    std::vector<acvr::InspectionLabel> labels;
    check(acvr::describe_inspection_labels(assembly,controls,"synthetic",labels,error));
    check(labels.size()==2&&labels[0].part=="support grip"&&
          labels[0].game_action=="Cover (UNCONFIRMED)"&&
          labels[0].controller_button=="GRIP (CANDIDATE)");
    check(labels[1].part=="A"&&labels[1].game_action=="UNCONFIRMED"&&
          labels[1].controller_button=="UNCONFIRMED");
    check(!acvr::describe_inspection_labels(assembly,controls,"other",labels,error));
    auto alternate=nlohmann::json::parse(data);
    auto &model=alternate["models"]["synthetic"];
    model["primary_mount_by_hand"]["left"]={{"node","grip_two"},
        {"translation",{0,0,.1}},{"rotation_xyzw",{0,.7071067811865476,0,.7071067811865476}}};
    model["support_by_primary"]={{"right",{{"node","grip_two"},{"translation",{0,0,0}},
                                                  {"rotation_xyzw",{0,0,0,1}}}},
                                  {"left",{{"node","grip"},{"translation",{.04,0,0}},
                                                  {"rotation_xyzw",{0,0,0,1}}}}};
    check(acvr::decode_hand_assembly(alternate.dump(),"synthetic",assembly,error));
    check(assembly.hands[1].node=="grip"&&assembly.primary_mount_by_hand[1].node=="grip_two"&&
          std::abs(assembly.primary_mount_by_hand[1].translation[2]-.1f)<.0001f&&
          assembly.support_by_primary[0].node=="grip_two"&&assembly.support_by_primary[1].node=="grip");
    Fixture fixture;acvr::GunInstance mounted;
    check(acvr::decode_gun_model(fixture.bytes(),metadata,"synthetic",mounted.model,error));
    acvr::GunNode second_gun;second_gun.name="grip_two";second_gun.parent=0;
    for(unsigned i=0;i<4;++i)second_gun.local[i*5]=second_gun.bind_world[i*5]=1;
    second_gun.local[12]=second_gun.bind_world[12]=-.26f;
    mounted.model.asset.nodes.push_back(second_gun);mounted.reset();
    const auto &selected=assembly.primary_mount_by_hand[1],&supported=assembly.support_by_primary[1];
    const acvr::GunMount primary{selected.node,selected.translation,selected.rotation_xyzw};
    const acvr::GunMount support{supported.node,supported.translation,supported.rotation_xyzw};
    std::array<float,3> relative{};
    check(acvr::gun_relative_mount_position(mounted.model.asset,primary,support,relative));
    check(std::abs(relative[0]-.1f)<.0001f&&std::abs(relative[2]-.3f)<.0001f);
    acvr::GunDraw placed;auto tracked=pose();auto slot=config();
    check(mounted.draw(tracked,tracked,slot,1,5,0,placed,primary)==ACVR_OK);
    check(std::abs(placed.scene_from_node.back()[12]-.1f)<.0001f&&
          std::abs(placed.scene_from_node[0][12]-.1f)<.0001f);
    acvr::GunNode second;second.name="grip_two";hand_asset.nodes.push_back(second);
    check(acvr::validate_hand_gun_nodes(assembly,hand_asset,error));
    hand_asset.nodes.pop_back();
    check(!acvr::validate_hand_gun_nodes(assembly,hand_asset,error));
    auto invalid=alternate;invalid["models"]["synthetic"]["primary_mount_by_hand"].erase("left");
    check(!acvr::decode_hand_assembly(invalid.dump(),"synthetic",assembly,error));
    invalid=alternate;invalid["models"]["synthetic"]["primary_mount_by_hand"]["left"]["node"]="unknown_primary";
    check(!acvr::decode_hand_assembly(invalid.dump(),"synthetic",assembly,error));
    invalid=alternate;invalid["models"]["synthetic"]["primary_mount_by_hand"]["left"]["rotation_xyzw"]={0,0,0,2};
    check(!acvr::decode_hand_assembly(invalid.dump(),"synthetic",assembly,error));
    model["support_by_primary"]["left"]["node"]="unknown_support";
    check(!acvr::decode_hand_assembly(alternate.dump(),"synthetic",assembly,error));
}
void authored_hand_source(const std::filesystem::path &root,const std::string &model){
    auto read=[&](const std::filesystem::path &path){
        std::ifstream in(path,std::ios::binary);check(bool(in));
        return std::string(std::istreambuf_iterator<char>(in),{});
    };
    acvr::HandAssembly assembly;std::string error;
    const bool assembled=acvr::decode_hand_assembly(read(root/"assets"/"hands"/"assembly.json"),model,assembly,error);
    if(!assembled)throw std::runtime_error("hand assembly: "+error);
    check(assembled);
    std::string set_id;std::vector<acvr::InspectionLabel> labels;
    const bool selected=acvr::select_shared_control_set(read(root/"data"/"controls"/"defaults.toml"),model,set_id,error);
    if(!selected)throw std::runtime_error("control defaults: "+error);
    check(selected);
    const bool described=acvr::describe_inspection_labels(assembly,read(root/"data"/"controls"/(set_id+".toml")),
                                                           model,labels,error);
    if(!described)throw std::runtime_error("inspection labels: "+error);
    check(described);
    check(labels.size()==assembly.labels.size());
    for(size_t i=0;i<labels.size();++i){
        check(labels[i].node==assembly.labels[i].node&&!labels[i].part.empty()&&
              !labels[i].game_action.empty()&&!labels[i].controller_button.empty());
        check(labels[i].game_action.find("UNCONFIRMED")!=std::string::npos);
    }
    if(model=="con-pistol-dpad"){
        const auto find=[&](const std::string &node)->const acvr::InspectionLabel &{
            const auto it=std::find_if(labels.begin(),labels.end(),[&](const auto &label){return label.node==node;});
            check(it!=labels.end());return *it;
        };
        for(const auto *node:{"button_a","button_b","button_c"})
            check(find(node).game_action=="UNCONFIRMED"&&find(node).controller_button=="UNCONFIRMED");
        check(find("button_start").controller_button=="A/X (CANDIDATE)");
    }
    const auto gun_bytes=read(root/"assets"/"guns"/(model+".glb"));acvr::GunAsset gun;
    const bool gun_ok=acvr::decode_gun_glb(std::vector<uint8_t>(gun_bytes.begin(),gun_bytes.end()),gun,error);
    if(gun_ok){
        check(acvr::validate_hand_gun_nodes(assembly,gun,error));
    }
    for(const auto &hand:assembly.hands){
        const auto path=root/std::filesystem::u8path(hand.asset);
        const auto glb=read(path);acvr::GunAsset asset;
        check(acvr::decode_hand_glb(std::vector<uint8_t>(glb.begin(),glb.end()),asset,error));
        auto meta=path;meta.replace_extension(".toml");std::vector<acvr::HandMotion> motions;
        check(acvr::decode_hand_motions(read(meta),asset,path.stem().string(),
                                        &hand==&assembly.hands[0]?"right":"left",motions,error));
        check(!acvr::decode_hand_motions(read(meta),asset,"wrong-hand", "right",motions,error));
        check(motions.size()==5);
        check(acvr::validate_hand_pose_nodes(hand,asset,error));
        if(!hand.finger_pose.empty()){
            auto typo=hand;typo.finger_pose.front().node="missing_pivot";
            check(!acvr::validate_hand_pose_nodes(typo,asset,error));
        }
        acvr::Matrix identity{};for(unsigned i=0;i<4;++i)identity[i*5]=1;
        std::vector<acvr::Matrix> neutral,pressed;
        check(acvr::pose_hand_nodes(asset,hand,motions,{},identity,neutral));
        check(acvr::pose_hand_nodes(asset,hand,motions,{1,1,1},identity,pressed));
        check(neutral.size()==asset.nodes.size()&&pressed.size()==neutral.size());
        const auto index=std::find_if(asset.nodes.begin(),asset.nodes.end(),[](const acvr::GunNode &n){return n.name=="pivot_index";});
        check(index!=asset.nodes.end()&&neutral[size_t(index-asset.nodes.begin())]!=pressed[size_t(index-asset.nodes.begin())]);
        for(const auto &pose:hand.finger_pose)if(pose.scale[0]>1.01f){
            const auto node=std::find_if(asset.nodes.begin(),asset.nodes.end(),[&](const acvr::GunNode &n){return n.name==pose.node;});
            const auto &m=neutral[size_t(node-asset.nodes.begin())];
            check(std::hypot(m[0],m[1],m[2])>1.2f);
        }
    }
}
int main(int argc,char **argv) {
    try { load_and_motion(); invalid_metadata_and_files(); logical_drives(); routed_outputs(); mounted_neutral_rest();
        alternate_primary_mount(); authored_hand_contract();
        if(argc==3)authored_hand_source(argv[1],argv[2]); }
    catch(const std::exception &e) {std::cerr<<e.what()<<'\n';return 1;}
    std::cout<<checks<<" model checks passed\n";
}
