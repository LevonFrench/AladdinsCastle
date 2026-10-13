// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include "n22_scene.hpp"
namespace n22 {
inline constexpr uint32_t MaxTextureEdge=1024,MaxLeaseTextures=256,MaxGlTriangles=65536;
inline constexpr size_t MaxLeaseTextureBytes=32*1024*1024;
struct TextureRectangle {
    uint32_t material=0,width=0,height=0;
    int32_t min_u=0,min_v=0;
    std::vector<uint8_t> rgba;
};
struct TexturePlan {
    std::vector<TextureRectangle> rectangles;
    std::vector<uint32_t> triangle_textures; // NoMaterial for explicit flat triangles
    size_t bytes=0;
    uint32_t fog_texture=NoMaterial; // private complete white unit-1 dummy
};
// All checked bounds and sparse samples succeed before replacing out.
// Exact texels, no centre substitution, scaling tiers, GPU or filesystem calls.
acvr_result prepare_texture_plan(const Frame &,TexturePlan &out);
}
