// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include "n22_scene.hpp"
// Synthetic test harness only. No game content loader, emulation or GPU renderer.
// acvr_backend_query reports supported_graphics=0; a real runtime must reject it.
namespace n22 {
acvr_result stage_cpu_scene(acvr_backend *, const SceneInput &);
acvr_result draw_cpu_frame(acvr_backend *, const acvr_frame *, const acvr_eye &, Image &,
                           bool hud_only = false);
}
