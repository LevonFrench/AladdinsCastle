// SPDX-License-Identifier: MIT
#include "runtime_host.hpp"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstring>
#include <deque>
#include <limits>
#include <map>
#include <string>
#include <thread>
#include <tuple>

namespace {
template<class T> acvr_result valid(const T *p) {
    if (!p || p->size < sizeof(T)) return ACVR_BAD_ARGUMENT;
    return p->version == ACVR_STRUCT_VERSION ? ACVR_OK : ACVR_BAD_VERSION;
}
template<class T> bool embedded(const T &p) {
    // Embedded records cannot grow in place without moving their parent's fields.
    return p.size == sizeof(T) && p.version == ACVR_STRUCT_VERSION;
}
template<class T> T init() { T p{}; ACVR_INIT(&p); return p; }
template<class T> void payload(T *out, const T &in) {
    std::memcpy(reinterpret_cast<char *>(out) + 8, reinterpret_cast<const char *>(&in) + 8, sizeof(T) - 8);
}
template<class T> bool array_ok(const T *p, uint32_t count, uint32_t stride) {
    return !count || (p && stride >= sizeof(T) && stride % alignof(T) == 0 &&
        reinterpret_cast<uintptr_t>(p) % alignof(T) == 0 &&
        count <= std::numeric_limits<size_t>::max() / stride);
}
template<class T> const T &item(const T *p, uint32_t stride, uint32_t i) {
    return *reinterpret_cast<const T *>(reinterpret_cast<const char *>(p) + size_t(stride) * i);
}
bool finite(const float *v, size_t n) {
    for (size_t i = 0; i < n; ++i) if (!std::isfinite(v[i])) return false;
    return true;
}
bool unit(const float *q) {
    if (!finite(q, 4)) return false;
    float n = 0; for (size_t i = 0; i < 4; ++i) n += q[i] * q[i];
    return std::abs(n - 1) < 0.001f;
}
bool pose_ok(const acvr_pose &p) {
    return valid(&p) == ACVR_OK && finite(p.position_m, 3) && unit(p.orientation_xyzw);
}
std::array<float, 3> rotate(const float *q, std::array<float, 3> v) {
    const std::array<float, 3> t{2 * (q[1]*v[2]-q[2]*v[1]),
        2 * (q[2]*v[0]-q[0]*v[2]), 2 * (q[0]*v[1]-q[1]*v[0])};
    return {v[0]+q[3]*t[0]+q[1]*t[2]-q[2]*t[1],
        v[1]+q[3]*t[1]+q[2]*t[0]-q[0]*t[2], v[2]+q[3]*t[2]+q[0]*t[1]-q[1]*t[0]};
}
struct Digital {
    bool sampled = false, held = false, armed = true;
    std::deque<bool> edges;
    void sample(bool down) {
        if (!armed) { sampled = down; if (!down) armed = true; return; }
        if (down != sampled) { edges.push_back(down); sampled = down; }
        // A stalled native loop must not allocate without bound. Fail closed.
        if (edges.size() > 64) clear();
    }
    uint32_t consume() {
        if (edges.empty()) return held ? ACVR_INPUT_HELD : 0;
        held = edges.front(); edges.pop_front();
        return held ? ACVR_INPUT_HELD | ACVR_INPUT_PRESSED : ACVR_INPUT_RELEASED;
    }
    void clear() { edges.clear(); held = sampled = false; armed = false; }
};
using Key = std::pair<uint32_t, uint32_t>; // semantic, player
}

