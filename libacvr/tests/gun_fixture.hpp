// SPDX-License-Identifier: MIT
#pragma once
#include <nlohmann/json.hpp>
#include <vector>
#include <cstdint>
#include <cstring>
using Json=nlohmann::json;
void word(std::vector<uint8_t> &b,uint32_t v) { for(unsigned i=0;i<4;++i) b.push_back(uint8_t(v>>(i*8))); }
void real(std::vector<uint8_t> &b,float f) { uint32_t v; std::memcpy(&v,&f,4); word(b,v); }
struct Fixture {
    Json doc=Json::parse(R"({
      "asset":{"version":"2.0"},"scene":0,"scenes":[{"nodes":[0]}],
      "nodes":[
        {"name":"grip","children":[1,2,4]},
        {"name":"muzzle","translation":[0,0.02,-0.2]},
        {"name":"LOD0","children":[3]},
        {"name":"body_mesh","mesh":0,"translation":[0.01,0,0]},
        {"name":"LOD1","children":[5]},
        {"name":"body_mesh_lod1","mesh":0}],
      "materials":[{"name":"body","pbrMetallicRoughness":{"baseColorFactor":[0.5,0.6,0.7,1]}}],
      "meshes":[{"primitives":[{"attributes":{"POSITION":0},"indices":1,"material":0}]}],
      "buffers":[{"byteLength":42}],
      "bufferViews":[{"buffer":0,"byteOffset":0,"byteLength":36},{"buffer":0,"byteOffset":36,"byteLength":6}],
      "accessors":[{"bufferView":0,"componentType":5126,"count":3,"type":"VEC3"},
                   {"bufferView":1,"componentType":5123,"count":3,"type":"SCALAR"}]
    })");
    std::vector<uint8_t> bin;
    Fixture() {
        for(float f:{0.f,0.f,0.f, .1f,0.f,0.f, 0.f,.1f,0.f}) real(bin,f);
        bin.insert(bin.end(),{0,0,1,0,2,0});
    }
    std::vector<uint8_t> bytes() const {
        auto j=doc.dump(); while(j.size()%4) j+=' ';
        auto binary=bin; while(binary.size()%4) binary.push_back(0);
        std::vector<uint8_t> out;
        word(out,0x46546c67); word(out,2); word(out,uint32_t(28+j.size()+binary.size()));
        word(out,uint32_t(j.size())); word(out,0x4e4f534a); out.insert(out.end(),j.begin(),j.end());
        word(out,uint32_t(binary.size())); word(out,0x004e4942); out.insert(out.end(),binary.begin(),binary.end());
        return out;
    }
};
