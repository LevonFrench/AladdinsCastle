// Copyright 2026 AladdinsCastle contributors
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <array>
#include <cmath>
#include <optional>

namespace ACFacing
{
using Vector = std::array<float, 3>;
using Rotation = std::array<float, 9>;

inline std::optional<float> Heading(const std::array<float, 4>& quaternion)
{
  float norm = 0;
  for (const float value : quaternion)
  {
    if (!std::isfinite(value))
      return {};
    norm += value * value;
  }
  if (!std::isfinite(norm) || norm < 1e-8f)
    return {};
  const float x = quaternion[0], y = quaternion[1];
  const float z = quaternion[2], w = quaternion[3];
  const float forward_x = -2 * (x * z + w * y) / norm;
  const float forward_z = -1 + 2 * (x * x + y * y) / norm;
  if (forward_x * forward_x + forward_z * forward_z < 1e-8f)
    return {};  // Looking straight up/down has no horizontal heading.
  return std::atan2(-forward_x, -forward_z);
}

inline Vector ToGame(Vector value, float yaw)
{
  const float c = std::cos(yaw), s = std::sin(yaw);
  return {c * value[0] - s * value[2], value[1], s * value[0] + c * value[2]};
}

inline Rotation ToGame(Rotation rotation, float yaw)
{
  for (int column = 0; column < 3; ++column)
  {
    const auto transformed = ToGame(Vector{rotation[column], rotation[3 + column],
                                           rotation[6 + column]}, yaw);
    for (int row = 0; row < 3; ++row)
      rotation[row * 3 + column] = transformed[row];
  }
  return rotation;
}

inline Rotation Compose(const Rotation& anchor, const Rotation& rotation)
{
  Rotation result{};
  for (int row = 0; row < 3; ++row)
    for (int column = 0; column < 3; ++column)
      for (int axis = 0; axis < 3; ++axis)
        result[row * 3 + column] += anchor[row * 3 + axis] * rotation[axis * 3 + column];
  return result;
}

// Caller serializes access. Presence and focus are separate facts: focus alone
// never establishes donning. Unsupported presence requires explicit confirmation.
class State
{
public:
  enum class Presence { Unknown, Absent, Present };
  void ResetSession(bool presence_supported)
  {
    m_supported = presence_supported;
    m_presence = Presence::Unknown;
    m_manual = m_automatic = false;
    m_yaw = 0;
    m_captured = false;
  }
  void ResetAnchor()
  {
    m_yaw = 0;
    m_captured = false;
    m_manual = false;
    m_automatic = m_supported && m_presence == Presence::Present;
  }
  void ObservePresence(bool present)
  {
    if (!m_supported)
      return;
    if (present && m_presence != Presence::Present)
      m_automatic = true;  // Initial known present, don or redon.
    if (!present)
      m_automatic = false; // Cancel a don that ended before a valid pose.
    m_presence = present ? Presence::Present : Presence::Absent;
  }
  void Request() { m_manual = true; }
  bool Pending() const { return m_manual || m_automatic; }
  bool Supported() const { return m_supported; }
  Presence GetPresence() const { return m_presence; }
  bool Captured() const { return m_captured; }
  float Yaw() const { return m_yaw; }

  bool Apply(bool focused, bool position_valid, bool orientation_valid, bool two_eyes,
             Vector center, std::array<float, 4> orientation)
  {
    if (!Pending() || !focused || !position_valid || !orientation_valid || !two_eyes ||
        (m_supported && m_presence == Presence::Absent))
      return false;
    for (const float value : center)
      if (!std::isfinite(value))
        return false;
    const auto yaw = Heading(orientation);
    if (!yaw)
      return false;
    m_yaw = *yaw;
    m_captured = true;
    m_manual = m_automatic = false;
    return true;
  }

private:
  bool m_supported = false, m_manual = false, m_automatic = false, m_captured = false;
  Presence m_presence = Presence::Unknown;
  float m_yaw = 0;
};
}  // namespace ACFacing
