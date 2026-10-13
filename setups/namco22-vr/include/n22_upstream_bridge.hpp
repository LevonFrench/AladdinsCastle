// SPDX-License-Identifier: GPL-3.0-only
// Optional source-conformance target: compiled against the pinned engine headers.
#pragma once
#include "n22_worker.hpp"
#include "n22_fog.hpp"
extern "C" {
#include "ss22_gl.h"
}
namespace n22 {
std::shared_ptr<const VideoSnapshot> copy_ss22_video(const ss22_regs &,uint64_t,uint16_t);
void with_ss22_regs(const VideoSnapshot &,const std::function<void(const ss22_regs &)> &);
struct CapturedQuad { geo_quad quad{};geo_view camera{};bool has_camera=false;float focal=0,cx=0,cy=0; };
CapturedQuad copy_geo_quad(const geo_quad &,const geo_view *);
// Explicit fan indices into the owned near-clipped rv array only. No guard-band
// selection, constant construction, camera/scene conversion or new native read.
acvr_result copy_geo_fog_triangle(const CapturedQuad &,uint64_t,std::array<uint32_t,3>,FogQuad &out);
std::vector<CapturedQuad> prepare_with_capture(const VideoSnapshot &,
    const std::function<void(const ss22_regs &)> &prepare);
acvr_result run_source_entry(Worker &,void (*entry)(void),const std::function<void(NativeInput)> &apply);
}
