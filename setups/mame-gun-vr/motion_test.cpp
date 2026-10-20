// SPDX-License-Identifier: GPL-3.0-or-later
// Offline test of the body-control mapping: scripted controller poses -> cabinet axes.
#include "BodyMotion.h"
#include <cstdio>
#include <cstdlib>

using namespace body_motion;

static int failures = 0;
static void Expect(const char* what, float got, float want, float tol = 0.02f) {
 const bool ok = std::abs(got - want) <= tol;
 std::printf("%-44s %+.3f (want %+.3f) %s\n", what, got, want, ok ? "ok" : "FAIL");
 if (!ok) ++failures;
}
// Quaternion for an angle (degrees) about a unit axis.
static Pose Turned(float ax, float ay, float az, float deg, float y = 1.0f) {
 Pose p; p.valid = true; const float h = deg / kDeg / 2, s = std::sin(h);
 p.orientation[0] = ax * s; p.orientation[1] = ay * s; p.orientation[2] = az * s; p.orientation[3] = std::cos(h);
 p.position[1] = y; return p;
}
static Pose Level(float y = 1.0f) { return Turned(0, 1, 0, 0, y); }

int main() {
 float a[acvr_mono::motion_axes];
 {  // roll: rotation about the forward axis (-z); right side down = negative angle about +z... use -z axis
  Mapper m; Input in; in.head = Level(1.6f); in.right = Turned(0, 0, -1, 30); in.rightTrigger = 0.75f; in.leftTrigger = 0.25f;
  m.Map(in, a);
  Expect("right roll 30 deg, one hand: steer", a[acvr_mono::axis_steer], 0.5f);
  Expect("right roll 30 deg: stick x (45 full)", a[acvr_mono::axis_stick_x], 0.667f);
  Expect("right trigger 0.75: throttle", a[acvr_mono::axis_throttle], 0.75f);
  Expect("left trigger 0.25: brake", a[acvr_mono::axis_brake], 0.25f);
  Expect("left trigger 0.25: throttle lever", a[acvr_mono::axis_throttle2], 0.25f);
 }
 {
  Mapper m; Input in; in.head = Level(1.6f); in.right = Turned(0, 0, -1, -30); in.left = Turned(0, 0, -1, -30);
  m.Map(in, a);
  Expect("both hands roll 30 deg left: steer", a[acvr_mono::axis_steer], -0.5f);
  Expect("both hands roll 30 deg left: lean (35 full)", a[acvr_mono::axis_lean], -0.857f);
 }
 {  // pitch: rotation about +x lifts the nose
  Mapper m; Input in; in.head = Level(1.6f); in.right = Turned(1, 0, 0, 20);
  m.Map(in, a);
  Expect("right pitched up 20 deg: stick y (40 full)", a[acvr_mono::axis_stick_y], 0.5f);
 }
 {  // twist: heading right = rotation about -y
  Mapper m; Input in; in.head = Level(1.6f); in.right = Turned(0, -1, 0, 17.5f); in.left = Turned(0, -1, 0, 17.5f);
  m.Map(in, a);
  Expect("both hands turned right 17.5 deg: twist", a[acvr_mono::axis_twist], 0.5f);
  in.head = Turned(0, -1, 0, 17.5f, 1.6f); m.Map(in, a);
  Expect("same, head turned with them: twist", a[acvr_mono::axis_twist], 0.0f);
 }
 {  // pedalling: hands 0.3 m apart in height moving in turn at 1 Hz
  Mapper m; Input in; in.head = Level(1.6f);
  for (int i = 0; i <= 180; ++i) {
   const double t = i / 90.0; const float s = 0.15f * std::sin(float(t) * 6.2832f);
   in.seconds = t; in.right = Level(1.0f + s); in.left = Level(1.0f - s); m.Map(in, a);
  }
  // mean |dy/dt| per hand = 4 * 0.15 m * 1 Hz = 0.6 m/s; two hands 1.2 m/s = full scale
  Expect("alternating hands at 1 Hz, 0.3 m: pedal", a[acvr_mono::axis_pedal], 1.0f, 0.12f);
  for (int i = 181; i <= 360; ++i) { in.seconds = i / 90.0; in.right = Level(1.0f); in.left = Level(1.0f); m.Map(in, a); }
  Expect("hands still for 2 s: pedal", a[acvr_mono::axis_pedal], 0.0f, 0.02f);
  in.rightTrigger = 0.6f; in.seconds += 1 / 90.0; m.Map(in, a);
  Expect("hands still, right trigger 0.6: pedal", a[acvr_mono::axis_pedal], 0.6f);
 }
 {  // default stick-and-trigger controls
  Mapper m; Input in; in.leftStick[0] = 0.55f; in.leftStick[1] = -0.55f; in.rightStick[0] = -1.0f; in.rightTrigger = 0.9f; in.leftTrigger = 0.3f;
  m.MapSticks(in, a);
  Expect("sticks: left x 0.55 -> steer (0.1 deadzone)", a[acvr_mono::axis_steer], 0.5f);
  Expect("sticks: left y pulled back 0.55 -> stick y", a[acvr_mono::axis_stick_y], 0.5f);
  Expect("sticks: left x -> ski swing", a[acvr_mono::axis_twist], 0.5f);
  Expect("sticks: right x -1 -> ski edge", a[acvr_mono::axis_lean], -1.0f);
  Expect("sticks: right trigger -> throttle", a[acvr_mono::axis_throttle], 0.9f);
  Expect("sticks: right trigger -> pedal", a[acvr_mono::axis_pedal], 0.9f);
  Expect("sticks: left trigger -> brake", a[acvr_mono::axis_brake], 0.3f);
  in.leftStick[0] = 0.08f; m.MapSticks(in, a);
  Expect("sticks: left x 0.08 inside deadzone -> steer", a[acvr_mono::axis_steer], 0.0f);
 }
 std::printf("%s\n", failures ? "FAILED" : "all passed");
 return failures ? 1 : 0;
}
