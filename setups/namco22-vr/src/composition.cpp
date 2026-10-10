// SPDX-License-Identifier: GPL-3.0-only
#include "n22_composition.hpp"
#include "n22_scene.hpp"
#include "n22_worker.hpp"
#include <algorithm>
#include <cmath>
#include <cstring>
#include <limits>
#include <utility>
namespace n22 {
namespace {
bool charge(size_t &used,size_t count,size_t size,size_t maximum) {
    if(count>maximum/size || used>maximum-count*size) return false;
    used+=count*size;return true;
}
CompositionStage expected_stage(CompositionKind kind) {
    if(kind==CompositionKind::Polygon) return CompositionStage::PolygonPostShadeFogPolyfade;
    if(kind==CompositionKind::Sprite) return CompositionStage::SpritePostFogPreOwnFade;
    return CompositionStage::TextPreOwnFadeAlphaResolved;
}
bool finite_eye(const acvr_eye &e) {
    for(auto v:e.view_from_scene) if(!std::isfinite(v)) return false;
    for(auto v:e.projection_from_view) if(!std::isfinite(v)) return false;
    return true;
}
uint32_t pack(const std::array<uint8_t,3> &c) {return (uint32_t(c[0])<<16)|(uint32_t(c[1])<<8)|c[2];}
uint8_t channel(uint32_t c,size_t k) {return static_cast<uint8_t>(c>>((2-k)*8));}
uint32_t fade_colour(uint32_t c,const CompositionMixer &m,bool text) {
    std::array<uint8_t,3> out{};
    for(size_t k=0;k<3;++k) {
        const uint32_t value=uint32_t(channel(c,k))*(255u-m.factor)+uint32_t(m.fade[k])*(text?m.factor:1u+m.factor);
        out[k]=static_cast<uint8_t>(value/(text?255u:256u));
    }
    return pack(out);
}
uint32_t alpha_colour(uint32_t src,uint32_t dst,uint8_t alpha) {
    std::array<uint8_t,3> out{};
    for(size_t k=0;k<3;++k) out[k]=static_cast<uint8_t>((uint32_t(channel(src,k))*alpha+uint32_t(channel(dst,k))*(255u-alpha))/255u);
    return pack(out);
}
}
acvr_result copy_super22_composition_mixer(const VideoSnapshot &source,CompositionMixer &out) {
    if(!source.tick || source.banks[Mixer].size()!=bank_sizes[Mixer]) return ACVR_BAD_ARGUMENT;
    CompositionMixer mix;mix.tick=source.tick;const auto &bytes=source.banks[Mixer];
    for(size_t c=0;c<3;++c) {
        mix.background[c]=bytes[8+c];mix.fade[c]=bytes[0x16+c];
        std::copy_n(bytes.begin()+static_cast<ptrdiff_t>((c+1)*256),256,mix.gamma[c].begin());
    }
    mix.factor=bytes[0x19];mix.flags=bytes[0x1a];out=mix;return ACVR_OK;
}
acvr_result validate_composition_plan(const CompositionPlan &plan,uint64_t tick) {
    if(plan.policy==CompositionPolicy::Absent) {
        if(plan.tick || plan.mixer.tick || !plan.items.empty() || std::any_of(plan.declarations.begin(),plan.declarations.end(),[](auto d){return d!=CompositionDeclaration::Missing;})) return ACVR_BAD_ARGUMENT;
        return ACVR_OK;
    }
    if(plan.policy!=CompositionPolicy::Super22SourceOrder) return ACVR_UNSUPPORTED;
    if(!tick || plan.tick!=tick || plan.mixer.tick!=tick) return ACVR_BAD_ARGUMENT;
    if(plan.mixer.flags&~3u) return ACVR_UNSUPPORTED;
    if(plan.items.size()>MaxCompositionItems || plan.items.capacity()>MaxCompositionInputBytes/sizeof(CompositionItem)) return ACVR_UNSUPPORTED;
    try {
        std::array<size_t,3> counts{};
        std::vector<std::pair<uint32_t,size_t>> ids;
        std::vector<std::pair<uint32_t,uint32_t>> polygons;
        std::vector<const CompositionItem *> sprites;
        for(size_t i=0;i<plan.items.size();++i) {
            const auto &item=plan.items[i];
            if(item.kind!=CompositionKind::Polygon && item.kind!=CompositionKind::Sprite && item.kind!=CompositionKind::Text) return ACVR_UNSUPPORTED;
            if(item.stage!=expected_stage(item.kind)) return ACVR_UNSUPPORTED;
            if(item.tick!=tick || item.id==UINT32_MAX || item.priority>0xffffff) return ACVR_BAD_ARGUMENT;
            ++counts[static_cast<size_t>(item.kind)];ids.emplace_back(item.id,i);
            if(item.kind==CompositionKind::Polygon) {
                if(!item.order_provided || item.sprite_fade || item.prioverchar) return ACVR_BAD_ARGUMENT;
                polygons.emplace_back(item.priority,item.order);
            } else if(item.kind==CompositionKind::Sprite) {
                if(!item.order_provided && item.order) return ACVR_BAD_ARGUMENT;
                sprites.push_back(&item);
            } else if(item.priority || item.order || item.order_provided || item.sprite_fade || item.prioverchar) return ACVR_BAD_ARGUMENT;
        }
        for(size_t k=0;k<3;++k) {
            if(plan.declarations[k]==CompositionDeclaration::Missing) return ACVR_UNSUPPORTED;
            if(plan.declarations[k]!=CompositionDeclaration::Empty && plan.declarations[k]!=CompositionDeclaration::Complete) return ACVR_UNSUPPORTED;
            if((plan.declarations[k]==CompositionDeclaration::Empty)!=(counts[k]==0)) return ACVR_BAD_ARGUMENT;
        }
        // The pin has one decoded text layer. Multiple independently ordered
        // text items would invent a glyph-order/spot/shade policy.
        if(counts[2]>1) return ACVR_UNSUPPORTED;
        std::sort(ids.begin(),ids.end());
        for(size_t i=1;i<ids.size();++i) if(ids[i].first==ids[i-1].first) return ACVR_BAD_ARGUMENT;
        std::sort(polygons.begin(),polygons.end());
        for(size_t i=1;i<polygons.size();++i) if(polygons[i]==polygons[i-1]) return ACVR_BAD_ARGUMENT;
        std::sort(sprites.begin(),sprites.end(),[](auto a,auto b){return std::make_pair(a->priority,a->order)<std::make_pair(b->priority,b->order);});
        for(size_t i=1;i<sprites.size();++i) if(sprites[i]->priority==sprites[i-1]->priority) {
            if(!sprites[i]->order_provided || !sprites[i-1]->order_provided) return ACVR_UNSUPPORTED;
            if(sprites[i]->order==sprites[i-1]->order) return ACVR_BAD_ARGUMENT;
        }
        return ACVR_OK;
    } catch(...) {return ACVR_ERROR;}
}
acvr_result compose_cpu(const Frame &frame,const acvr_eye &eye,const CompositionEyeSpans &input,Image &output) {
    if(auto r=validate_composition_plan(frame.composition,frame.id);r!=ACVR_OK) return r;
    if(frame.composition.policy==CompositionPolicy::Absent) return ACVR_UNSUPPORTED;
    const auto &bound=input.eye;
    if(eye.size<sizeof(eye) || eye.version!=ACVR_STRUCT_VERSION || bound.size<sizeof(bound) || bound.version!=ACVR_STRUCT_VERSION) return ACVR_BAD_VERSION;
    if(input.frame_id!=frame.id) return ACVR_BAD_STATE;
    if(eye.eye_index>1 || eye.array_layer || eye.rect_x<0 || eye.rect_y<0 || !eye.rect_width || !eye.rect_height || !finite_eye(eye)) return ACVR_BAD_ARGUMENT;
    if(bound.eye_index!=eye.eye_index || bound.array_layer!=eye.array_layer || bound.rect_x!=eye.rect_x || bound.rect_y!=eye.rect_y || bound.rect_width!=eye.rect_width || bound.rect_height!=eye.rect_height ||
       std::memcmp(bound.view_from_scene,eye.view_from_scene,sizeof(eye.view_from_scene)) || std::memcmp(bound.projection_from_view,eye.projection_from_view,sizeof(eye.projection_from_view))) return ACVR_BAD_STATE;
    if(!output.width || !output.height || output.width>8192 || output.height>8192 || uint64_t(eye.rect_x)+eye.rect_width>output.width || uint64_t(eye.rect_y)+eye.rect_height>output.height ||
       output.rgb.size()!=uint64_t(output.width)*output.height || output.depth.size()!=output.rgb.size()) return ACVR_BAD_ARGUMENT;
    const uint64_t pixels=uint64_t(eye.rect_width)*eye.rect_height;
    if(eye.rect_width>MaxCompositionEdge || eye.rect_height>MaxCompositionEdge || pixels>MaxCompositionPixels) return ACVR_UNSUPPORTED;
    const auto &plan=frame.composition;
    if(input.items.size()!=plan.items.size()) return ACVR_BAD_ARGUMENT;
    size_t bytes=0,work=0,spans=0;
    if(!charge(bytes,1,sizeof(CompositionPlan),MaxCompositionInputBytes) || !charge(bytes,plan.items.capacity(),sizeof(CompositionItem),MaxCompositionInputBytes) ||
       !charge(bytes,input.items.capacity(),sizeof(CompositionEyeItem),MaxCompositionInputBytes) || !charge(work,static_cast<size_t>(pixels),9,MaxCompositionWorkBytes) ||
       !charge(work,plan.items.size(),sizeof(size_t)*2+sizeof(std::pair<uint32_t,size_t>),MaxCompositionWorkBytes)) return ACVR_UNSUPPORTED;
    try {
        std::vector<std::pair<uint32_t,size_t>> ids;ids.reserve(plan.items.size());
        for(size_t i=0;i<plan.items.size();++i) ids.emplace_back(plan.items[i].id,i);
        std::sort(ids.begin(),ids.end());std::vector<size_t> payload(plan.items.size(),SIZE_MAX);
        // All descriptor/dimension/count/memory checks precede pixel allocation.
        for(size_t i=0;i<input.items.size();++i) {
            const auto &src=input.items[i];
            const auto found=std::lower_bound(ids.begin(),ids.end(),std::make_pair(src.id,size_t{0}));
            if(found==ids.end() || found->first!=src.id || payload[found->second]!=SIZE_MAX) return ACVR_BAD_ARGUMENT;
            const auto &item=plan.items[found->second];
            if(src.tick!=frame.id) return ACVR_BAD_ARGUMENT;
            if(src.stage!=item.stage) return ACVR_UNSUPPORTED;
            payload[found->second]=i;
            if(src.spans.size()>MaxCompositionSpans-spans || !charge(bytes,src.spans.capacity(),sizeof(CompositionSpan),MaxCompositionInputBytes)) return ACVR_UNSUPPORTED;
            spans+=src.spans.size();
            for(const auto &span:src.spans) {
                if(!span.width || !span.height || uint64_t(span.x)+span.width>eye.rect_width || uint64_t(span.y)+span.height>eye.rect_height ||
                   uint64_t(span.width)*span.height!=span.pixels.size()) return ACVR_BAD_ARGUMENT;
                if(!charge(bytes,span.pixels.capacity(),sizeof(CompositionPixel),MaxCompositionInputBytes)) return ACVR_UNSUPPORTED;
            }
        }
        if(std::any_of(payload.begin(),payload.end(),[](auto i){return i==SIZE_MAX;})) return ACVR_BAD_ARGUMENT;
        std::vector<uint32_t> coverage(static_cast<size_t>(pixels),0);
        for(size_t i=0;i<plan.items.size();++i) for(const auto &span:input.items[payload[i]].spans) {
            for(uint32_t y=0;y<span.height;++y) for(uint32_t x=0;x<span.width;++x) {
                const auto at=size_t(span.y+y)*eye.rect_width+span.x+x;
                if(coverage[at]==i+1) return ACVR_BAD_ARGUMENT;
                coverage[at]=static_cast<uint32_t>(i+1);
                if(plan.items[i].kind==CompositionKind::Polygon) {
                    const auto alpha=span.pixels[size_t(y)*span.width+x].alpha;
                    if(alpha!=0 && alpha!=255) return ACVR_UNSUPPORTED;
                }
            }
        }
        std::vector<uint32_t> colour(static_cast<size_t>(pixels),pack(plan.mixer.background));
        std::vector<uint8_t> mask(static_cast<size_t>(pixels),0);
        std::vector<size_t> order;order.reserve(plan.items.size());size_t text=SIZE_MAX;
        for(size_t i=0;i<plan.items.size();++i) {if(plan.items[i].kind==CompositionKind::Text) text=i;else order.push_back(i);}
        std::sort(order.begin(),order.end(),[&](auto a,auto b) {
            const auto &x=plan.items[a],&y=plan.items[b];
            if(x.priority!=y.priority) return x.priority>y.priority;
            if(x.kind!=y.kind) return x.kind==CompositionKind::Sprite;
            return x.kind==CompositionKind::Polygon?x.order>y.order:x.order<y.order;
        });
        auto paint=[&](size_t i,bool is_text) {
            const auto &item=plan.items[i];
            for(const auto &span:input.items[payload[i]].spans) for(uint32_t y=0;y<span.height;++y) for(uint32_t x=0;x<span.width;++x) {
                const auto at=size_t(span.y+y)*eye.rect_width+span.x+x;const auto &pixel=span.pixels[size_t(y)*span.width+x];
                if(!pixel.alpha || (is_text && mask[at])) continue;
                uint32_t src=pack(pixel.rgb);
                if(plan.mixer.factor && (is_text?(plan.mixer.flags&2):(item.kind==CompositionKind::Sprite && ((plan.mixer.flags&2) || item.sprite_fade)))) src=fade_colour(src,plan.mixer,is_text);
                colour[at]=alpha_colour(src,colour[at],pixel.alpha);
                if(item.kind==CompositionKind::Sprite && item.prioverchar) mask[at]=1;
            }
        };
        for(auto i:order) paint(i,false);
        if((plan.mixer.flags&1) && plan.mixer.factor) for(auto &c:colour) c=fade_colour(c,plan.mixer,false);
        if(text!=SIZE_MAX) paint(text,true);
        bool gamma=false;for(const auto &lut:plan.mixer.gamma) gamma=gamma || std::any_of(lut.begin(),lut.end(),[](uint8_t v){return v!=0;});
        if(gamma) for(auto &c:colour) {
            std::array<uint8_t,3> mapped{};for(size_t k=0;k<3;++k) mapped[k]=plan.mixer.gamma[k][channel(c,k)];c=pack(mapped);
        }
        // Only after every check/computation succeeds: commit the eye RGB.
        // Caller physical depth and other eye/neighbour pixels are untouched.
        const size_t top=output.height-static_cast<uint32_t>(eye.rect_y)-eye.rect_height;
        for(uint32_t y=0;y<eye.rect_height;++y) std::copy_n(colour.begin()+static_cast<ptrdiff_t>(size_t(y)*eye.rect_width),eye.rect_width,
            output.rgb.begin()+static_cast<ptrdiff_t>((top+y)*output.width+static_cast<uint32_t>(eye.rect_x)));
        return ACVR_OK;
    } catch(...) {return ACVR_ERROR;}
}
}
