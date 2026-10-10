// SPDX-License-Identifier: MIT
#pragma once
#include <array>
#include <cstdint>
#include <string>
#include <vector>

namespace acvr {
using Matrix = std::array<float,16>;
struct GunVertex {
    std::array<float,3> position{}, normal{};
    std::array<float,4> colour{1,1,1,1};
};
struct GunMaterial { std::string name; std::array<float,4> base_colour{1,1,1,1}; };
struct GunPrimitive {
    uint32_t node=0, material=0, lod=0;
    bool has_normals=false;
    std::vector<GunVertex> vertices;
    std::vector<uint32_t> indices;
};
struct GunNode { std::string name; int32_t parent=-1; Matrix local{}, bind_world{}; };
struct GunAsset {
    std::vector<GunNode> nodes;
    std::vector<GunMaterial> materials;
    std::vector<GunPrimitive> primitives;
    uint32_t grip=0,muzzle=0,lod_count=0;
    std::array<uint32_t,2> triangles{};
};
// Decodes an in-memory, self-contained GLB. No filesystem, network or graphics.
// Output remains unchanged on failure. Named bind transforms never animate.
bool decode_gun_glb(const std::vector<uint8_t> &, GunAsset &, std::string &error) noexcept;
}
