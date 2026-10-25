// SPDX-License-Identifier: MIT
#pragma once
#include <array>
#include <string>
#include <string_view>
#include <vector>
#include "gun_asset.hpp"

namespace acvr {
struct HandMount {
    struct FingerPose {
        std::string node;
        std::array<float,3> translation_offset{},scale{1,1,1};
        std::array<float,4> rotation_xyzw{0,0,0,1};
    };
    std::string asset,node;
    std::array<float,3> translation{};
    std::array<float,4> rotation_xyzw{};
    std::vector<FingerPose> finger_pose;
};
struct HandLabelAnchor {
    std::string semantic,node;
    std::array<float,3> local_offset{},bind_position{};
};
struct HandAssembly {
    std::array<HandMount,2> hands; // right, left
    std::array<HandMount,2> primary_mount_by_hand; // gun mount, independent of glove attachment
    std::vector<HandLabelAnchor> labels;
    bool has_support_grip=false;
    HandMount support_grip;
    std::array<HandMount,2> support_by_primary; // right, left primary hand
};
struct InspectionLabel {
    std::string semantic,node,part,game_action,controller_button;
};
struct HandMotion {
    std::string node,drive;
    std::array<float,3> axis{};
    float minimum=0,maximum=0;
};
// Glove colours from the hand metadata `[tints]` table (the `default` entry), as linear
// RGBA for GunDraw: the authored sRGB hex is decoded to linear here, not in the host.
struct HandTint {std::array<float,4> body{1,1,1,1},accent{1,1,1,1};};
// Whether the manifest lists `model_id`. A model without an entry is drawn without hands
// (the caller logs it); false is returned only when the manifest itself cannot be parsed.
bool hand_assembly_lists_model(std::string_view json,std::string_view model_id,
                               bool &listed,std::string &error) noexcept;
// Decodes the authored hand-assembly/1 placement. Does not synthesize
// fallback assets or accept arbitrary paths from the manifest.
bool decode_hand_assembly(std::string_view json,std::string_view model_id,
                          HandAssembly &out,std::string &error) noexcept;
bool decode_hand_motions(std::string_view toml,const GunAsset &asset,
                         std::string_view expected_id,std::string_view expected_hand,
                         std::vector<HandMotion> &out,std::string &error) noexcept;
bool decode_hand_tint(std::string_view toml,HandTint &out,std::string &error) noexcept;
bool validate_hand_pose_nodes(const HandMount &mount,const GunAsset &asset,
                              std::string &error) noexcept;
bool validate_hand_gun_nodes(const HandAssembly &assembly,const GunAsset &asset,
                             std::string &error) noexcept;
// The room has a model but no game/backend declaration. Shared-set rows are
// therefore candidate annotations only; unresolved actions stay unconfirmed.
bool select_shared_control_set(std::string_view defaults,std::string_view model_id,
                               std::string &set_id,std::string &error) noexcept;
bool describe_inspection_labels(const HandAssembly &assembly,std::string_view control_set,
                                std::string_view model_id,std::vector<InspectionLabel> &out,
                                std::string &error) noexcept;
struct HandDriveLevels {float trigger=0,grip=0,primary=0;};
bool pose_hand_nodes(const GunAsset &asset,const HandMount &mount,
                     const std::vector<HandMotion> &motions,HandDriveLevels levels,
                     const Matrix &scene_from_grip,std::vector<Matrix> &out) noexcept;
}
