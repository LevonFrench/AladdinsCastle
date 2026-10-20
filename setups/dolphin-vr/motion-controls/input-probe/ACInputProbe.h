// SPDX-License-Identifier: GPL-2.0-or-later
// AladdinsCastle offline input probe for DolphinXR (test only, off unless env vars are set).
//
// DOLPHINXR_INPUT_SCRIPT=<file>  feeds a scripted OpenXR controller state, keyed on emulated
//                                time, into Common::VR::OpenXRInputState when no XR session
//                                runs. Both the OpenXR ControllerInterface device (buttons,
//                                sticks) and the Wii Remote override (pointer, accel, gyro)
//                                read that state, so the real WiimoteNew.ini is exercised.
// DOLPHINXR_INPUT_PROBE=<file>   appends one line per change of what emulated Wii Remote 1
//                                reports to the game (buttons, accel, IR dot, MotionPlus,
//                                Nunchuk stick/buttons/accel).
//
// Script lines: "<from_s> <to_s> <L|R> <control> <value>" or "shot <at_s> <name>".
// Controls: primary secondary menu thumbclick trigger squeeze stick_x stick_y
//           roll pitch yaw (degrees, aim orientation) u v (screen hit) ang_x ang_y ang_z.
#pragma once

#ifdef ENABLE_VR

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <sstream>
#include <string>
#include <variant>
#include <vector>

#include "Common/VR/OpenXRInputState.h"
#include "Core/Core.h"
#include "Core/HW/WiimoteEmu/DesiredWiimoteState.h"

namespace ACInputProbe
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
  // yaw*pitch
  const float w1 = cy * cp, x1 = cy * sp, y1 = sy * cp, z1 = -sy * sp;
  // (yaw*pitch)*roll
  return {x1 * cr + y1 * sr, y1 * cr - x1 * sr, z1 * cr + w1 * sr, w1 * cr - z1 * sr};
}

// Called on the emulation thread just before the Bluetooth input poll.
inline void Feed(double t)
{
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
    else if (st.control == "u") k.screen_hit.u = st.value;
    else if (st.control == "v") k.screen_hit.v = st.value;
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

// Called after Wii Remote 1 built the state it reports to the game.
inline void Record(double t, const WiimoteEmu::DesiredWiimoteState& w)
{
  State& s = Get();
  if (!s.probe)
    return;
  char buf[512];
  const auto& a = w.acceleration.value;
  const auto& ir = w.camera_points[0];
  int len = std::snprintf(buf, sizeof(buf), "buttons=%04x accel=%u,%u,%u ir0=%u,%u",
                          w.buttons.hex & 0x9f1f, a.x, a.y, a.z, ir.position.x, ir.position.y);
  if (w.motion_plus)
  {
    const auto& g = w.motion_plus->gyro.value;
    len += std::snprintf(buf + len, sizeof(buf) - len, " mplus=%u,%u,%u", g.x, g.y, g.z);
  }
  if (const auto* n = std::get_if<WiimoteEmu::Nunchuk::DataFormat>(&w.extension.data))
  {
    const u8 b = n->GetButtons();
    len += std::snprintf(buf + len, sizeof(buf) - len, " nunchuk stick=%u,%u c=%d z=%d accel=%u,%u,%u",
                         n->jx, n->jy, (b & WiimoteEmu::Nunchuk::BUTTON_C) ? 1 : 0,
                         (b & WiimoteEmu::Nunchuk::BUTTON_Z) ? 1 : 0, n->GetAccelX(),
                         n->GetAccelY(), n->GetAccelZ());
  }
  else
  {
    len += std::snprintf(buf + len, sizeof(buf) - len, " ext=%zu", w.extension.data.index());
  }
  if (s.last != buf)
  {
    s.last = buf;
    std::fprintf(s.probe, "t=%.3f %s\n", t, buf);
    std::fflush(s.probe);
  }
}
}  // namespace ACInputProbe

#endif  // ENABLE_VR
