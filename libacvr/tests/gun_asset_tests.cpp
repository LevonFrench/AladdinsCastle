// SPDX-License-Identifier: MIT
#include "gun_asset.hpp"
#include <nlohmann/json.hpp>
#include <cmath>
#include <cstring>
#include <functional>
#include <iostream>
#include <limits>
#include <stdexcept>

using Json=nlohmann::json;
unsigned checks=0;
void check(bool ok) { ++checks; if(!ok) throw std::runtime_error("asset check "+std::to_string(checks)); }
#include "gun_fixture.hpp"
void reject(const Fixture &f) {
    acvr::GunAsset asset; asset.grip=987; std::string error;
    check(!acvr::decode_gun_glb(f.bytes(),asset,error));
    check(asset.grip==987 && asset.nodes.empty() && !error.empty());
}
void valid_assets() {
    Fixture f; acvr::GunAsset asset; std::string error="stale";
    check(acvr::decode_gun_glb(f.bytes(),asset,error));
    check(error.empty() && asset.nodes.size()==6 && asset.primitives.size()==2);
    check(asset.grip==0 && asset.muzzle==1 && asset.lod_count==2 && asset.triangles[0]==1 && asset.triangles[1]==1);
    check(asset.primitives[0].vertices[1].position[0]==.1f && asset.primitives[1].indices[2]==2);
    check(asset.nodes[3].bind_world[12]==.01f && asset.materials[0].base_colour[0]==.5f);
    const auto muzzle=asset.nodes[asset.muzzle].bind_world;
    asset.nodes[3].local[14]+=.04f; // visual recoil cannot mutate the static aim anchor
    check(asset.nodes[asset.muzzle].bind_world==muzzle);
    for(uint32_t type:{5121u,5123u,5125u}) {
        Fixture indexed; indexed.bin.resize(36);
        const auto width=type==5121?1u:type==5123?2u:4u;
        for(uint32_t i=0;i<3;++i) for(unsigned n=0;n<width;++n) indexed.bin.push_back(uint8_t(i>>(n*8)));
        indexed.doc["accessors"][1]["componentType"]=type;
        indexed.doc["bufferViews"][1]["byteLength"]=width*3;
        indexed.doc["buffers"][0]["byteLength"]=indexed.bin.size();
        check(acvr::decode_gun_glb(indexed.bytes(),asset,error) && asset.primitives[0].indices[2]==2);
    }
    f.doc["meshes"][0]["primitives"][0].erase("indices");
    check(acvr::decode_gun_glb(f.bytes(),asset,error) && asset.primitives[0].indices.size()==3);
    f.doc["nodes"][2]["name"]="geometry"; f.doc["nodes"][4]["name"]="geometry_low";
    check(acvr::decode_gun_glb(f.bytes(),asset,error) && asset.lod_count==1 && asset.triangles[0]==2);
}
void interleaved_and_colours() {
    Fixture f; f.bin.clear();
    for(unsigned i=0;i<3;++i) {
        real(f.bin,float(i)); real(f.bin,0); real(f.bin,-.1f); word(f.bin,0x44332211);
    }
    f.bin.insert(f.bin.end(),{0,0,1,0,2,0,0,0});
    f.doc["bufferViews"][0]["byteLength"]=48; f.doc["bufferViews"][0]["byteStride"]=16;
    f.doc["bufferViews"][1]["byteOffset"]=48;
    f.doc["bufferViews"].push_back({{"buffer",0},{"byteOffset",56},{"byteLength",12}});
    f.bin.insert(f.bin.end(),{255,0,128,255, 0,255,0,128, 0,0,255,255});
    f.doc["accessors"].push_back({{"bufferView",2},{"componentType",5121},{"count",3},{"type","VEC4"},{"normalized",true}});
    f.doc["meshes"][0]["primitives"][0]["attributes"]["COLOR_0"]=2;
    f.doc["buffers"][0]["byteLength"]=f.bin.size();
    acvr::GunAsset asset; std::string error;
    check(acvr::decode_gun_glb(f.bytes(),asset,error));
    check(asset.primitives[0].vertices[2].position[0]==2 && asset.primitives[0].vertices[0].colour[0]==1);
    check(std::abs(asset.primitives[0].vertices[1].colour[3]-128.f/255.f)<.0001f);
    f.doc["accessors"][2]["normalized"]=false; reject(f);
}
void malformed() {
    const std::vector<std::function<void(Fixture &)>> edits{
        [](Fixture &f){f.doc["buffers"][0]["uri"]="external.bin";},
        [](Fixture &f){f.doc["extensionsRequired"]={"unsupported"};},
        [](Fixture &f){f.doc["images"]=Json::array({{{"uri","image.png"}}});},
        [](Fixture &f){f.doc["nodes"][1]["name"]="grip";},
        [](Fixture &f){f.doc["nodes"][0]["translation"]={1,0,0};},
        [](Fixture &f){f.doc["nodes"][1]["translation"]={0,0,1};},
        [](Fixture &f){f.doc["nodes"][1]["rotation"]={0,1,0,0};},
        [](Fixture &f){f.doc["nodes"][0]["children"]={2,4}; f.doc["nodes"][3]["children"]={1};},
        [](Fixture &f){f.doc["nodes"][3]["children"]={2};},
        [](Fixture &f){f.doc["nodes"][2]["children"]=Json::array();},
        [](Fixture &f){f.doc["nodes"][3]["scale"]={0,1,1};},
        [](Fixture &f){f.doc["nodes"][3].erase("translation");f.doc["nodes"][3]["matrix"]={0,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1};},
        [](Fixture &f){f.doc["nodes"][3]["rotation"]={0,0,0,2};},
        [](Fixture &f){f.doc["nodes"][3]["mesh"]=5;},
        [](Fixture &f){f.doc["bufferViews"][0]["byteLength"]=35;},
        [](Fixture &f){f.doc["bufferViews"][0]["byteStride"]=8;},
        [](Fixture &f){f.doc["accessors"][0]["byteOffset"]=1;},
        [](Fixture &f){f.doc["accessors"][0]["count"]=100001;},
        [](Fixture &f){f.doc["accessors"][0]["sparse"]={};},
        [](Fixture &f){f.doc["meshes"][0]["primitives"][0]["mode"]=1;},
        [](Fixture &f){f.doc["meshes"][0]["primitives"][0]["material"]=9;},
        [](Fixture &f){f.doc["materials"][0]["name"]="invalid";},
        [](Fixture &f){f.bin[40]=3;},
        [](Fixture &f){f.bin[0]=0;f.bin[1]=0;f.bin[2]=128;f.bin[3]=127;},
        [](Fixture &f){f.doc["bufferViews"][0]["byteLength"]=uint64_t(1)<<40;},
        [](Fixture &f){f.doc["accessors"][0]["bufferView"]=-1;}
    };
    for(const auto &edit:edits) { Fixture f; edit(f); reject(f); }
    Fixture f; const auto good=f.bytes();
    for(size_t length=0;length<good.size();++length) {
        auto truncated=good; truncated.resize(length); acvr::GunAsset asset; std::string error;
        check(!acvr::decode_gun_glb(truncated,asset,error));
    }
    auto bad=good; bad[4]=3; acvr::GunAsset asset; std::string error;
    check(!acvr::decode_gun_glb(bad,asset,error));
    auto *cursor=&f.doc["extras"];
    for(unsigned i=0;i<70;++i) { (*cursor)["nested"]=Json::object(); cursor=&(*cursor)["nested"]; }
    reject(f);
    Fixture repeated;
    repeated.doc["meshes"][0]["primitives"]=Json::array();
    const auto primitive=Fixture{}.doc["meshes"][0]["primitives"][0];
    for(unsigned i=0;i<2049;++i) repeated.doc["meshes"][0]["primitives"].push_back(primitive);
    reject(repeated);
}
int main() {
    try { valid_assets(); interleaved_and_colours(); malformed(); }
    catch(const std::exception &e) { std::cerr<<e.what()<<'\n'; return 1; }
    std::cout<<checks<<" gun asset checks passed\n"; return 0;
}
