// SPDX-License-Identifier: MIT
#pragma once
#include "acvr.h"
#include "gun_asset.hpp"
#include <string_view>

namespace acvr {
struct GunMotion {
    std::string node, drive;
    std::vector<uint32_t> nodes;
    std::array<float,3> axis{};
    float minimum=0, maximum=0;
    bool rotate=false;
    uint32_t duration_ms=45;
};
struct GunModel {
    std::string id;
    GunAsset asset;
    std::vector<GunMotion> motions;
};
// Native metadata/model preparation. No graphics. Output is atomic on failure.
bool decode_gun_model(const std::vector<uint8_t> &glb, std::string_view metadata,
                      std::string_view expected_id, GunModel &, std::string &error) noexcept;
bool load_gun_model(const std::string &model_path, const std::string &metadata_path,
                    std::string_view expected_id, GunModel &, std::string &error) noexcept;

struct GunDraw {
    const GunAsset *asset=nullptr; // borrowed until draw returns
    std::vector<Matrix> scene_from_node;
    std::array<float,4> body{}, accent{};
    acvr_ray muzzle{}; // bind pose, independent of animated node transforms
    uint32_t lod=0, slot=0, hand=0, laser_mode=0;
    bool visible=true;
};
class GunInstance {
public:
    GunModel model;
    // Called once after model construction. Events are owner-thread-only.
    void reset();
    void clear_motion(); // preserves sequence monotonicity
    acvr_result event(const acvr_gun_event &, int64_t now_ns);
    // Runtime logical drives. Missing optional motion is a no-op. A pulse is
    // issued once on a consuming native tick, never while drawing an eye.
    acvr_result drive(std::string_view name, float value, bool pulse, int64_t now_ns);
    acvr_result draw(const acvr_pose &scene_from_stage, const acvr_pose &grip,
                     const acvr_gun_slot_config &, float scale, float distance,
                     int64_t now_ns, GunDraw &) const;
private:
    struct MotionState { float value=0; int64_t start=0, duration=0; bool pulse=false; };
    std::vector<MotionState> states;
    uint64_t sequence=0;
};
}
