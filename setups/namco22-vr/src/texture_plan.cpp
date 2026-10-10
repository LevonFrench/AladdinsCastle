// SPDX-License-Identifier: GPL-3.0-only
#include "n22_texture_plan.hpp"
#include <algorithm>
#include <cmath>
#include <map>
#include <tuple>
namespace n22 {
acvr_result prepare_texture_plan(const Frame &frame,TexturePlan &out) {
    if(auto r=validate_material_packet(frame.materials);r!=ACVR_OK) return r;
    if(frame.triangles.size()>MaxGlTriangles) return ACVR_UNSUPPORTED;
    TexturePlan plan;plan.triangle_textures.resize(frame.triangles.size(),NoMaterial);
    using Key=std::tuple<uint32_t,int32_t,int32_t,uint32_t,uint32_t>;
    std::map<Key,uint32_t> keys;
    for(size_t i=0;i<frame.triangles.size();++i) {
        const auto &t=frame.triangles[i];
        if(t.material==NoMaterial || t.layer==Layer::GunFlash) continue;
        if(t.material>=frame.materials.materials.size()) return ACVR_BAD_ARGUMENT;
        for(const auto &v:t.attributes) if(!valid_material_vertex(v)) return ACVR_BAD_ARGUMENT;
        int32_t min_u=0,min_v=0;uint32_t width=1,height=1;
        if(!frame.materials.materials[t.material].objectflags) {
            double u0=t.attributes[0].u,u1=u0,v0=t.attributes[0].v,v1=v0;
            for(const auto &v:t.attributes) {u0=std::min(u0,double(v.u));u1=std::max(u1,double(v.u));v0=std::min(v0,double(v.v));v1=std::max(v1,double(v.v));}
            min_u=static_cast<int32_t>(std::floor(u0));min_v=static_cast<int32_t>(std::floor(v0));
            const int64_t w=static_cast<int64_t>(std::floor(u1))-min_u+1,h=static_cast<int64_t>(std::floor(v1))-min_v+1;
            if(w<=0 || h<=0 || w>MaxTextureEdge || h>MaxTextureEdge) return ACVR_UNSUPPORTED;
            width=static_cast<uint32_t>(w);height=static_cast<uint32_t>(h);
        }
        const Key key{t.material,min_u,min_v,width,height};auto found=keys.find(key);
        if(found!=keys.end()) {plan.triangle_textures[i]=found->second;continue;}
        const uint64_t bytes=uint64_t(width)*height*4;
        if(plan.rectangles.size()>=MaxLeaseTextures || bytes>MaxLeaseTextureBytes ||
           plan.bytes>MaxLeaseTextureBytes-static_cast<size_t>(bytes)) return ACVR_UNSUPPORTED;
        TextureRectangle rect;rect.material=t.material;rect.min_u=min_u;rect.min_v=min_v;rect.width=width;rect.height=height;
        const auto id=static_cast<uint32_t>(plan.rectangles.size());keys.emplace(key,id);plan.triangle_textures[i]=id;
        plan.bytes+=static_cast<size_t>(bytes);plan.rectangles.push_back(std::move(rect));
    }
    // Check every extent/key and the complete lease budget before pixel allocation.
    for(auto &rect:plan.rectangles) {
        const auto width=rect.width,height=rect.height;
        const auto bytes=uint64_t(width)*height*4;
        rect.rgba.resize(static_cast<size_t>(bytes));
        for(uint32_t y=0;y<height;++y) for(uint32_t x=0;x<width;++x) {
            // The sampler floors U/V. The integer cell coordinate represents
            // the same pen as its centre and stays inside inclusive +65536.
            uint32_t rgb=0;const auto r=sample_material(frame.materials,rect.material,double(rect.min_u)+x,double(rect.min_v)+y,64,rgb);
            if(r!=ACVR_OK) return r;
            const size_t at=(size_t(y)*width+x)*4;
            rect.rgba[at]=static_cast<uint8_t>(rgb>>16);rect.rgba[at+1]=static_cast<uint8_t>(rgb>>8);
            rect.rgba[at+2]=static_cast<uint8_t>(rgb);rect.rgba[at+3]=255;
        }
    }
    out=std::move(plan);return ACVR_OK;
}
}
