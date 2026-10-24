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
// DOLPHINXR_INPUT_APPEND=<file> optional append-only future commands, synthetic offline only.
// DOLPHINXR_INPUT_SAVE_AT / SAVE_PATH request one observation-only private checkpoint.
// DOLPHINXR_INPUT_CHECKPOINT_DIR pairs safe checkpoint-* shots with private states, offline only.
//
// Script lines: "<from_s> <to_s> <L|R> <control> <value>" or "shot <at_s> <name>".
// Controls: primary secondary menu thumbclick trigger squeeze stick_x stick_y
//           roll pitch yaw (degrees, aim orientation) u v (screen hit) ang_x ang_y ang_z
//           lin_x lin_y lin_z (grip linear velocity, meters/second; each component is bounded).
#pragma once

#ifdef ENABLE_VR

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <filesystem>
#include <sstream>
#include <string>
#include <variant>
#include <vector>

#include "Common/VR/OpenXRInputState.h"
#include "Core/ACInputAppend.h"
#include "Core/Core.h"
#include "Core/HW/WiimoteEmu/DesiredWiimoteState.h"
#include "Core/State.h"
#include "VideoCommon/VR/OpenXRManager.h"

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
  std::string append_path;
  Append::Inbox inbox;
  std::size_t fixed_steps = 0, fixed_shots = 0;
  bool append_purged = false;
  bool save_requested = false;
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
    if (const char* path = std::getenv("DOLPHINXR_INPUT_APPEND"); path && *path)
      s.append_path = path;  // No file access until Feed validates the offline gate.
    s.fixed_steps = s.steps.size();
    s.fixed_shots = s.shots.size();
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

  // Feed's BTEmu call site is only ENABLE_VR-guarded. Restrict the NEW inbox
  // explicitly; never infer offline status from the synthetic input snapshot.
  const char* synthetic = std::getenv("AC_STEREO_PROBE");
  if (!s.save_requested && s.probe && synthetic && std::string(synthetic) == "1" && !VR::g_openxr)
  {
    const char* at = std::getenv("DOLPHINXR_INPUT_SAVE_AT");
    const char* path = std::getenv("DOLPHINXR_INPUT_SAVE_PATH");
    char* end = nullptr;
    const double when = at ? std::strtod(at, &end) : -1;
    if (at && end != at && *end == '\0' && std::isfinite(when) && when >= 0 && path && *path && t >= when)
    {
      s.save_requested = true;
      // Leave the input callback before serializing the emulated machine.
      Core::QueueHostJob([filename = std::string(path)](Core::System& system) {
        const char* gate = std::getenv("AC_STEREO_PROBE");
        if (gate && std::string(gate) == "1" && !VR::g_openxr)
          ::State::SaveAs(system, filename);
      });
      std::fprintf(s.probe, "t=%.3f AC_INPUT_STATE save_requested\n", t);
      std::fflush(s.probe);
    }
  }
  if (!s.append_path.empty() && !s.append_purged && s.probe && synthetic && std::string(synthetic) == "1" &&
      !VR::g_openxr)
  {
    auto log = [&](const std::string& message) {
      std::fprintf(s.probe, "t=%.3f %s\n", t, message.c_str());
      std::fflush(s.probe);
    };
    s.inbox.Poll(s.append_path, t, [&](const Append::Command& command) {
      if (command.shot)
      {
        for (const Shot& old : s.shots)
          if (Append::SameName(old.name, command.name))
          {
            log("AC_INPUT_APPEND rejected reason=duplicate_shot_name");
            return false;
          }
        if (s.shots.size() >= Append::MAX_TOTAL_SHOTS)
        {
          log("AC_INPUT_APPEND rejected reason=shot_cap");
          return false;
        }
        s.shots.push_back({command.from, command.name, false});
        log("AC_INPUT_APPEND accepted shot " + std::to_string(command.from) + " " + command.name);
      }
      else
      {
        if (s.steps.size() >= Append::MAX_TOTAL_STEPS)
        {
          log("AC_INPUT_APPEND rejected reason=step_cap");
          return false;
        }
        s.steps.push_back({command.from, command.to, command.hand, command.control, command.value});
        log("AC_INPUT_APPEND accepted step " + std::to_string(command.from) + " " +
            std::to_string(command.to) + " " + (command.hand == 0 ? "L " : "R ") +
            command.control + " " + std::to_string(command.value));
      }
      return true;
    }, log);
    if (s.inbox.Disabled() && !s.append_purged)
    {
      s.steps.resize(s.fixed_steps);
      s.shots.resize(s.fixed_shots);
      s.append_purged = true;
      log("AC_INPUT_APPEND purged appended actions; fixed replay retained");
    }
  }
  else if (s.inbox.Ready() && !s.append_purged)
  {
    // A manager appearing or the opt-in gate changing revokes appended input.
    // Never reactivate the inbox or replay its already consumed bytes.
    s.steps.resize(s.fixed_steps);
    s.shots.resize(s.fixed_shots);
    s.append_purged = true;
    if (s.probe)
    {
      std::fprintf(s.probe, "t=%.3f AC_INPUT_APPEND disabled reason=offline_gate_lost\n", t);
      std::fflush(s.probe);
    }
  }

  std::array<Common::VR::OpenXRControllerState, 2> c{};
  std::array<float, 2> yaw{}, pitch{}, roll{};
  std::array<bool, 2> scripted_linear_velocity{};
  for (const Step& st : s.steps)
  {
    if (st.control == "lin_x" || st.control == "lin_y" || st.control == "lin_z")
      scripted_linear_velocity[st.hand] = true;
  }
  for (int h = 0; h < 2; ++h)
  {
    c[h].connected = true;
    c[h].grip_pose.valid = true;
    c[h].grip_pose.position = {h == 0 ? -0.2f : 0.2f, 1.0f, -0.3f};
    // When a script uses linear velocity, publish zero outside active steps too. The
    // velocity edge back to zero is needed for the existing OpenXR-to-accelerometer
    // derivative path to see both the start and end of a synthetic movement pulse.
    c[h].grip_velocity.linear_valid = scripted_linear_velocity[h];
    c[h].aim_pose.valid = true;
    c[h].screen_hit = {true, 0.0f, 0.0f, 2.0f};
  }
  const auto bounded_linear_velocity = [](float value) {
    if (!std::isfinite(value))
      return 0.0f;
    const float limit = static_cast<float>(Append::MAX_LINEAR_VELOCITY_MPS);
    return std::clamp(value, -limit, limit);
  };
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
    else if (st.control == "lin_x")
    {
      k.grip_velocity.linear[0] = bounded_linear_velocity(st.value);
      k.grip_velocity.linear_valid = true;
    }
    else if (st.control == "lin_y")
    {
      k.grip_velocity.linear[1] = bounded_linear_velocity(st.value);
      k.grip_velocity.linear_valid = true;
    }
    else if (st.control == "lin_z")
    {
      k.grip_velocity.linear[2] = bounded_linear_velocity(st.value);
      k.grip_velocity.linear_valid = true;
    }
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
      const char* directory = std::getenv("DOLPHINXR_INPUT_CHECKPOINT_DIR");
      const char* gate = std::getenv("AC_STEREO_PROBE");
      if (directory && *directory && gate && std::string(gate) == "1" && !VR::g_openxr &&
          s.probe && shot.name.rfind("checkpoint-", 0) == 0 && Append::SafeName(shot.name))
      {
        const std::string filename = (std::filesystem::path(directory) / (shot.name + ".sav")).string();
        Core::QueueHostJob([filename](Core::System& system) {
          const char* opt_in = std::getenv("AC_STEREO_PROBE");
          std::error_code error;
          if (opt_in && std::string(opt_in) == "1" && !VR::g_openxr &&
              !std::filesystem::exists(filename, error) && !error)
            ::State::SaveAs(system, filename);
        });
        std::fprintf(s.probe, "t=%.3f AC_INPUT_STATE checkpoint_requested %s\n", t, shot.name.c_str());
        std::fflush(s.probe);
      }
      if (s.probe)
      {
        std::fprintf(s.probe, "t=%.3f shot %s\n", t, shot.name.c_str());
        if (!s.append_path.empty())
          std::fflush(s.probe);
      }
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
