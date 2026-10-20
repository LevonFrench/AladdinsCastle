// SPDX-License-Identifier: GPL-3.0-or-later
// Controls for motion cabinets -> named cabinet axes. Default (MapSticks): thumbsticks and
// triggers. Opt-in (Map): body motion from controller poses (roll, tilt, lean, twist, pedalling).
// Header-only and free of OpenXR/GL so the mapping can be tested offline
// (motion_test.cpp). Axis meanings are documented in acvr_mono_frames.h.
#pragma once
#include "acvr_mono_frames.h"
#include <algorithm>
#include <cmath>

namespace body_motion {

struct Pose { float position[3]{}; float orientation[4]{0, 0, 0, 1}; bool valid = false; };

struct Input {
 Pose head, right, left;          // head: between the eyes; right/left: grip poses
 float rightTrigger = 0, leftTrigger = 0;
 float leftStick[2]{}, rightStick[2]{};  // x right +, y up +
 double seconds = 0;              // monotonic time of this sample
};

// Full-scale angles (degrees) and pedalling speed (metres per second).
struct Tuning {
 float steerDeg = 60, stickRollDeg = 45, stickPitchDeg = 40, leanDeg = 35, twistDeg = 35;
 float pedalFull = 1.2f;          // summed vertical hand speed that reads as full pedalling
 float pedalSmoothing = 0.25f;    // seconds
 float stickDeadzone = 0.1f;
};

inline void Rotate(const float q[4], const float v[3], float out[3]) {
 // out = q * v * q^-1 (x, y, z, w)
 const float x = q[0], y = q[1], z = q[2], w = q[3];
 const float tx = 2 * (y * v[2] - z * v[1]), ty = 2 * (z * v[0] - x * v[2]), tz = 2 * (x * v[1] - y * v[0]);
 out[0] = v[0] + w * tx + (y * tz - z * ty);
 out[1] = v[1] + w * ty + (z * tx - x * tz);
 out[2] = v[2] + w * tz + (x * ty - y * tx);
}

constexpr float kDeg = 57.29578f;

// Roll about the controller's forward axis: right side down is positive.
inline float RollDeg(const Pose& p) {
 const float r0[3] = {1, 0, 0}, u0[3] = {0, 1, 0};
 float r[3], u[3]; Rotate(p.orientation, r0, r); Rotate(p.orientation, u0, u);
 return std::atan2(-r[1], u[1]) * kDeg;
}
// Pitch of the controller's forward axis: nose up is positive.
inline float PitchDeg(const Pose& p) {
 const float f0[3] = {0, 0, -1}; float f[3]; Rotate(p.orientation, f0, f);
 return std::asin(std::clamp(f[1], -1.f, 1.f)) * kDeg;
}
// Heading of the forward axis on the floor plane: turning right is positive.
inline float YawDeg(const Pose& p) {
 const float f0[3] = {0, 0, -1}; float f[3]; Rotate(p.orientation, f0, f);
 return std::atan2(f[0], -f[2]) * kDeg;
}
inline float Wrap180(float a) { while (a > 180) a -= 360; while (a < -180) a += 360; return a; }
inline float Unit(float v, float full) { return std::clamp(v / full, -1.f, 1.f); }

class Mapper {
public:
 explicit Mapper(Tuning t = {}) : m_t(t) {}

