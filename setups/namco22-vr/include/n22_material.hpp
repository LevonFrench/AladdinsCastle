// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include "acvr.h"
#include <array>
#include <cstdint>
#include <vector>

namespace n22 {
inline constexpr uint32_t NoMaterial=UINT32_MAX;
enum class TileAddressing {Unknown,Extended17,Fixed16};
struct TextureCell {uint32_t index=0;uint16_t tile=0;uint8_t attribute=0;};
struct TextureTile {uint32_t index=0;std::array<uint8_t,256> pens{};};
struct Material {
    uint32_t colour_word=0,cz_adjust=0;
    uint8_t texbank=0,cmode=0,objectflags=0;
};
struct MaterialVertex {
    float u=.5f,v=.5f; // texel-centre coordinates, before any perspective division
    float brightness=64;
};
// Sorted sparse native map cells/tiles, bounded to 4096 each. All bytes are
// owned values; no GL handles, atlas offsets, content paths or native pointers.
// Texture/palette/shade only; CZ/fades/gamma/sprites/text are deferred explicitly.
struct MaterialPacket {
    TileAddressing addressing=TileAddressing::Unknown;
    std::vector<TextureCell> cells;
    std::vector<TextureTile> tiles;
    std::vector<uint32_t> palette; // exactly 128*256 packed RGB entries
    std::vector<Material> materials;
};
TextureCell decode_texture_cell(uint32_t index,uint8_t tile_lo,uint8_t tile_hi,uint8_t packed_attribute);
acvr_result decode_planar_palette(const std::vector<uint8_t> &,std::vector<uint32_t> &);
acvr_result validate_material_packet(const MaterialPacket &);
bool valid_material_vertex(const MaterialVertex &);
// Caller validates packet once before sampling. Missing referenced data returns
// BAD_ARGUMENT rather than painting a made-up texture. Polygon pen 0 is opaque.
acvr_result sample_material(const MaterialPacket &,uint32_t material,double u,double v,double brightness,uint32_t &rgb);
}
