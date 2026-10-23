// SPDX-License-Identifier: GPL-2.0-or-later
// Strict, bounded append-inbox parser. Standard library only; no engine/runtime calls.
#pragma once

#include <cmath>
#include <cstddef>
#include <fstream>
#include <sstream>
#include <string>

namespace ACInputProbe::Append
{
inline constexpr std::size_t MAX_BYTES = 64 * 1024;
inline constexpr std::size_t MAX_LINE_BYTES = 256;
inline constexpr std::size_t MAX_LINES = 2048;
inline constexpr std::size_t MAX_ACTIONS = 1024;
inline constexpr std::size_t MAX_TOTAL_STEPS = 2048;
inline constexpr std::size_t MAX_TOTAL_SHOTS = 512;
inline constexpr double MAX_FUTURE_SECONDS = 600;
inline constexpr double MAX_STEP_SECONDS = 120;

struct Command
{
  bool shot = false;
  double from = 0, to = 0;
  int hand = 1;
  std::string control, name;
  float value = 0;
};

enum class Result { Ignore, Accept, Reject };

inline bool SameName(const std::string& a, const std::string& b)
{
  if (a.size() != b.size())
    return false;
  auto upper = [](unsigned char c) { return c >= 'a' && c <= 'z' ? c - 'a' + 'A' : c; };
  for (std::size_t i = 0; i < a.size(); ++i)
    if (upper(static_cast<unsigned char>(a[i])) != upper(static_cast<unsigned char>(b[i])))
      return false;
  return true;
}

inline bool SafeName(const std::string& name)
{
  if (name.empty() || name.size() > 64)
    return false;
  std::string upper;
  for (const unsigned char c : name)
  {
    if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
          (c >= '0' && c <= '9') || c == '_' || c == '-'))
      return false;
    upper.push_back(static_cast<char>(c >= 'a' && c <= 'z' ? c - 'a' + 'A' : c));
  }
  if (upper == "CON" || upper == "PRN" || upper == "AUX" || upper == "NUL")
    return false;
  if (upper.size() == 4 && (upper.substr(0, 3) == "COM" || upper.substr(0, 3) == "LPT") &&
      upper[3] >= '1' && upper[3] <= '9')
    return false;
  return true;
}

inline Result ParseLine(const std::string& line, double now, Command& out, std::string& reason)
{
  auto reject = [&](const char* why) { reason = why; return Result::Reject; };
  out = {};
  if (line.size() > MAX_LINE_BYTES)
    return reject("line_too_long");
  for (const unsigned char c : line)
    if (c == 0 || (c < 32 && c != '\t' && c != '\r'))
      return reject("invalid_character");
  std::istringstream input(line);
  std::string first, extra;
  if (!(input >> first) || first[0] == '#')
    return Result::Ignore;
  if (!std::isfinite(now) || now < 0)
    return reject("invalid_clock");
  if (first == "shot")
  {
    if (!(input >> out.from >> out.name) || (input >> extra))
      return reject("invalid_shot_syntax");
    if (!std::isfinite(out.from) || out.from < now || out.from > now + MAX_FUTURE_SECONDS)
      return reject("shot_time_not_future_or_out_of_range");
    if (!SafeName(out.name))
      return reject("unsafe_shot_name");
    out.shot = true;
    return Result::Accept;
  }
  std::istringstream start(first);
  std::string hand;
  double value = 0;
  if (!(start >> out.from) || (start >> extra) ||
      !(input >> out.to >> hand >> out.control >> value) || (input >> extra))
    return reject("invalid_step_syntax");
  if (!std::isfinite(out.from) || !std::isfinite(out.to) || !std::isfinite(value) ||
      out.from < now || out.to <= out.from || out.from > now + MAX_FUTURE_SECONDS ||
      out.to > now + MAX_FUTURE_SECONDS || out.to - out.from > MAX_STEP_SECONDS)
    return reject("step_time_or_value_out_of_range");
  if (hand != "L" && hand != "R")
    return reject("invalid_hand");
  out.hand = hand == "L" ? 0 : 1;
  if (out.control == "menu")
    return reject("reserved_menu_control");
  double low = 0, high = 0;
  if (out.control == "primary" || out.control == "secondary" || out.control == "thumbclick")
  {
    if (value != 0 && value != 1)
      return reject("button_value_not_boolean");
    high = 1;
  }
  else if (out.control == "trigger" || out.control == "squeeze")
    high = 1;
  else if (out.control == "stick_x" || out.control == "stick_y")
    low = -1, high = 1;
  else if (out.control == "u" || out.control == "v")
    low = -5, high = 5;
  else if (out.control == "roll" || out.control == "pitch" || out.control == "yaw")
    low = -360, high = 360;
  else if (out.control == "ang_x" || out.control == "ang_y" || out.control == "ang_z")
    low = -100, high = 100;
  else
    return reject("unknown_control");
  if (value < low || value > high)
    return reject("control_value_out_of_range");
  out.value = static_cast<float>(value);
  return Result::Accept;
}

