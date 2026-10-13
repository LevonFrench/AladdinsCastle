// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include "acvr.h"
#include <array>
#include <cstddef>
#include <cstdint>
#include <vector>
namespace n22 {
struct VideoSnapshot;
struct Frame;
struct Image;
enum class CompositionPolicy {Absent,Super22SourceOrder};
enum class CompositionKind {Polygon,Sprite,Text};
enum class CompositionDeclaration {Missing,Empty,Complete};
enum class CompositionStage {Unspecified,PolygonPostShadeFogPolyfade,SpritePostFogPreOwnFade,TextPreOwnFadeAlphaResolved};
struct CompositionMixer {
    uint64_t tick=0;
    std::array<uint8_t,3> background{},fade{};
    uint8_t factor=0,flags=0;
    std::array<std::array<uint8_t,256>,3> gamma{};
};
struct CompositionItem {
    uint32_t id=0,priority=0,order=0;
    uint64_t tick=0;
    CompositionKind kind=CompositionKind::Polygon;
    CompositionStage stage=CompositionStage::Unspecified;
    // Polygon order is native emission (reverse for ties); sprite order, when
    // provided, is the actual already-sorted draw order (ascending for ties).
    bool order_provided=false,sprite_fade=false,prioverchar=false;
};
struct CompositionPlan {
    CompositionPolicy policy=CompositionPolicy::Absent;
    uint64_t tick=0;
    CompositionMixer mixer;
    std::array<CompositionDeclaration,3> declarations{CompositionDeclaration::Missing,CompositionDeclaration::Missing,CompositionDeclaration::Missing};
    std::vector<CompositionItem> items;
};
struct CompositionPixel {std::array<uint8_t,3> rgb{};uint8_t alpha=255;};
static_assert(sizeof(CompositionPixel)==4,"explicit CPU input byte budget");
struct CompositionSpan {
    uint32_t x=0,y=0,width=0,height=0; // top-left, eye-local; rectangles disjoint within an item
    std::vector<CompositionPixel> pixels;
};
struct CompositionEyeItem {
    uint32_t id=0;
    uint64_t tick=0;
    CompositionStage stage=CompositionStage::Unspecified;
    std::vector<CompositionSpan> spans; // empty explicitly means no visible pixels
};
struct CompositionEyeSpans {
    uint64_t frame_id=0;
    acvr_eye eye{}; // owned exact current eye/rectangle/matrix identity
    std::vector<CompositionEyeItem> items;
};
// Provisional implementation admission budgets, not native coverage claims.
inline constexpr size_t MaxCompositionItems=4096,MaxCompositionSpans=65536;
inline constexpr uint32_t MaxCompositionEdge=4096;
inline constexpr uint64_t MaxCompositionPixels=4*1024*1024;
inline constexpr size_t MaxCompositionInputBytes=64*1024*1024,MaxCompositionWorkBytes=64*1024*1024;
acvr_result copy_super22_composition_mixer(const VideoSnapshot &,CompositionMixer &out);
acvr_result validate_composition_plan(const CompositionPlan &,uint64_t frame_tick);
// Reusable setup-private CPU contract. Transactional RGB-only output; physical
// caller/gun depth is never read, synthesized, cleared or written. No native
// decoding/rasterization/anchoring and no GL finalization/factory admission.
acvr_result compose_cpu(const Frame &,const acvr_eye &,const CompositionEyeSpans &,Image &);
}
