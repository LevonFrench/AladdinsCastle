// Copyright 2026 AladdinsCastle contributors
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <cctype>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

// Offline pixel-shader trace and skip list. Off unless an environment variable asks for it.
//
// The hash is the one DolphinXR's Shader Hunter shows and GameSettingsVR [ShaderOverride] uses
// for Type=PS: the CRC32 of the pipeline's pixel-shader UID data. Compute it exactly like the
// hunter does (Common::ComputeCRC32 over ps_uid.GetUidDataRaw()) and pass it in.
//
//   AC_PS_TRACE        CSV path, opened for replacement. One row per traced draw.
//   AC_PS_TRACE_FIRST  first emulator frame to trace (default 0)
//   AC_PS_TRACE_LAST   last emulator frame to trace (default 100000)
//   AC_PS_TRACE_ALL    "1" also traces perspective draws (default: orthographic, i.e. 2D, only)
//   AC_PS_SKIP         comma/space separated hex pixel-shader hashes whose draws are skipped
//
// Frames count OnEndFrame calls since the process started (a restored savestate starts at 0).
// No vertices, textures or pixels are read. At most 200000 rows are written.
namespace ACPsTrace
{
constexpr std::uint32_t kMaxRows = 200000;
constexpr std::size_t kMaxSkip = 64;

// "a8d2ed4e, 0x677241d9" -> {0xa8d2ed4e, 0x677241d9}. Unparseable or oversized tokens are
// dropped and counted in *rejected. At most kMaxSkip values are kept.
inline std::vector<std::uint32_t> ParseHashList(const char* text, int* rejected = nullptr)
{
  std::vector<std::uint32_t> out;
  int bad = 0;
  std::string token;
  auto flush = [&] {
    if (token.empty())
      return;
    std::string digits = token;
    if (digits.size() > 2 && digits[0] == '0' && (digits[1] == 'x' || digits[1] == 'X'))
      digits.erase(0, 2);
    bool ok = !digits.empty() && digits.size() <= 16;
    for (char c : digits)
      ok = ok && std::isxdigit(static_cast<unsigned char>(c));
    if (ok)
    {
      const unsigned long long value = std::strtoull(digits.c_str(), nullptr, 16);
      if (value <= 0xffffffffULL && out.size() < kMaxSkip)
        out.push_back(static_cast<std::uint32_t>(value));
      else
        ++bad;
    }
    else
    {
      ++bad;
    }
    token.clear();
  };
  for (const char* p = text ? text : ""; *p; ++p)
  {
    if (*p == ',' || *p == ';' || *p == ' ' || *p == '\t')
      flush();
    else
      token.push_back(*p);
  }
  flush();
  if (rejected)
    *rejected = bad;
  return out;
}

struct Sample
{
  std::uint32_t projection_type;  // 0 perspective, 1 orthographic
  std::uint32_t primitive, indices;
  std::uint32_t ps_hash, vs_hash;
  float viewport_x, viewport_y, viewport_w, viewport_h;
  std::uint32_t blend_mode, z_mode, gen_mode;
};

class Tracer
{
public:
  Tracer()
  {
    const char* skip = std::getenv("AC_PS_SKIP");
    if (skip && *skip)
    {
      int rejected = 0;
      m_skip = ParseHashList(skip, &rejected);
      if (rejected)
        std::fprintf(stderr, "AC ps trace: %d AC_PS_SKIP entries ignored\n", rejected);
    }
    const char* first = std::getenv("AC_PS_TRACE_FIRST");
    const char* last = std::getenv("AC_PS_TRACE_LAST");
    if (first)
      m_first = static_cast<std::uint32_t>(std::strtoul(first, nullptr, 10));
    if (last)
      m_last = static_cast<std::uint32_t>(std::strtoul(last, nullptr, 10));
    const char* all = std::getenv("AC_PS_TRACE_ALL");
    m_all = all && std::strcmp(all, "1") == 0;
    const char* path = std::getenv("AC_PS_TRACE");
    if (path && *path)
    {
      m_file.open(std::filesystem::path(path), std::ios::binary | std::ios::trunc);
      if (m_file)
        m_file << "frame,draw,projection,primitive,indices,ps_hash,vs_hash,viewport_x,viewport_y,"
                  "viewport_w,viewport_h,blend,zmode,gen_mode\n";
      else
        std::fputs("AC ps trace: could not open AC_PS_TRACE\n", stderr);
    }
  }
  ~Tracer()
  {
    if (m_file)
      m_file.flush();
    if (!m_skip.empty() || m_file.is_open())
      std::fprintf(stderr, "AC ps trace: %u rows, %llu draws skipped\n", m_rows,
                   static_cast<unsigned long long>(m_skipped));
  }

  bool Active() const { return m_file.is_open() || !m_skip.empty(); }

  bool ShouldSkip(std::uint32_t ps_hash)
  {
    for (const std::uint32_t h : m_skip)
    {
      if (h == ps_hash)
      {
        ++m_skipped;
        return true;
      }
    }
    return false;
  }

  void Record(const Sample& s)
  {
    ++m_draw;
    if (!m_file || m_rows >= kMaxRows || m_frame < m_first || m_frame > m_last)
      return;
    if (!m_all && s.projection_type != 1)
      return;
    char line[256];
    const int n = std::snprintf(
        line, sizeof line, "%u,%u,%u,%u,%u,%08x,%08x,%.1f,%.1f,%.1f,%.1f,%08x,%08x,%08x\n", m_frame,
        m_draw, s.projection_type, s.primitive, s.indices, s.ps_hash, s.vs_hash, s.viewport_x,
        s.viewport_y, s.viewport_w, s.viewport_h, s.blend_mode, s.z_mode, s.gen_mode);
    if (n > 0 && n < static_cast<int>(sizeof line))
    {
      m_file.write(line, n);
      ++m_rows;
    }
  }

  void EndFrame()
  {
    ++m_frame;
    m_draw = 0;
    if (m_file && (m_frame & 15) == 0)
      m_file.flush();
  }

private:
  std::vector<std::uint32_t> m_skip;
  std::ofstream m_file;
  std::uint32_t m_first = 0, m_last = 100000, m_frame = 0, m_draw = 0, m_rows = 0;
  std::uint64_t m_skipped = 0;
  bool m_all = false;
};

inline Tracer& Get()
{
  static Tracer tracer;
  return tracer;
}
}  // namespace ACPsTrace