struct acvr_runtime {
    std::thread::id owner = std::this_thread::get_id();
    std::unique_ptr<acvr::Host> host;
    acvr_backend_api api{};
    acvr_backend *backend = nullptr;
    acvr_frame *frame = nullptr;
    acvr_frame_info frame_info = init<acvr_frame_info>();
    acvr_backend_info info = init<acvr_backend_info>();
    acvr_tracking tracking{};
    std::vector<acvr_control_desc> controls;
    std::map<Key, Digital> buttons;
    std::array<Digital, 2> triggers;
    std::array<uint32_t, 2> players{};
    uint32_t gun_count = 0, budget = 0;
    float fallback_m = 0;
    uint64_t tick = 0, display_id = 0, sequence = 0, dropped = 0;
    int64_t simulation_ns = 0, due_ns = 0, last_display_ns = 0;
    uint64_t period_whole = 0, period_rem = 0, remainder = 0;
    bool clock_started = false, user_paused = false, paused = false, failed = false, have_tracking = false;
    std::string game, content, storage, options;
    bool owns_thread() const { return owner == std::this_thread::get_id(); }
    void clear_inputs() { for (auto &g : triggers) g.clear(); for (auto &b : buttons) b.second.clear(); }
    void release() { if (frame) { api.game_release_frame(backend, frame); frame = nullptr; } }
    acvr_result pause(bool value) {
        if (paused == value) return ACVR_OK;
        const auto result = api.game_pause(backend, value ? 1u : 0u);
        if (result != ACVR_OK) return result;
        paused = value; clear_inputs(); host->cancel_effects(); clock_started = false;
        return ACVR_OK;
    }
    acvr_result outputs() {
        std::array<acvr_output_event, 32> events;
        // A broken backend must not trap the owner thread in an endless MORE loop.
        for (unsigned batch = 0; batch < 128; ++batch) {
            for (auto &e : events) e = init<acvr_output_event>();
            auto out = init<acvr_outputs>(); out.events = events.data();
            out.capacity = static_cast<uint32_t>(events.size()); out.stride = sizeof(events[0]);
            const auto result = api.game_poll_outputs(backend, &out);
            if (result != ACVR_OK && result != ACVR_MORE) return result;
            if (out.count > events.size() || out.events != events.data() || out.stride != sizeof(events[0]) ||
                (result == ACVR_MORE && !out.count) || out.dropped < dropped)
                return ACVR_BAD_STATE;
            if (out.dropped != dropped) { host->cancel_effects(); dropped = out.dropped; }
            for (uint32_t i = 0; i < out.count; ++i) {
                const auto &e = events[i];
                if (valid(&e) != ACVR_OK || e.sequence <= sequence || e.tick_id > tick ||
                    e.player >= info.player_count || !std::isfinite(e.strength)) return ACVR_BAD_STATE;
                sequence = e.sequence; host->output(e);
            }
            if (result == ACVR_OK) return ACVR_OK;
        }
        return ACVR_BAD_STATE;
    }
    acvr_result aim(uint32_t slot, const acvr::Display &d, acvr_gun_input &gun) {
        gun = init<acvr_gun_input>(); gun.slot = slot; gun.player = players[slot];
        gun.flags = ACVR_GUN_OFFSCREEN; gun.camera_id = ACVR_NO_CAMERA;
        gun.screen_x = gun.screen_y = 0.5f;
        const auto &hand = slot == 0 ? d.tracking.right : d.tracking.left;
        if ((hand.aim_flags & 3u) != 3u || !pose_ok(hand.aim)) { triggers[slot].clear(); return ACVR_OK; }
        gun.flags |= ACVR_GUN_TRACKED; gun.trigger = triggers[slot].consume();
        if (!frame) return ACVR_OK;
        gun.aim_frame_id = frame_info.frame_id;
        auto mount = init<acvr_pose>(); mount.orientation_xyzw[3] = 1;
        const float identity[4]{0, 0, 0, 1};
        auto ray = init<acvr_ray>();
        auto result = acvr::anchored_muzzle_ray(d.scene_from_stage, hand.aim, mount, identity,
                                               info.scene_units_per_metre, fallback_m, ray);
        if (result != ACVR_OK) return result;
        auto hit = init<acvr_hit>();
        result = api.game_raycast(backend, frame, &ray, &hit);
        if (result != ACVR_OK) return result;
        const uint32_t camera_id = hit.found ? hit.camera_id : frame_info.primary_camera_id;
        if (camera_id == ACVR_NO_CAMERA || camera_id >= frame_info.camera_count) return ACVR_OK;
        auto camera = init<acvr_game_camera>();
        result = api.game_camera(backend, frame, camera_id, &camera);
        if (result != ACVR_OK) return result;
        if (!camera.raster_width || !camera.raster_height || !finite(camera.viewport_px, 4)) return ACVR_BAD_STATE;
        float nx = 0, ny = 0;
        if (hit.found && (hit.flags & ACVR_HIT_GUN_COORDS)) { nx = hit.screen_x; ny = hit.screen_y; }
        else {
            float p[3];
            for (unsigned i = 0; i < 3; ++i) p[i] = hit.found ? hit.position_scene[i] :
                ray.origin_scene[i] + ray.direction_scene[i] * ray.max_distance_scene;
            float c[3];
            for (unsigned row = 0; row < 3; ++row) c[row] = camera.view_from_scene[row] * p[0] +
                camera.view_from_scene[4+row] * p[1] + camera.view_from_scene[8+row] * p[2] + camera.view_from_scene[12+row];
            if (!finite(c, 3) || c[2] >= 0) return ACVR_OK;
            nx = (camera.centre_x_px + camera.focal_x_px * c[0] / -c[2]) / float(camera.raster_width);
            ny = (camera.centre_y_px - camera.focal_y_px * c[1] / -c[2]) / float(camera.raster_height);
        }
        if (!std::isfinite(nx) || !std::isfinite(ny)) return ACVR_OK;
        gun.screen_x = nx; gun.screen_y = ny; gun.camera_id = camera_id;
        const float x = nx * float(camera.raster_width), y = ny * float(camera.raster_height);
        if (nx >= 0 && nx <= 1 && ny >= 0 && ny <= 1 && x >= camera.viewport_px[0] && y >= camera.viewport_px[1] &&
            x <= camera.viewport_px[0]+camera.viewport_px[2] && y <= camera.viewport_px[1]+camera.viewport_px[3])
            gun.flags &= ~ACVR_GUN_OFFSCREEN;
        return ACVR_OK;
    }
    acvr_result advance(const acvr::Display &d) {
        std::vector<acvr_gun_input> guns(gun_count);
        for (uint32_t slot = 0; slot < gun_count; ++slot) {
            auto result = aim(slot, d, guns[slot]); if (result != ACVR_OK) return result;
        }
        std::vector<acvr_axis_input> axes;
        std::vector<acvr_button_input> switches;
        for (const auto &control : controls) {
            if (control.kind == ACVR_CONTROL_AXIS) {
                auto a = init<acvr_axis_input>(); a.semantic = control.semantic; a.player = control.player;
                for (const auto &value : d.axes) if (value.semantic == a.semantic && value.player == a.player) a.value = value.value;
                if (!std::isfinite(a.value) || a.value < control.minimum || a.value > control.maximum) return ACVR_BAD_ARGUMENT;
                // Lost gun tracking must not hold a player's pedal down.
                for (const auto &g : guns) if (g.player == a.player && a.semantic == ACVR_AXIS_COVER_PEDAL && !(g.flags & ACVR_GUN_TRACKED)) a.value = 0;
                axes.push_back(a);
            } else if (control.kind == ACVR_CONTROL_BUTTON) {
                auto b = init<acvr_button_input>(); b.semantic = control.semantic; b.player = control.player;
                b.state = buttons[{b.semantic, b.player}].consume(); switches.push_back(b);
            }
        }
        auto in = init<acvr_inputs>(); in.tick_id = tick + 1; in.sample_time_ns = d.tracking.sample_time_ns;
        in.gun_count = uint32_t(guns.size()); in.gun_stride = sizeof(acvr_gun_input); in.guns = guns.data();
        in.axis_count = uint32_t(axes.size()); in.axis_stride = sizeof(acvr_axis_input); in.axes = axes.data();
        in.button_count = uint32_t(switches.size()); in.button_stride = sizeof(acvr_button_input); in.buttons = switches.data();
        release();
        auto result = api.game_set_inputs(backend, &in); if (result != ACVR_OK) return result;
        auto step = init<acvr_step_info>(); step.tick_id = tick + 1; step.simulation_time_ns = simulation_ns;
        frame_info = init<acvr_frame_info>();
        result = api.game_step(backend, &step, &frame, &frame_info);
        if (result != ACVR_OK) return result;
        if (!frame || frame_info.frame_id != tick + 1 ||
            (frame_info.camera_count && frame_info.primary_camera_id >= frame_info.camera_count) ||
            (!frame_info.camera_count && frame_info.primary_camera_id != ACVR_NO_CAMERA)) return ACVR_BAD_STATE;
        ++tick;
        remainder += period_rem;
        const uint64_t interval = period_whole + remainder / info.native_rate_num;
        remainder %= info.native_rate_num;
        if (interval > uint64_t(INT64_MAX - simulation_ns) || due_ns > INT64_MAX - int64_t(interval)) return ACVR_BAD_STATE;
        simulation_ns += int64_t(interval); due_ns += int64_t(interval);
        return outputs();
    }
};

