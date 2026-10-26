// SPDX-License-Identifier: MIT
#pragma once
#include "acvr.h"
#include "gun_model.hpp"
#include "controls.hpp"
#include "hold_ring.hpp"
#include <memory>
#include <vector>

namespace acvr {
// Internal platform boundary. The optional OpenXR provider owns wait/begin,
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
    acvr_controller_sample calibration{}; // optional room sample, not mapper admission
    std::vector<acvr_axis_input> axes;
    std::vector<acvr_button_input> buttons; // HELD levels only; core derives edges
};
struct Host {
    virtual ~Host() = default;
    virtual acvr_graphics_device device() const = 0;
    virtual bool headless() const noexcept = 0;
    // Own the graphics context for each core callback span. A provider may
    // restore a caller's context after the span; recorded hosts are no-ops.
    virtual acvr_result activate() { return ACVR_OK; }
    virtual void deactivate() noexcept {}
    virtual acvr_result configure(float scene_units_per_metre, uint32_t anchor_mode) = 0;
    virtual acvr_result begin(Display &) = 0; // OK creates exactly one end obligation
    // Bind the validated provider-owned target immediately before its world draw.
    // The owner context is current; default is for recorded/headless hosts only.
    virtual acvr_result bind_world_target(const acvr_draw_info &) { return ACVR_OK; }
    virtual acvr_result end(bool rendered) = 0; // false submits zero layers
    virtual void output(const acvr_output_event &) = 0;
    virtual void cancel_effects() noexcept = 0;
    virtual bool supports_guns() const noexcept { return false; }
    virtual GunMount primary_anchor_mount(uint32_t) const noexcept { return {"grip"}; }
    // Null disables two-hand capture. Native hosts may choose an authored
    // support anchor for the current primary hand; recorded hosts keep v0.1.
    virtual GunMount support_anchor_mount(uint32_t) const noexcept { return {"grip_two"}; }
    // Resolved binding policy only; false unless explicitly configured.
    virtual bool offscreen_reload(uint32_t) const noexcept { return false; }
    virtual bool supports_controller_samples() const noexcept { return false; }
    virtual bool supports_calibration_samples() const noexcept { return false; }
    virtual uint32_t supported_runtime_actions() const noexcept { return 0; }
    // An accepted Pause immediately pauses the core and ends this display with
    // zero game layers. Provider UI owns resume via acvr_runtime_set_paused(0).
    // It must not require another mapped action: gameplay mapping is suspended.
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
    // U4 hold ring (hold_ring.hpp). A provider that can draw over the finished eye image returns true here: the runtime
    // then hands it the ring state every tick (show_hold_ring) and asks it to draw after the game and the gun
    // (draw_hold_ring), and it holds the quit back for the 0.7 s flash so the player sees it. The default draws nothing
    // and quits at once.
    virtual bool draws_hold_ring() const noexcept { return false; }
    virtual void show_hold_ring(const HoldRingView &) {}
    virtual acvr_result draw_hold_ring(const acvr_draw_info &) { return ACVR_OK; }
};
// Headless is accepted only here, never through the public XR create export.
// The host's lifetime transfers on success or failure. Optional model/control
// files are prepared once during creation; no per-frame filesystem reads.
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
