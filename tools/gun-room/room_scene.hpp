// SPDX-License-Identifier: MIT
#pragma once
#include <array>
#include <string>
#include <vector>

namespace room {
using Vec3=std::array<float,3>;
struct Target {Vec3 center;float radius;bool lit=false;};
struct Ray {Vec3 origin,direction;};
struct Scene {
    std::vector<Target> targets;
    Ray ray{{0,1.4f,0},{0,0,-1}};
    unsigned hits=0;
    Scene();
    // Returns the nearest struck target or -1. Direction need only be finite;
    // the quadratic handles non-unit input without changing its caller.
    int intersect(const Ray &) const;
    bool shoot(const Ray &);
};
// The second hand is a grip only when close to the authored second-grip point.
// When engaged, -Z is aligned with the main-to-off-hand line. Quaternions XYZW.
bool two_hand_bore(Vec3 main,Vec3 off,Vec3 grip_two_from_main,float grab_radius,
                   std::array<float,4> &orientation);
std::array<float,4> yaw_angle(float degrees);
std::string preview_svg(const Scene &);
}