namespace acvr {
acvr_result anchored_muzzle_ray(const acvr_pose &anchor, const acvr_pose &grip,
                               const acvr_pose &mount, const float *angle,
                               float scale, float distance, acvr_ray &out) {
    if (valid(&out) != ACVR_OK || !pose_ok(anchor) || !std::isfinite(scale) || scale <= 0)
        return ACVR_BAD_ARGUMENT;
    auto ray = init<acvr_ray>();
    const auto status = muzzle_ray(grip, mount, angle, 1, distance, ray);
    if (status != ACVR_OK) return status;
    const auto origin = rotate(anchor.orientation_xyzw, {ray.origin_scene[0], ray.origin_scene[1], ray.origin_scene[2]});
    const auto direction = rotate(anchor.orientation_xyzw, {ray.direction_scene[0], ray.direction_scene[1], ray.direction_scene[2]});
    for (unsigned i = 0; i < 3; ++i) {
        ray.origin_scene[i] = (origin[i] + anchor.position_m[i]) * scale;
        ray.direction_scene[i] = direction[i];
    }
    ray.max_distance_scene *= scale;
    if (!finite(ray.origin_scene, 3) || !std::isfinite(ray.max_distance_scene)) return ACVR_BAD_ARGUMENT;
    payload(&out, ray); return ACVR_OK;
}
acvr_result muzzle_ray(const acvr_pose &grip, const acvr_pose &mount, const float *angle,
                       float scale, float distance, acvr_ray &out) {
    if (valid(&out) != ACVR_OK || !pose_ok(grip) || !pose_ok(mount) || !angle || !unit(angle) ||
        !std::isfinite(scale) || scale <= 0 || !std::isfinite(distance) || distance <= 0) return ACVR_BAD_ARGUMENT;
    auto pos = rotate(grip.orientation_xyzw, rotate(angle, {mount.position_m[0], mount.position_m[1], mount.position_m[2]}));
    const auto dir = rotate(grip.orientation_xyzw, rotate(angle, rotate(mount.orientation_xyzw, {0, 0, -1})));
    auto result = init<acvr_ray>();
    for (unsigned i = 0; i < 3; ++i) { result.origin_scene[i] = (pos[i] + grip.position_m[i]) * scale; result.direction_scene[i] = dir[i]; }
    result.max_distance_scene = distance * scale;
    if (!finite(result.origin_scene, 3) || !std::isfinite(result.max_distance_scene)) return ACVR_BAD_ARGUMENT;
    payload(&out, result); return ACVR_OK;
}
acvr_result create_with_host(const acvr_runtime_config *config, const acvr_backend_api *api,
                            std::unique_ptr<Host> host, acvr_runtime **out) {
    if (!out) return ACVR_BAD_ARGUMENT;
    *out = nullptr;
    if (!config || config->size < ACVR_RUNTIME_CONFIG_V1_SIZE || !host) return ACVR_BAD_ARGUMENT;
    if (config->version != ACVR_STRUCT_VERSION) return ACVR_BAD_VERSION;
    if (config->size != ACVR_RUNTIME_CONFIG_V1_SIZE && config->size < sizeof(*config)) return ACVR_BAD_ARGUMENT;
    auto result = valid(api); if (result != ACVR_OK) return result;
    if (api->abi_version != ACVR_ABI_VERSION) return ACVR_BAD_VERSION;
    if (!api->game_open || !api->game_close || !api->game_pause || !api->game_set_inputs || !api->game_step ||
        !api->game_release_frame || !api->game_camera || !api->game_draw_eye || !api->game_poll_outputs) return ACVR_BAD_ARGUMENT;
    if (!config->max_catchup_ticks || config->max_catchup_ticks > 1024 || !std::isfinite(config->fallback_aim_distance_m) ||
        config->fallback_aim_distance_m <= 0 || !config->game_id_utf8 || !*config->game_id_utf8 ||
        !std::isfinite(config->requested_refresh_hz) || config->requested_refresh_hz < 0 ||
        (config->anchor_mode != ACVR_ANCHOR_RAIL && config->anchor_mode != ACVR_ANCHOR_COCKPIT)) return ACVR_BAD_ARGUMENT;
    // Parsing/binding/model loading is the next slice. Never silently ignore it.
    if ((config->size >= sizeof(*config) && config->gun_slot_count) ||
        (config->merged_controls_path_utf8 && *config->merged_controls_path_utf8)) return ACVR_UNSUPPORTED;
    try {
        auto r = std::make_unique<acvr_runtime>(); r->api = *api; r->host = std::move(host);
        auto graphics = r->host->device();
        if (valid(&graphics) != ACVR_OK || graphics.api != config->graphics_api) return ACVR_BAD_ARGUMENT;
        if (r->host->headless()) { if (graphics.api != 0) return ACVR_BAD_ARGUMENT; }
        else if (graphics.api < ACVR_GRAPHICS_GL || graphics.api > ACVR_GRAPHICS_D3D11 ||
            !(api->supported_graphics & ACVR_GRAPHICS_BIT(graphics.api))) return ACVR_UNSUPPORTED;
        r->game = config->game_id_utf8; r->content = config->content_root_utf8 ? config->content_root_utf8 : "";
        r->storage = config->storage_root_utf8 ? config->storage_root_utf8 : "";
        r->options = config->backend_options_utf8 ? config->backend_options_utf8 : "";
        auto open = init<acvr_open_info>(); open.graphics = graphics; open.game_id_utf8 = r->game.c_str();
        open.content_root_utf8 = r->content.c_str(); open.storage_root_utf8 = r->storage.c_str(); open.backend_options_utf8 = r->options.c_str();
        result = api->game_open(&open, &r->backend, &r->info);
        if (result != ACVR_OK) { if (r->backend) api->game_close(r->backend); return result; }
        // Any failure after open must close even if vector allocation throws.
        acvr_runtime *raw = r.release();
        try {
            const auto &info = raw->info;
            if (!raw->backend || !info.native_rate_num || !info.native_rate_den || !info.player_count ||
                valid(&info) != ACVR_OK || info.gun_count > 1 || !std::isfinite(info.scene_units_per_metre) || info.scene_units_per_metre <= 0 ||
                !array_ok(info.controls, info.control_count, info.control_stride) ||
                (info.gun_count && !(info.capabilities & ACVR_CAP_RAYCAST)) ||
                ((info.capabilities & ACVR_CAP_RAYCAST) && !api->game_raycast) ||
                ((info.capabilities & ACVR_CAP_PERSISTENCE) && !api->game_flush_persistent) ||
                ((info.capabilities & ACVR_CAP_MULTIVIEW) && !api->game_draw_multiview) ||
                (info.capabilities & ACVR_CAP_SEPARATE_HUD)) result = ACVR_UNSUPPORTED;
            std::map<std::tuple<uint32_t,uint32_t,uint32_t>, bool> seen;
            std::array<bool,2> gun_declared{};
            if (result == ACVR_OK) for (uint32_t i = 0; i < info.control_count; ++i) {
                const auto &c = item(info.controls, info.control_stride, i);
                if (valid(&c) != ACVR_OK || c.size > info.control_stride || c.player >= info.player_count ||
                    c.kind < ACVR_CONTROL_AXIS || c.kind > ACVR_CONTROL_GUN || !std::isfinite(c.minimum) ||
                    !std::isfinite(c.maximum) || c.minimum > c.maximum ||
                    !seen.emplace(std::make_tuple(c.kind,c.semantic,c.player),true).second) { result = ACVR_BAD_ARGUMENT; break; }
                if (c.kind == ACVR_CONTROL_GUN) {
                    if (c.semantic >= info.gun_count || gun_declared[c.semantic]) { result = ACVR_BAD_ARGUMENT; break; }
                    gun_declared[c.semantic] = true; raw->players[c.semantic] = c.player;
                }
                raw->controls.push_back(c);
                if (c.kind == ACVR_CONTROL_BUTTON) raw->buttons[{c.semantic,c.player}] = Digital{};
            }
            for (uint32_t i = 0; result == ACVR_OK && i < info.gun_count; ++i)
                if (!gun_declared[i]) result = ACVR_BAD_ARGUMENT;
            if (result != ACVR_OK) { acvr_runtime_destroy(raw); return result; }
            raw->gun_count = info.gun_count; raw->budget = config->max_catchup_ticks; raw->fallback_m = config->fallback_aim_distance_m;
            const uint64_t period = 1000000000ULL * info.native_rate_den;
            raw->period_whole = period / info.native_rate_num; raw->period_rem = period % info.native_rate_num;
            if (!raw->period_whole) { acvr_runtime_destroy(raw); return ACVR_BAD_ARGUMENT; }
            result = raw->host->configure(info.scene_units_per_metre, config->anchor_mode);
            if (result != ACVR_OK) { acvr_runtime_destroy(raw); return result; }
            *out = raw; return ACVR_OK;
        } catch (...) { acvr_runtime_destroy(raw); return ACVR_ERROR; }
    } catch (...) { return ACVR_ERROR; }
}
}

