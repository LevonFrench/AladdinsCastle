// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include "n22_scene.hpp"
// Synthetic test harness only. No game loader, emulation or graphics-provider
// creation. Compiled GL scene drawing is private until provider admission.
// acvr_backend_query reports supported_graphics=0; a real runtime must reject it.
namespace n22 {
struct GlDiagnostic;
acvr_result stage_cpu_scene(acvr_backend *, const SceneInput &);
acvr_result draw_cpu_frame(acvr_backend *, const acvr_frame *, const acvr_eye &, Image &,
                           bool hud_only = false);
acvr_result compose_cpu_frame(acvr_backend *,const acvr_frame *,const acvr_eye &,const CompositionEyeSpans &,Image &);
// State/decision inspection only. No fog pixel or factory admission change.
acvr_result inspect_fog_frame(acvr_backend *,const acvr_frame *,uint32_t triangle,uint32_t vertex,FogDecision &);
// Private integration binding. Factory still has graphics mask 0 until provider/
// engine integration is accepted. Mock tests exercise the real game_draw_eye path.
acvr_result configure_gl_draw(acvr_backend *,const acvr_graphics_device &);
GlDiagnostic gl_diagnostic(acvr_backend *);
}
