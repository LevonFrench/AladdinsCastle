// SPDX-License-Identifier: GPL-2.0-or-later
// AladdinsCastle offline input probe for DolphinXR, GameCube pad (test only, off unless env vars
// are set; does nothing while a Wii title runs, where ACInputProbe.h covers the Wii Remote).
//
// DOLPHINXR_INPUT_SCRIPT=<file>  feeds a scripted OpenXR controller state, keyed on emulated
//                                time, into Common::VR::OpenXRInputState just before the serial
//                                interface polls the controllers. The OpenXR ControllerInterface
//                                device (buttons, sticks, raw accel from the pose) reads that
//                                state, so the real GCPadNew.ini / per-game pad profile is used.
// DOLPHINXR_INPUT_PROBE=<file>   appends one line per change of what GameCube pad 1 reports to
//                                the game (buttons, main stick, C stick, analog L/R).
//
// Script lines: "<from_s> <to_s> <L|R> <control> <value>" or "shot <at_s> <name>".
// Controls: primary secondary menu thumbclick trigger squeeze stick_x stick_y
//           roll pitch yaw (degrees, pose orientation) ang_x ang_y ang_z.
// Same format as the Wii Remote probe (setups/dolphin-vr/motion-controls/input-probe).
#pragma once

#ifdef ENABLE_VR

#include <array>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

#include "Common/VR/OpenXRInputState.h"
#include "Core/Core.h"
#include "Core/System.h"
#include "InputCommon/GCPadStatus.h"

namespace ACPadProbe
{
struct Step
{
  double from = 0, to = 0;
  int hand = 1;
  std::string control;
  float value = 0;
};

struct Shot
{
  double at = 0;
  std::string name;
  bool done = false;
};

struct State
{
  bool loaded = false;
  bool script_on = false;
  std::vector<Step> steps;
  std::vector<Shot> shots;
  std::FILE* probe = nullptr;
  std::string last;
};

inline State& Get()
{
  static State s;
  if (!s.loaded)
  {
    s.loaded = true;
    if (const char* path = std::getenv("DOLPHINXR_INPUT_SCRIPT"); path && *path)
    {
      std::ifstream in(path);
      std::string line;
      while (std::getline(in, line))
      {
        if (line.empty() || line[0] == '#')
          continue;
        std::istringstream ls(line);
        std::string first;
        ls >> first;
        if (first == "shot")
        {
          Shot shot;
          ls >> shot.at >> shot.name;
          s.shots.push_back(shot);
          continue;
        }
        Step st;
        std::string hand;
        st.from = std::atof(first.c_str());
        ls >> st.to >> hand >> st.control >> st.value;
        st.hand = (hand == "L") ? 0 : 1;
        s.steps.push_back(st);
      }
      s.script_on = true;
    }
    if (const char* path = std::getenv("DOLPHINXR_INPUT_PROBE"); path && *path)
      s.probe = std::fopen(path, "a");
  }
  return s;
}

inline std::array<float, 4> Quat(float yaw_deg, float pitch_deg, float roll_deg)
{
  // q = yaw(Y) * pitch(X) * roll(Z), OpenXR axes (x right, y up, z back), xyzw.
  const float d = 3.14159265f / 360.0f;
  const float cy = std::cos(yaw_deg * d), sy = std::sin(yaw_deg * d);
  const float cp = std::cos(pitch_deg * d), sp = std::sin(pitch_deg * d);
  const float cr = std::cos(roll_deg * d), sr = std::sin(roll_deg * d);
  const float w1 = cy * cp, x1 = cy * sp, y1 = sy * cp, z1 = -sy * sp;
  return {x1 * cr + y1 * sr, y1 * cr - x1 * sr, z1 * cr + w1 * sr, w1 * cr - z1 * sr};
}

// Called on the CPU thread just before the serial interface updates the controller interface.
inline void Feed(double t)
{
  if (Core::System::GetInstance().IsWii())
    return;
  State& s = Get();
  if (!s.script_on)
    return;

  std::array<Common::VR::OpenXRControllerState, 2> c{};
  std::array<float, 2> yaw{}, pitch{}, roll{};
  for (int h = 0; h < 2; ++h)
  {
    c[h].connected = true;
    c[h].grip_pose.valid = true;
    c[h].grip_pose.position = {h == 0 ? -0.2f : 0.2f, 1.0f, -0.3f};
    c[h].aim_pose.valid = true;
    c[h].screen_hit = {true, 0.0f, 0.0f, 2.0f};
  }
  for (const Step& st : s.steps)
  {
    if (t < st.from || t >= st.to)
      continue;
    auto& k = c[st.hand];
    const bool on = st.value > 0.5f;
    if (st.control == "primary") k.primary_button = on;
    else if (st.control == "secondary") k.secondary_button = on;
    else if (st.control == "menu") k.menu_button = on;
    else if (st.control == "thumbclick") k.thumbstick_button = on;
    else if (st.control == "trigger") { k.trigger_value = st.value; k.trigger_button = on; }
    else if (st.control == "squeeze") { k.squeeze_value = st.value; k.squeeze_button = on; }
    else if (st.control == "stick_x") k.thumbstick_x = st.value;
    else if (st.control == "stick_y") k.thumbstick_y = st.value;
    else if (st.control == "yaw") yaw[st.hand] = st.value;
    else if (st.control == "pitch") pitch[st.hand] = st.value;
    else if (st.control == "roll") roll[st.hand] = st.value;
    else if (st.control == "ang_x") { k.grip_velocity.angular[0] = st.value; k.grip_velocity.angular_valid = true; }
    else if (st.control == "ang_y") { k.grip_velocity.angular[1] = st.value; k.grip_velocity.angular_valid = true; }
    else if (st.control == "ang_z") { k.grip_velocity.angular[2] = st.value; k.grip_velocity.angular_valid = true; }
  }
  for (int h = 0; h < 2; ++h)
  {
    c[h].aim_pose.orientation = Quat(yaw[h], pitch[h], roll[h]);
    c[h].grip_pose.orientation = c[h].aim_pose.orientation;
  }
  Common::VR::OpenXRInputState::SetControllers(c, true, {}, {"probe", "probe"}, true,
                                               static_cast<int64_t>(t * 1e9));

  for (Shot& shot : s.shots)
  {
    if (!shot.done && t >= shot.at)
    {
      shot.done = true;
      Core::SaveScreenShot(shot.name);
      if (s.probe)
        std::fprintf(s.probe, "t=%.3f shot %s\n", t, shot.name.c_str());
    }
  }
}

// Called after GameCube pad 1's status was read for the game.
inline void Record(double t, const GCPadStatus& p)
{
  if (Core::System::GetInstance().IsWii())
    return;
  State& s = Get();
  if (!s.probe)
    return;
  char buf[256];
  std::snprintf(buf, sizeof(buf), "pad buttons=%04x stick=%u,%u cstick=%u,%u l=%u r=%u",
                p.button & 0x1f7f, p.stickX, p.stickY, p.substickX, p.substickY, p.triggerLeft,
                p.triggerRight);
  if (s.last != buf)
  {
    s.last = buf;
    std::fprintf(s.probe, "t=%.3f %s\n", t, buf);
    std::fflush(s.probe);
  }
}
}  // namespace ACPadProbe

#endif  // ENABLE_VR