extern "C" {
acvr_result ACVR_CALL acvr_runtime_create(const acvr_runtime_config *, const acvr_backend_api *, acvr_runtime **out) {
    if (!out) return ACVR_BAD_ARGUMENT;
    *out = nullptr; return ACVR_UNSUPPORTED; // OpenXR provider not linked yet.
}
acvr_result ACVR_CALL acvr_runtime_tick(acvr_runtime *r) {
    if (!r) return ACVR_BAD_ARGUMENT;
    if (!r->owns_thread() || r->failed) return ACVR_BAD_STATE;
    bool begun = false;
    try {
        acvr::Display d;
        auto result = r->host->begin(d);
        if (result != ACVR_OK) { r->failed = true; r->pause(true); r->host->cancel_effects(); return result; }
        begun = true; ++r->display_id;
        const auto finish = [&](acvr_result value, bool rendered) {
            begun = false; // end may throw; never attempt a second submission.
            const auto end = r->host->end(rendered && value == ACVR_OK);
            if (value == ACVR_OK) value = end;
            if (value != ACVR_OK) { r->failed = true; r->pause(true); r->host->cancel_effects(); r->clear_inputs(); }
            return value;
        };
        const auto now = d.tracking.predicted_display_time_ns;
        if (valid(&d.tracking) != ACVR_OK || !pose_ok(d.scene_from_stage) || now < 0 || d.tracking.sample_time_ns < 0 ||
            (r->have_tracking && now <= r->last_display_ns)) return finish(ACVR_BAD_ARGUMENT, false);
        r->tracking = d.tracking; r->tracking.display_id = r->display_id; r->have_tracking = true;
        const bool active = d.focused && (d.tracking.head_flags & 3u) == 3u && pose_ok(d.tracking.head) && !r->user_paused;
        result = r->pause(!active); if (result != ACVR_OK) return finish(result, false);
        if (active) {
            std::map<Key, bool> seen_axes, seen_buttons;
            for (const auto &a : d.axes) {
                const auto found = std::find_if(r->controls.begin(), r->controls.end(), [&](const acvr_control_desc &c) {
                    return c.kind == ACVR_CONTROL_AXIS && c.semantic == a.semantic && c.player == a.player;
                });
                if (valid(&a) != ACVR_OK || found == r->controls.end() || !std::isfinite(a.value) ||
                    a.value < found->minimum || a.value > found->maximum || !seen_axes.emplace(Key{a.semantic,a.player},true).second)
                    return finish(ACVR_BAD_ARGUMENT, false);
            }
            for (const auto &b : d.buttons) {
                if (valid(&b) != ACVR_OK || b.state > ACVR_INPUT_HELD || !r->buttons.count({b.semantic,b.player}) ||
                    !seen_buttons.emplace(Key{b.semantic,b.player},true).second) return finish(ACVR_BAD_ARGUMENT, false);
            }
            if (!r->clock_started || now - r->last_display_ns > 250000000 || now - r->due_ns > 250000000) {
                r->due_ns = now; r->clock_started = true;
            }
            for (uint32_t i = 0; i < r->gun_count; ++i) {
                const auto &hand = i == 0 ? d.tracking.right : d.tracking.left;
                if ((hand.aim_flags & 3u) != 3u || !pose_ok(hand.aim)) r->triggers[i].clear();
                else r->triggers[i].sample(d.trigger[i]);
            }
            for (auto &b : r->buttons) {
                bool held = false;
                for (const auto &v : d.buttons) if (v.semantic == b.first.first && v.player == b.first.second) held = (v.state & ACVR_INPUT_HELD) != 0;
                b.second.sample(held);
            }
            for (uint32_t n = 0; now >= r->due_ns && n < r->budget; ++n) {
                result = r->advance(d); if (result != ACVR_OK) return finish(result, false);
            }
        }
        r->last_display_ns = now;
        const bool draw = d.should_render && r->frame && active;
        if (draw) for (unsigned eye = 0; eye < 2; ++eye) {
            auto &info = d.eyes[eye];
            if (valid(&info) != ACVR_OK || valid(&info.target) != ACVR_OK || info.view_count != 1 ||
                !array_ok(info.views, info.view_count, info.view_stride) || valid(info.views) != ACVR_OK)
                return finish(ACVR_BAD_ARGUMENT, false);
            info.frame_id = r->frame_info.frame_id; info.display_id = r->display_id; info.predicted_display_time_ns = now;
            result = r->api.game_draw_eye(r->backend, r->frame, &info);
            if (result != ACVR_OK) return finish(result, false);
        }
        return finish(ACVR_OK, draw);
    } catch (...) {
        if (begun) { try { r->host->end(false); } catch (...) {} }
        r->failed = true; r->pause(true); r->host->cancel_effects(); return ACVR_ERROR;
    }
}
acvr_result ACVR_CALL acvr_runtime_set_paused(acvr_runtime *r, uint32_t paused) {
    if (!r || paused > 1) return ACVR_BAD_ARGUMENT;
    if (!r->owns_thread() || r->failed) return ACVR_BAD_STATE;
    r->user_paused = paused != 0;
    return paused ? r->pause(true) : ACVR_OK; // resume only after next valid focused pose
}
acvr_result ACVR_CALL acvr_runtime_get_tracking(acvr_runtime *r, acvr_tracking *out) {
    if (!r) return ACVR_BAD_ARGUMENT;
    if (!r->owns_thread() || !r->have_tracking) return ACVR_BAD_STATE;
    if (valid(out) != ACVR_OK || !embedded(out->head) || !embedded(out->right) ||
        !embedded(out->left) || !embedded(out->right.grip) || !embedded(out->right.aim) ||
        !embedded(out->left.grip) || !embedded(out->left.aim)) return ACVR_BAD_ARGUMENT;
    // Nested prefixes belong to caller too. Save them across the payload copy.
    const auto saved = *out; payload(out, r->tracking);
    out->head.size=saved.head.size; out->head.version=saved.head.version;
    out->right.size=saved.right.size; out->right.version=saved.right.version;
    out->left.size=saved.left.size; out->left.version=saved.left.version;
    out->right.grip.size=saved.right.grip.size; out->right.grip.version=saved.right.grip.version;
    out->right.aim.size=saved.right.aim.size; out->right.aim.version=saved.right.aim.version;
    out->left.grip.size=saved.left.grip.size; out->left.grip.version=saved.left.grip.version;
    out->left.aim.size=saved.left.aim.size; out->left.aim.version=saved.left.aim.version;
    return ACVR_OK;
}
acvr_result ACVR_CALL acvr_runtime_gun_event(acvr_runtime *r, const acvr_gun_event *) {
    if (!r) return ACVR_BAD_ARGUMENT;
    return r->owns_thread() ? ACVR_UNSUPPORTED : ACVR_BAD_STATE; // model animation not linked
}
acvr_result ACVR_CALL acvr_runtime_destroy(acvr_runtime *r) {
    if (!r) return ACVR_OK;
    if (!r->owns_thread()) return ACVR_BAD_STATE;
    acvr_result result = ACVR_OK;
    r->host->cancel_effects();
    if (r->backend) {
        result = r->api.game_pause(r->backend, 1);
        r->release();
        if ((r->info.capabilities & ACVR_CAP_PERSISTENCE) && r->api.game_flush_persistent) {
            const auto saved = r->api.game_flush_persistent(r->backend); if (result == ACVR_OK) result = saved;
        }
        r->api.game_close(r->backend);
    }
    delete r; return result;
}
}