 // Default controls: left stick steers / is the flight stick / swings the skis, the right
 // stick's x edges the skis, right trigger is throttle and pedalling, left trigger brake
 // and throttle lever. Pulling a stick back reads as +stick_y (climb, nose up, lean back).
 void MapSticks(const Input& in, float axis[acvr_mono::motion_axes]) const {
  auto dz = [&](float v) { const float a = std::abs(v); if (a <= m_t.stickDeadzone) return 0.f; return std::copysign((a - m_t.stickDeadzone) / (1 - m_t.stickDeadzone), v); };
  for (unsigned i = 0; i < acvr_mono::motion_axes; ++i) axis[i] = 0;
  const float lx = std::clamp(dz(in.leftStick[0]), -1.f, 1.f), ly = std::clamp(dz(in.leftStick[1]), -1.f, 1.f);
  const float rx = std::clamp(dz(in.rightStick[0]), -1.f, 1.f);
  axis[acvr_mono::axis_steer] = lx;
  axis[acvr_mono::axis_stick_x] = lx;
  axis[acvr_mono::axis_stick_y] = -ly;
  axis[acvr_mono::axis_twist] = lx;
  axis[acvr_mono::axis_lean] = rx;
  axis[acvr_mono::axis_throttle] = std::clamp(in.rightTrigger, 0.f, 1.f);
  axis[acvr_mono::axis_pedal] = axis[acvr_mono::axis_throttle];
  axis[acvr_mono::axis_brake] = std::clamp(in.leftTrigger, 0.f, 1.f);
  axis[acvr_mono::axis_throttle2] = axis[acvr_mono::axis_brake];
 }

 // Opt-in body motion. Fills axis[motion_axes]; returns false when the right controller is not tracked.
 bool Map(const Input& in, float axis[acvr_mono::motion_axes]) {
  for (unsigned i = 0; i < acvr_mono::motion_axes; ++i) axis[i] = 0;
  axis[acvr_mono::axis_throttle] = std::clamp(in.rightTrigger, 0.f, 1.f);
  axis[acvr_mono::axis_brake] = std::clamp(in.leftTrigger, 0.f, 1.f);
  axis[acvr_mono::axis_throttle2] = std::clamp(in.leftTrigger, 0.f, 1.f);
  if (!in.right.valid) { m_have = false; axis[acvr_mono::axis_pedal] = axis[acvr_mono::axis_throttle]; return false; }

  const float rollR = RollDeg(in.right);
  const bool both = in.left.valid;
  const float roll = both ? 0.5f * (rollR + RollDeg(in.left)) : rollR;
  // Handlebar / bike bank: both hands' mean roll when both are tracked.
  axis[acvr_mono::axis_steer] = Unit(roll, m_t.steerDeg);
  // Flight stick: the right controller alone.
  axis[acvr_mono::axis_stick_x] = Unit(rollR, m_t.stickRollDeg);
  axis[acvr_mono::axis_stick_y] = Unit(PitchDeg(in.right), m_t.stickPitchDeg);
  // Skis: lean = mean roll (edge), twist = mean heading against the head (swing).
  axis[acvr_mono::axis_lean] = Unit(roll, m_t.leanDeg);
  const float headYaw = in.head.valid ? YawDeg(in.head) : 0.f;
  const float yawR = Wrap180(YawDeg(in.right) - headYaw);
  const float yaw = both ? 0.5f * (yawR + Wrap180(YawDeg(in.left) - headYaw)) : yawR;
  axis[acvr_mono::axis_twist] = Unit(yaw, m_t.twistDeg);

  // Pedalling: hands moving up and down in turn; the summed vertical speed, smoothed.
  float speed = 0;
  if (m_have && in.seconds > m_last) {
   const float dt = float(in.seconds - m_last);
   speed = std::abs(in.right.position[1] - m_prevR) / dt;
   if (both && m_haveL) speed += std::abs(in.left.position[1] - m_prevL) / dt;
   const float k = std::clamp(dt / m_t.pedalSmoothing, 0.f, 1.f);
   m_pedal += (speed - m_pedal) * k;
  }
  m_have = true; m_haveL = both; m_last = in.seconds;
  m_prevR = in.right.position[1]; if (both) m_prevL = in.left.position[1];
  axis[acvr_mono::axis_pedal] = std::max(std::clamp(m_pedal / m_t.pedalFull, 0.f, 1.f), axis[acvr_mono::axis_throttle]);
  return true;
 }
private:
 Tuning m_t;
 bool m_have = false, m_haveL = false;
 double m_last = 0;
 float m_prevR = 0, m_prevL = 0, m_pedal = 0;
};

} // namespace body_motion