class Inbox
{
public:
  bool Disabled() const { return m_disabled; }
  bool Ready() const { return m_ready; }

  template <typename Accept, typename Log>
  void Poll(const std::string& path, double now, Accept accept, Log log)
  {
    if (m_disabled)
      return;
    auto disable = [&](const char* reason) {
      m_disabled = true;
      log(std::string("AC_INPUT_APPEND disabled reason=") + reason);
    };
    if (!std::isfinite(now) || now < 0 || now < m_last_time)
    {
      disable("clock_rewind_or_invalid");
      return;
    }
    m_last_time = now;
    if (now < m_next_poll)
      return;
    m_next_poll = now + 1;  // At most once per emulated second, even when opening fails.
    if (!m_input.is_open())
    {
      m_input.clear();
      m_input.open(path, std::ios::in | std::ios::binary);
      if (!m_input.is_open())
        return;
    }
    m_input.clear();
    m_input.seekg(0, std::ios::end);
    const std::streamoff size = static_cast<std::streamoff>(m_input.tellg());
    if (size < m_offset || size < 0)
    {
      disable("truncated_or_unreadable");
      return;
    }
    if (size > static_cast<std::streamoff>(MAX_BYTES))
    {
      disable("byte_cap");
      return;
    }
    if (!m_ready)
    {
      log("AC_INPUT_APPEND ready");
      m_ready = true;
    }
    const auto count = static_cast<std::size_t>(size - m_offset);
    if (count == 0)
      return;
    std::string bytes(count, '\0');
    m_input.seekg(m_offset, std::ios::beg);
    m_input.read(bytes.data(), static_cast<std::streamsize>(count));
    const auto read = m_input.gcount();
    if (read != static_cast<std::streamsize>(count))
    {
      disable("read_failure");
      return;
    }
    m_offset += read;  // Never reset/re-read old bytes or re-open a replaced file.
    bytes.resize(static_cast<std::size_t>(read));
    for (const char c : bytes)
    {
      if (c != '\n')
      {
        if (m_pending.size() >= MAX_LINE_BYTES)
        {
          disable("line_byte_cap");
          return;
        }
        m_pending.push_back(c);
        continue;
      }
      if (++m_lines > MAX_LINES)
      {
        disable("line_cap");
        return;
      }
      Command command;
      std::string reason;
      const Result result = ParseLine(m_pending, now, command, reason);
      m_pending.clear();  // Partial final lines remain pending until a later newline.
      if (result == Result::Reject)
        log("AC_INPUT_APPEND rejected line=" + std::to_string(m_lines) + " reason=" + reason);
      if (result != Result::Accept)
        continue;
      if (m_actions >= MAX_ACTIONS)
      {
        disable("action_cap");
        return;
      }
      if (accept(command))
        ++m_actions;
    }
  }

private:
  std::ifstream m_input;
  std::streamoff m_offset = 0;
  std::string m_pending;
  double m_next_poll = 0, m_last_time = 0;
  std::size_t m_lines = 0, m_actions = 0;
  bool m_ready = false, m_disabled = false;
};
}  // namespace ACInputProbe::Append
