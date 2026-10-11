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
#include <locale>
#include <sstream>
#include <string>
#include <type_traits>
#include <fcntl.h>
#ifdef _WIN32
#include <io.h>
#include <share.h>
#include <sys/stat.h>
#else
#include <unistd.h>
#endif

namespace ACVRRouteTrace
{
// Numeric raster/resource metadata only; no pixels, vertices, names or addresses.
struct Texture
{
  int bound = 0, efb = -1, xfb = -1, width = -1, height = -1, layers = -1;
  int stereo_efb_reference = -1;
};
struct Sample
{
  std::uint32_t draw = 0, projection_type = 0, viewport_class = 0;
  bool active = false, flat = false, virtual_screen = false, panes = false, detect_targets = false;
  bool pane_override = false;
  int auto_efb_stage = -1;
  float explicit_override = NAN, route = NAN, units_per_meter = NAN, effective_units_per_meter = NAN;
  float display_aspect = NAN;
  std::array<float, 6> projection{}, viewport{};
  std::array<int, 5> frame{}; // valid, left, top, width, height
  std::array<float, 4> screen{}, depth{};
  std::array<float, 24> eye_rows{}; // cached GS eye tables; final route may use another table
  std::uint32_t scissor_tl = 0, scissor_br = 0, scissor_offset = 0, z_mode = 0, blend_mode = 0;
  std::uint32_t gen_mode = 0, used_texture_mask = 0;
  std::uint64_t vs_uid = 0, ps_uid = 0;
  std::array<Texture, 8> textures{};
};

inline const char* Reason(const Sample& s)
{
  if (!s.active) return "inactive";
  if (s.flat) return "flat";
  if (s.auto_efb_stage >= 0) return "auto-efb-native-request";
  if (std::isfinite(s.explicit_override)) return "explicit-per-draw-request";
  if (s.pane_override)
    return s.projection_type == 0 && s.frame[0] != 0 && (s.route == 2 || s.route == 3) ?
        "pane-route-with-request" : "pane-request-not-applied";
  // VRViewportClass in the pinned source: HudElement=3, RenderToTexture=4, Offscreen=5.
  if (s.detect_targets && (s.viewport_class == 4 || s.viewport_class == 5) && s.route == 0)
    return "preserved-target";
  if (s.projection_type == 0 && s.virtual_screen && s.panes && s.viewport_class == 3 && s.route == -1)
    return "small-pane-screen";
  if (s.projection_type == 0 && s.route == 1) return "perspective-world";
  if (s.projection_type != 0 && s.virtual_screen && s.route == -1) return "ortho-screen";
  return "unclassified";
}

inline std::string Columns()
{
  std::ostringstream out;
  out << "frame,elapsed_ms,draw,projection_type,viewport_class,reason,active,flat,virtual_screen,"
         "panes,detect_targets,pane_override,auto_efb_stage,explicit_override,final_route,"
         "config_units_per_meter,cached_units_per_meter,display_aspect";
  for (const char* group : {"projection", "viewport"})
    for (int i = 0; i < 6; ++i) out << ',' << group << i;
  out << ",frame_valid,frame_left,frame_top,frame_width,frame_height";
  for (const char* group : {"cached_screen", "cached_depth"})
    for (int i = 0; i < 4; ++i) out << ',' << group << i;
  for (int i = 0; i < 24; ++i) out << ",cached_eye_row" << i;
  out << ",scissor_tl,scissor_br,scissor_offset,z_mode,blend_mode,gen_mode,used_texture_mask,vs_uid,ps_uid";
  for (int stage = 0; stage < 8; ++stage)
    for (const char* key : {"bound","efb","xfb","width","height","layers","stereo_efb_ref"})
      out << ",tex" << stage << '_' << key;
  out << '\n';
  return out.str();
}

inline std::string Serialize(const Sample& s, std::uint64_t frame, double elapsed_ms)
{
  Sample fields = s;
  if (!s.active || s.flat || !std::isfinite(s.route) || s.route == 0)
  {
    fields.effective_units_per_meter = NAN;
    fields.screen.fill(NAN); fields.depth.fill(NAN); fields.eye_rows.fill(NAN);
  }
  std::ostringstream out;
  out.imbue(std::locale::classic());
  out << std::setprecision(9) << frame << ',';
  if (std::isfinite(elapsed_ms)) out << elapsed_ms; else out << "NA";
  out << ',' << s.draw << ','
      << s.projection_type << ',' << s.viewport_class << ',' << Reason(s) << ','
      << s.active << ',' << s.flat << ',' << s.virtual_screen << ',' << s.panes << ','
      << s.detect_targets << ',' << s.pane_override << ',' << s.auto_efb_stage;
  const auto number = [&out](auto value) {
    out << ',';
    if constexpr (std::is_floating_point_v<decltype(value)>)
    { if (!std::isfinite(value)) { out << "NA"; return; } }
    out << value;
  };
  number(s.explicit_override); number(s.route); number(s.units_per_meter);
  number(fields.effective_units_per_meter); number(s.display_aspect);
  const auto values = [&number](const auto& array) { for (const auto value : array) number(value); };
  values(s.projection); values(s.viewport); values(s.frame); values(fields.screen); values(fields.depth);
  values(fields.eye_rows);
  out << ',' << s.scissor_tl << ',' << s.scissor_br << ',' << s.scissor_offset << ',' << s.z_mode
      << ',' << s.blend_mode << ',' << s.gen_mode << ',' << s.used_texture_mask
      << ',' << s.vs_uid << ',' << s.ps_uid;
  for (const auto& t : s.textures)
    out << ',' << t.bound << ',' << t.efb << ',' << t.xfb << ',' << t.width << ',' << t.height
        << ',' << t.layers << ',' << t.stereo_efb_reference;
  out << '\n';
  return out.str();
}

struct Limits
{
  std::uint64_t interval = 30, rows = 100000, bytes = 32 * 1024 * 1024;
  double start_seconds = 0, duration_seconds = 600;
};

class Logger
{
public:
  Logger() : Logger(EnvironmentPath(), EnvironmentLimits()) {}
  explicit Logger(const std::filesystem::path& path, Limits limits = {}) : m_limits(limits)
  {
    if (path.empty()) return;
    if (!m_limits.interval || m_limits.interval > 600 || !m_limits.rows || m_limits.rows > 100000 ||
        m_limits.bytes > 32 * 1024 * 1024 || !std::isfinite(m_limits.start_seconds) ||
        !std::isfinite(m_limits.duration_seconds) || m_limits.start_seconds < 0 ||
        m_limits.duration_seconds <= 0 || m_limits.start_seconds > 600 ||
        m_limits.duration_seconds > 600) return;
    const auto header = Columns();
    if (header.size() > m_limits.bytes) return;
#ifdef _WIN32
    int descriptor = -1;
    if (_wsopen_s(&descriptor, path.c_str(), _O_WRONLY | _O_CREAT | _O_EXCL | _O_BINARY,
                  _SH_DENYWR, _S_IREAD | _S_IWRITE) == 0)
    {
      m_file = _fdopen(descriptor, "wb");
      if (!m_file) _close(descriptor);
    }
#else
    const int descriptor = ::open(path.c_str(), O_WRONLY | O_CREAT | O_EXCL, 0600);
    if (descriptor >= 0)
    {
      m_file = ::fdopen(descriptor, "wb");
      if (!m_file) ::close(descriptor);
    }
#endif
    if (m_file && std::fwrite(header.data(), 1, header.size(), m_file) == header.size())
      m_bytes = header.size();
    else Close();
  }
  ~Logger() { Close(); }
  Logger(const Logger&) = delete;
  Logger& operator=(const Logger&) = delete;
  bool IsSampling()
  {
    if (!m_file) return false;
    const double elapsed = Elapsed();
    if (m_file && elapsed >= m_limits.start_seconds + m_limits.duration_seconds) Close();
    return m_file && elapsed >= m_limits.start_seconds && m_frame % m_limits.interval == 0;
  }
  void EndFrame() { ++m_frame; if (m_file && std::fflush(m_file) != 0) Close(); }
  bool Record(const Sample& sample)
  {
    if (!IsSampling()) return false;
    const auto row = Serialize(sample, m_frame, Elapsed() * 1000);
    if (m_rows >= m_limits.rows || row.size() > m_limits.bytes - m_bytes) { Close(); return false; }
    if (std::fwrite(row.data(), 1, row.size(), m_file) != row.size()) { Close(); return false; }
    m_bytes += row.size();
    if (++m_rows == m_limits.rows) Close();
    return true;
  }
private:
  static std::filesystem::path EnvironmentPath()
  {
#ifdef _WIN32
    wchar_t* value = nullptr; std::size_t length = 0;
    if (_wdupenv_s(&value, &length, L"AC_DOLPHIN_VR_ROUTE_LOG") != 0) return {};
    const std::filesystem::path path = value ? value : L"";
    std::free(value); return path;
#else
    const auto* value = std::getenv("AC_DOLPHIN_VR_ROUTE_LOG");
    return value ? std::filesystem::path(value) : std::filesystem::path();
#endif
  }
  static Limits EnvironmentLimits()
  {
    Limits limits;
#ifdef _WIN32
    char* allocated = nullptr; std::size_t length = 0;
    if (_dupenv_s(&allocated, &length, "AC_DOLPHIN_VR_ROUTE_START_SECONDS") != 0)
    { limits.start_seconds = NAN; return limits; }
    const char* value = allocated;
#else
    const char* value = std::getenv("AC_DOLPHIN_VR_ROUTE_START_SECONDS");
#endif
    if (!value) return limits;
    char* end = nullptr;
    const double delay = std::strtod(value, &end);
    // Invalid delay fails closed instead of unexpectedly recording immediately.
    limits.start_seconds = end != value && *end == '\0' && std::isfinite(delay) &&
                           delay >= 0 && delay <= 600 ? delay : NAN;
#ifdef _WIN32
    std::free(allocated);
#endif
    return limits;
  }
  double Elapsed() const
  { return std::chrono::duration<double>(std::chrono::steady_clock::now() - m_start).count(); }
  void Close() { if (m_file) { std::fclose(m_file); m_file = nullptr; } }
  Limits m_limits;
  std::FILE* m_file = nullptr;
  std::chrono::steady_clock::time_point m_start = std::chrono::steady_clock::now();
  std::uint64_t m_frame = 0, m_rows = 0, m_bytes = 0;
};
}  // namespace ACVRRouteTrace
