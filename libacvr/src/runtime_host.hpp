// SPDX-License-Identifier: MIT
#pragma once
#include "acvr.h"
#include "gun_model.hpp"
#include "controls.hpp"
#include <memory>
#include <vector>

namespace acvr {
// Internal platform boundary. The production OpenXR provider will own wait/begin,
// image acquisition, predicted poses and submission. Tests use a recorded provider.
// This is not part of the C ABI and must never be passed across a module boundary.
struct Display {
    acvr_tracking tracking{};
    // Rigid anchor, translation in metres before scene scaling. Required each
    // begin; eyes must use the inverse of this SAME predicted-frame transform.
    // Tracking stays stage-space; never pre-transform it in a provider.
    acvr_pose scene_from_stage{};
    bool focused = true, should_render = true;
    acvr_draw_info eyes[2]{};
    bool trigger[2]{};
    bool reload[2]{}; // resolved gun-slot HELD levels; core queues native edges
    ControllerButtons controllers[2]{}; // populated only by a capable provider
    std::vector<acvr_axis_input> axes;
    std::vector<acvr_button_input> buttons; // HELD levels only; core derives edges
};
struct Host {
    virtual ~Host() = default;
    virtual acvr_graphics_device device() const = 0;
    virtual bool headless() const noexcept = 0;
    virtual acvr_result configure(float scene_units_per_metre, uint32_t anchor_mode) = 0;
    virtual acvr_result begin(Display &) = 0; // OK creates exactly one end obligation
    virtual acvr_result end(bool rendered) = 0; // false submits zero layers
    virtual void output(const acvr_output_event &) = 0;
    virtual void cancel_effects() noexcept = 0;
    virtual bool supports_guns() const noexcept { return false; }
    // Resolved binding policy only; false unless explicitly configured.
    virtual bool offscreen_reload(uint32_t) const noexcept { return false; }
    virtual bool supports_controller_samples() const noexcept { return false; }
    virtual uint32_t supported_runtime_actions() const noexcept { return 0; }
    virtual acvr_result runtime_action(RuntimeAction) { return ACVR_UNSUPPORTED; }
    virtual void unavailable_controls(const std::vector<std::string> &) {}
    // Already-resolved controls data, copied/validated at create. No channel is
    // inferred from catalog labels. Empty keeps the native-fire visual fallback.
    virtual std::vector<GunOutputRoute> gun_output_routes() const { return {}; }
    // Rebind player/slot haptics to this hand; core cancels old effects first.
    virtual void gun_hand_changed(uint32_t, uint32_t) {}
    // Called after world draw for each eye, using its existing depth attachment.
    // Do not retain pointers, mutate draw state, or advance motion here.
    virtual acvr_result draw_gun(const acvr_draw_info &, const GunDraw &) { return ACVR_UNSUPPORTED; }
};
// Headless is accepted only here, never through the public XR create export.
// The host's lifetime transfers on success or failure. No files are read by core.
acvr_result create_with_host(const acvr_runtime_config *, const acvr_backend_api *,
                            std::unique_ptr<Host>, acvr_runtime **);
// Shared model-independent math. The mount is the non-animated muzzle-in-grip
// pose; calibration rotates it before grip placement. All translations are metres.
acvr_result muzzle_ray(const acvr_pose &grip, const acvr_pose &mount,
                       const float angle_xyzw[4], float units_per_metre,
                       float distance_m, acvr_ray &out);
acvr_result anchored_muzzle_ray(const acvr_pose &scene_from_stage,
                               const acvr_pose &grip, const acvr_pose &mount,
                               const float angle_xyzw[4], float units_per_metre,
                               float distance_m, acvr_ray &out);
}
