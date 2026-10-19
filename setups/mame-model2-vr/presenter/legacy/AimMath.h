// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef ACVR_AIM_MATH_H
#define ACVR_AIM_MATH_H
#include <cmath>
namespace acvr_aim {
struct vec { float x, y, z; };
struct quat { float x, y, z, w; };
inline vec rotate(quat q, vec v)
{
 const vec t{2*(q.y*v.z-q.z*v.y),2*(q.z*v.x-q.x*v.z),2*(q.x*v.y-q.y*v.x)};
 return {v.x+q.w*t.x+q.y*t.z-q.z*t.y, v.y+q.w*t.y+q.z*t.x-q.x*t.z, v.z+q.w*t.z+q.x*t.y-q.y*t.x};
}
struct hit { float u = .5f, v = .5f; bool on_screen = false; };
// All poses use one reference space; picture coordinates start at its top left.
inline hit intersect(vec origin, quat aim, vec centre, quat quad, float width, float height)
{
 hit result;
 if (!(width>0 && height>0)) return result;
 const quat inverse{-quad.x,-quad.y,-quad.z,quad.w};
 const vec o=rotate(inverse,{origin.x-centre.x,origin.y-centre.y,origin.z-centre.z});
 const vec d=rotate(inverse,rotate(aim,{0,0,-1}));
 if (!std::isfinite(d.z) || d.z >= -1e-6f) return result; // parallel/back-facing ray
 const float t=-o.z/d.z;
 if (!(t>=0) || !std::isfinite(t)) return result;
 result.u=.5f+(o.x+t*d.x)/width;
 result.v=.5f-(o.y+t*d.y)/height;
 result.on_screen=std::isfinite(result.u) && std::isfinite(result.v) && result.u>=0 && result.u<=1 && result.v>=0 && result.v<=1;
 return result;
}
// Steering from a controller held like a wheel: its roll about the pointing
// axis. Clockwise (right side down) steers right; full_lock_degrees gives +-1.
inline float steer_from_roll(quat aim, float full_lock_degrees)
{
 const vec right=rotate(aim,{1,0,0});
 if (!std::isfinite(right.y) || !(full_lock_degrees>0)) return 0;
 const float roll=std::asin(std::fmax(-1.f,std::fmin(1.f,right.y)))*57.2957795f;
 return std::fmax(-1.f,std::fmin(1.f,-roll/full_lock_degrees));
}
}
#endif
