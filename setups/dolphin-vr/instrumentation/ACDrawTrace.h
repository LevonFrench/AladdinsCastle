// Copyright 2026 AladdinsCastle contributors
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <array>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <iomanip>
#include <limits>
#include <locale>
#include <memory>
#include <sstream>

namespace ACDrawTrace
{
// Emulated raster state only. No vertices, textures, pixels or memory dumps.
struct Sample
{
  std::uint32_t draw, primitive, indices, projection_type;
  std::array<float, 6> projection, viewport;
  std::uint32_t scissor_tl, scissor_br, scissor_offset, z_mode, blend_mode, gen_mode;
  std::uint64_t vertex_shader, pixel_shader;
};

inline double FovDegrees(float scale, float offset)
{
  if (!std::isfinite(scale) || !std::isfinite(offset) || scale <= 0.0f)
    return std::numeric_limits<double>::quiet_NaN();
  // For w=-z: slope=(NDC+offset)/scale. Handles off-centre frusta.
  constexpr double radians_to_degrees = 57.2957795130823208768;
  return (std::atan((1.0 + offset) / scale) -
          std::atan((-1.0 + offset) / scale)) * radians_to_degrees;
}

class Logger
{
public:
  Logger() : Logger(EnvironmentPath(), 30, 200000) {}
  explicit Logger(const std::filesystem::path& path, std::uint32_t interval = 30,
                  std::uint32_t max_rows = 200000)
      : m_interval(interval ? interval : 1), m_max_rows(max_rows)
  {
    if (path.empty())
      return;
#ifdef _WIN32
    if (_wfopen_s(&m_file, path.c_str(), L"wb") != 0)
      m_file = nullptr;
#else
    m_file = std::fopen(path.c_str(), "wb");
#endif
    if (!m_file)
    {
      std::fputs("AC draw trace: could not open requested output\n", stderr);
      return;
    }
    std::fputs("frame,elapsed_ms,submitted,draw,primitive,indices,projection_type,"
               "p0,p1,p2,p3,p4,p5,hfov_deg,vfov_deg,viewport_wd,viewport_ht,"
               "viewport_zrange,viewport_x,viewport_y,viewport_farz,scissor_tl,"
               "scissor_br,scissor_offset,z_mode,blend_mode,gen_mode,vs_uid_hash,ps_uid_hash\n", m_file);
  }
  ~Logger() { Close(); }
  Logger(const Logger&) = delete;
  Logger& operator=(const Logger&) = delete;

  bool IsSampling() const { return m_file && m_frame % m_interval == 0; }

  void Record(const Sample& s)
  {
    if (!m_file)
      return;
    const auto submitted = m_submitted++;
    if (m_frame % m_interval != 0)
      return;
    if (m_rows >= m_max_rows)
    {
      Close();
      return;
    }
    const auto& p = s.projection;
    const auto& v = s.viewport;
    const double nan = std::numeric_limits<double>::quiet_NaN();
    // GX perspective type is zero. Orthographic FoV is undefined.
    const double hfov = s.projection_type == 0 ? FovDegrees(p[0], p[1]) : nan;
    const double vfov = s.projection_type == 0 ? FovDegrees(p[2], p[3]) : nan;
    const double ms = std::chrono::duration<double, std::milli>(
        std::chrono::steady_clock::now() - m_start).count();
    // Dolphin selects a global UI locale. Keep CSV decimals/delimiters invariant.
    std::ostringstream line;
    line.imbue(std::locale::classic());
    line << std::setprecision(9) << m_frame << ',' << ms << ',' << submitted << ','
         << s.draw << ',' << s.primitive << ',' << s.indices << ',' << s.projection_type;
    for (const auto value : p)
      line << ',' << value;
    line << ',' << hfov << ',' << vfov;
    for (const auto value : v)
      line << ',' << value;
    line << ',' << s.scissor_tl << ',' << s.scissor_br << ',' << s.scissor_offset << ','
         << s.z_mode << ',' << s.blend_mode << ',' << s.gen_mode << ','
         << s.vertex_shader << ',' << s.pixel_shader << '\n';
    const std::string record = line.str();
    if (std::fwrite(record.data(), 1, record.size(), m_file) != record.size())
    {
      std::fputs("AC draw trace: output write failed\n", stderr);
      Close();
      return;
    }
    ++m_rows;
    if (m_rows == m_max_rows)
      Close();
  }

  void EndFrame()
  {
    if (m_file && m_frame % m_interval == 0 && std::fflush(m_file) != 0)
    {
      std::fputs("AC draw trace: output flush failed\n", stderr);
      Close();
    }
    ++m_frame;
    m_submitted = 0;
  }

private:
  static std::filesystem::path EnvironmentPath()
  {
#ifdef _WIN32
    wchar_t* value = nullptr;
    std::size_t length = 0;
    if (_wdupenv_s(&value, &length, L"AC_DOLPHIN_DRAW_LOG") != 0)
      return {};
    const std::unique_ptr<wchar_t, decltype(&std::free)> owned(value, &std::free);
    const auto* path = owned.get();
#else
    const auto* path = std::getenv("AC_DOLPHIN_DRAW_LOG");
#endif
    return path ? std::filesystem::path(path) : std::filesystem::path();
  }
  void Close()
  {
    if (m_file)
      std::fclose(m_file);
    m_file = nullptr;
  }
  std::FILE* m_file = nullptr;
  std::uint32_t m_interval, m_max_rows, m_rows = 0, m_submitted = 0;
  std::uint64_t m_frame = 0;
  const std::chrono::steady_clock::time_point m_start = std::chrono::steady_clock::now();
};
}  // namespace ACDrawTrace
