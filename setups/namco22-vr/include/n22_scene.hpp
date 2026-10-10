// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include "acvr.h"
#include "n22_material.hpp"
#include "n22_fog.hpp"
#include "n22_background.hpp"
#include <array>
#include <cstdint>
#include <vector>

namespace n22 {
struct Vec3 { float x, y, z; };
// Native geo_vert coordinates: x/y in sixteenths of a pixel, positive depth.
struct ProjectedVertex { float x16, y16, depth; };
enum class Layer { World, Hud, Backdrop, GunFlash };
struct Polygon {
    std::array<ProjectedVertex, 3> vertices;
    uint32_t camera_id = 0;
    Layer layer = Layer::World;
    uint32_t rgb = 0xffffff;
    uint32_t material = NoMaterial;
    std::array<MaterialVertex,3> attributes{};
    FogQuad fog;
};
struct Triangle {
    std::array<Vec3, 3> vertices;
    uint32_t camera_id;
    Layer layer;
    uint32_t rgb;
    uint32_t material = NoMaterial;
    std::array<MaterialVertex,3> attributes{};
    FogQuad fog;
    FogSamples fog_samples;
};
struct SceneInput {
    std::vector<acvr_game_camera> cameras;
    std::vector<Polygon> polygons;
    float hud_depth_scene = 2;
    MaterialPacket materials;
    FogState fog;
    BackgroundState background;
};
// Owns all data. Never retains mutable source arrays or references to emulation.
struct Frame {
    uint64_t id = 0;
    std::vector<acvr_game_camera> cameras;
    std::vector<Triangle> triangles;
    MaterialPacket materials;
    FogState fog;
    BackgroundState background;
};
struct Image {
    uint32_t width, height;
    std::vector<uint32_t> rgb; // top-left row order; packed 0xRRGGBB
    std::vector<float> depth;
    Image(uint32_t w, uint32_t h);
};
acvr_game_camera synthetic_camera();
SceneInput synthetic_cube();
SceneInput synthetic_material_cube();
Vec3 unproject(const ProjectedVertex &, const acvr_game_camera &);
acvr_result prepare(const SceneInput &, uint64_t id, Frame &out);
// Bounded draw admission; state-only inspection can still represent other fog
// policies/layers. Reject unsupported enabled fog before uploads/target clears.
acvr_result validate_fog_draw(const Frame &);
acvr_result project_gun(Vec3 point, const acvr_game_camera &, float &x, float &y,
                        bool &offscreen);
acvr_result raycast(const Frame &, const acvr_ray &, acvr_hit &);
// Private CPU bridge, not an acvr_render_target graphics API or GL handle.
// Uses GL clip convention and lower-left eye rectangles; no graphics calls.
acvr_result draw_cpu(const Frame &, const acvr_eye &, Image &, bool hud_only = false);
acvr_eye desktop_eye(uint32_t index, float eye_x, float convergence_depth,
                     uint32_t width, uint32_t height);
std::array<uint32_t, 2> time_crisis_adc(float x, float y);
}
