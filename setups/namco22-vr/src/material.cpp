// SPDX-License-Identifier: GPL-3.0-only
#include "n22_material.hpp"
#include <algorithm>
#include <cmath>

namespace n22 {
TextureCell decode_texture_cell(uint32_t index,uint8_t lo,uint8_t hi,uint8_t attributes) {
    return {index,static_cast<uint16_t>(lo|(uint16_t(hi)<<8)),
        static_cast<uint8_t>((index&1)?attributes&15:attributes>>4)};
}
acvr_result decode_planar_palette(const std::vector<uint8_t> &bytes,std::vector<uint32_t> &out) {
    if(bytes.size()!=0x18000) return ACVR_BAD_ARGUMENT;
    std::vector<uint32_t> palette(0x8000);
    for(size_t i=0;i<palette.size();++i)
        palette[i]=(uint32_t(bytes[i])<<16)|(uint32_t(bytes[i+0x8000])<<8)|bytes[i+0x10000];
    out=std::move(palette);return ACVR_OK;
}
bool valid_material_vertex(const MaterialVertex &v) {
    return std::isfinite(v.u) && std::isfinite(v.v) && std::isfinite(v.brightness) &&
        std::abs(v.u)<=65536 && std::abs(v.v)<=65536 && v.brightness>=0 && v.brightness<=255;
}
acvr_result validate_material_packet(const MaterialPacket &p) {
    if(p.materials.empty()) return p.cells.empty() && p.tiles.empty() && p.palette.empty()?ACVR_OK:ACVR_BAD_ARGUMENT;
    if(p.addressing!=TileAddressing::Extended17 && p.addressing!=TileAddressing::Fixed16) return ACVR_UNSUPPORTED;
    if(p.materials.size()>4096 || p.cells.size()>4096 || p.tiles.size()>4096 || p.palette.size()!=0x8000) return ACVR_BAD_ARGUMENT;
    uint32_t previous=0;bool first=true;
    for(const auto &cell:p.cells) {
        if(cell.index>=0x100000 || cell.attribute>15 || (!first && cell.index<=previous)) return ACVR_BAD_ARGUMENT;
        first=false;previous=cell.index;
    }
    first=true;
    for(const auto &tile:p.tiles) {
        // Matches this pin's TEXTURE_TOTAL_SIZE (16 MiB / 256). Extended
        // IDs beyond it yield native pen 0, not a lookup into unowned memory.
        if(tile.index>=0x10000 || (!first && tile.index<=previous)) return ACVR_BAD_ARGUMENT;
        first=false;previous=tile.index;
    }
    for(auto rgb:p.palette) if(rgb>0xffffff) return ACVR_BAD_ARGUMENT;
    for(const auto &m:p.materials)
        if(m.colour_word>0xffffff || m.cz_adjust>0xffffff || m.texbank>15 || m.cmode>15 || m.objectflags>7) return ACVR_BAD_ARGUMENT;
    return ACVR_OK;
}
acvr_result sample_material(const MaterialPacket &p,uint32_t id,double u,double v,double bri,uint32_t &rgb) {
    if(id>=p.materials.size() || p.palette.size()!=0x8000 || !std::isfinite(u) || !std::isfinite(v) ||
       !std::isfinite(bri) || std::abs(u)>65536 || std::abs(v)>65536 || bri<0 || bri>255) return ACVR_BAD_ARGUMENT;
    const auto &m=p.materials[id];uint32_t pen=0;
    if(m.objectflags) {
        const uint32_t col=(m.colour_word>>8)&255;
        if(m.objectflags&6) {pen=m.cz_adjust&0x7fff;bri=64;}
        else pen=(((col&127)<<8)+(((m.cz_adjust>>16)&127)&(col|31)))&0x7fff;
    } else {
        if(p.addressing!=TileAddressing::Extended17 && p.addressing!=TileAddressing::Fixed16) return ACVR_UNSUPPORTED;
        const auto iu=static_cast<uint32_t>(static_cast<int32_t>(std::floor(u)))&0xfff;
        const auto iv=(static_cast<uint32_t>(static_cast<int32_t>(std::floor(v)))&0xfff)|(uint32_t(m.texbank)<<12);
        const uint32_t index=((iv&0xfff0)<<4)|((iu&0xff0)>>4);
        const auto cell=std::lower_bound(p.cells.begin(),p.cells.end(),index,[](const TextureCell &a,uint32_t b){return a.index<b;});
        if(cell==p.cells.end() || cell->index!=index) return ACVR_BAD_ARGUMENT;
        uint32_t tile=cell->tile;
        if((cell->attribute&1) && p.addressing==TileAddressing::Extended17) tile|=0x10000;
        uint8_t raw=0;
        if(tile<0x10000) {
            const auto pixels=std::lower_bound(p.tiles.begin(),p.tiles.end(),tile,[](const TextureTile &a,uint32_t b){return a.index<b;});
            if(pixels==p.tiles.end() || pixels->index!=tile) return ACVR_BAD_ARGUMENT;
            uint32_t x=iu&15,y=iv&15;
            if(cell->attribute&4) x=15-x;
            if(cell->attribute&2) y=15-y;
            if(cell->attribute&8) std::swap(x,y);
            raw=pixels->pens[y*16+x];
        }
        uint32_t offset=0,shift=0,mask=255;
        if(m.cmode&4) {offset=0xec+((m.cmode&8)<<1);shift=2*(~m.cmode&3);mask=3;}
        else if(m.cmode&2) {offset=0xe0+((m.cmode&8)<<1);shift=4*(~m.cmode&1);mask=15;}
        pen=((m.colour_word>>8)&127)*256+offset+((raw>>shift)&mask);
    }
    const uint32_t base=p.palette[pen];rgb=0;
    for(unsigned shift: {16u,8u,0u}) {
        const auto channel=static_cast<uint32_t>(std::clamp(double((base>>shift)&255)*bri/64.,0.,255.));
        rgb|=channel<<shift;
    }
    return ACVR_OK;
}
}
