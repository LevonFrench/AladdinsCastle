// SPDX-License-Identifier: GPL-3.0-or-later
// Minimal PNG writer (stored deflate blocks) for offline proof pictures.
#pragma once
#include <cstdint>
#include <cstdio>
#include <vector>

namespace acvr_png {
inline std::uint32_t crc(const unsigned char *p, std::size_t n, std::uint32_t c = 0xffffffffu)
{
  for (std::size_t i = 0; i < n; ++i) {
    c ^= p[i];
    for (int k = 0; k < 8; ++k) c = (c >> 1) ^ (0xedb88320u & (0u - (c & 1u)));
  }
  return c;
}

// pixels: 0x00RRGGBB, top row first.
inline bool write(const char *path, const std::uint32_t *pixels, unsigned w, unsigned h)
{
  std::vector<unsigned char> raw;
  raw.reserve((std::size_t)(w * 3 + 1) * h);
  for (unsigned y = 0; y < h; ++y) {
    raw.push_back(0);
    for (unsigned x = 0; x < w; ++x) {
      const std::uint32_t p = pixels[(std::size_t)y * w + x];
      raw.push_back((p >> 16) & 255); raw.push_back((p >> 8) & 255); raw.push_back(p & 255);
    }
  }
  std::vector<unsigned char> z = { 0x78, 0x01 };
  std::uint32_t a = 1, b = 0;
  for (unsigned char c : raw) { a = (a + c) % 65521; b = (b + a) % 65521; }
  for (std::size_t at = 0; at < raw.size() || at == 0;) {
    const std::size_t n = raw.size() - at > 65535 ? 65535 : raw.size() - at;
    const bool last = at + n >= raw.size();
    z.push_back(last ? 1 : 0);
    z.push_back(n & 255); z.push_back(n >> 8); z.push_back(~n & 255); z.push_back((~n >> 8) & 255);
    z.insert(z.end(), raw.begin() + at, raw.begin() + at + n);
    at += n;
    if (last) break;
  }
  const std::uint32_t adler = (b << 16) | a;
  for (int s = 24; s >= 0; s -= 8) z.push_back((adler >> s) & 255);
  FILE *f = std::fopen(path, "wb");
  if (!f) return false;
  auto be = [](std::vector<unsigned char> &v, std::uint32_t x) { for (int s = 24; s >= 0; s -= 8) v.push_back((x >> s) & 255); };
  auto chunk = [&](const char *type, const std::vector<unsigned char> &data) {
    std::vector<unsigned char> out; be(out, (std::uint32_t)data.size());
    std::vector<unsigned char> body(type, type + 4); body.insert(body.end(), data.begin(), data.end());
    out.insert(out.end(), body.begin(), body.end());
    be(out, crc(body.data(), body.size()) ^ 0xffffffffu);
    std::fwrite(out.data(), 1, out.size(), f);
  };
  static const unsigned char sig[] = { 137, 80, 78, 71, 13, 10, 26, 10 };
  std::fwrite(sig, 1, 8, f);
  std::vector<unsigned char> ihdr; be(ihdr, w); be(ihdr, h);
  ihdr.insert(ihdr.end(), { 8, 2, 0, 0, 0 });
  chunk("IHDR", ihdr); chunk("IDAT", z); chunk("IEND", {});
  return std::fclose(f) == 0;
}
}
