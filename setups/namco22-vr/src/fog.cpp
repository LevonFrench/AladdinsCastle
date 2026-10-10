// SPDX-License-Identifier: GPL-3.0-only
#include "n22_fog.hpp"
#include "n22_worker.hpp"
#include <algorithm>
#include <utility>
namespace n22 {
acvr_result copy_super22_fog(const VideoSnapshot &source,FogState &out) {
    if(!source.tick || source.banks[Mixer].size()!=bank_sizes[Mixer]) return ACVR_BAD_ARGUMENT;
    FogState state;state.policy=FogPolicy::Super22Table;state.tick=source.tick;state.attributes=source.czattr;
    for(size_t c=0;c<3;++c) state.rgb[c]=source.banks[Mixer][5+c];
    // Pinned generic fog_hw.c recalc_bank: a skipped non-increasing word
    // does not alter prev or extrema. Preserve that source behavior exactly.
    for(size_t bank=0;bank<4;++bank) {
        auto &table=state.tables[bank];
        const int reverse=((state.attributes[4]>>(bank*4))&2)?255:0;
        int small=8192,small_factor=reverse,large=0,large_factor=reverse^255,prev=0;
        for(int i=0;i<256;++i) {
            const int factor=i^reverse;
            const int value=std::min(int(source.czram[bank][static_cast<size_t>(factor)]),8192);
            if(i>0) {
                if(prev>=value) continue;
                std::fill(table.begin()+prev,table.begin()+value,static_cast<uint8_t>(factor));
            }
            if(value<small) {small=value;small_factor=factor;}
            if(value>large) {large=value;large_factor=factor;}
            prev=value;
        }
        std::fill(table.begin(),table.begin()+small,static_cast<uint8_t>(small_factor));
        std::fill(table.begin()+large,table.end(),static_cast<uint8_t>(large_factor));
    }
    out=std::move(state);return ACVR_OK;
}
acvr_result validate_fog_binding(const FogState &state,const FogQuad &quad,uint64_t tick) {
    if(state.policy!=FogPolicy::Absent && state.policy!=FogPolicy::System22Constant && state.policy!=FogPolicy::Super22Table) return ACVR_UNSUPPORTED;
    if(state.policy==FogPolicy::Absent) return quad.provided?ACVR_UNSUPPORTED:ACVR_OK;
    if(!tick || state.tick!=tick || !quad.provided || quad.tick!=tick || quad.colour_word>0xffffff ||
       quad.cz_adjust>0xffffff || quad.cz_type>3) return ACVR_BAD_ARGUMENT;
    if(state.policy==FogPolicy::Super22Table) {
        if(!quad.has_native_depth || quad.constant_provided) return ACVR_BAD_ARGUMENT;
    } else {
        if(!quad.constant_provided) return ACVR_UNSUPPORTED;
        if(quad.constant_alpha<-1 || quad.constant_alpha>255) return ACVR_BAD_ARGUMENT;
    }
    return ACVR_OK;
}
acvr_result decide_fog(const FogState &state,const FogQuad &quad,uint32_t vertex,FogDecision &out) {
    if(vertex>=3) return ACVR_BAD_ARGUMENT;
    if(auto r=validate_fog_binding(state,quad,state.tick);r!=ACVR_OK) return r;
    FogDecision result;result.policy=state.policy;
    if(state.policy==FogPolicy::Absent) {out=result;return ACVR_OK;}
    if(state.policy==FogPolicy::System22Constant) {
        result.rgb=quad.constant_rgb;
        if(!(quad.cz_adjust&0x800000)) result.alpha=static_cast<uint8_t>(quad.constant_alpha<0?255:quad.constant_alpha);
        result.enabled=result.alpha<255;out=result;return ACVR_OK;
    }
    result.rgb=state.rgb;
    if((quad.colour_word&0x8000) || (quad.cz_adjust&0x800000)) {out=result;return ACVR_OK;}
    const int bank=(state.attributes[6]>>(quad.cz_type*2))&3;
    result.bank=bank;
    if(!((state.attributes[4]>>(bank*4))&4)) {out=result;return ACVR_OK;}
    const int word=state.attributes[static_cast<size_t>(bank)];
    result.delta=(word&0x8000)?((word|0xff00)-0x10000):(word&255);
    const int32_t z=quad.native_depth[vertex];
    const size_t cz=static_cast<size_t>(z<=0?0:std::min(z>>8,8191));
    const int factor=int(state.tables[static_cast<size_t>(bank)][cz])+result.delta;
    result.alpha=static_cast<uint8_t>(factor<=0?255:255-std::min(factor,255));
    result.enabled=result.alpha<255;out=result;return ACVR_OK;
}
}
