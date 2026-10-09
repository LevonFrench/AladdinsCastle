// SPDX-License-Identifier: GPL-3.0-only
#include "HeaderSafety.h"
#include <array>
namespace ac::scan::detail {
bool resolveChdHunk(const unsigned char *map, quint64 mapBytes,
                    quint32 hunkCount, quint32 entryBytes, bool compressed,
                    quint32 requested, quint32 &terminal,
                    std::atomic_bool &cancel, QString &error) {
  constexpr quint64 mapByteBudget = 32ULL * 1024 * 1024;
  if (!map || !hunkCount || entryBytes != (compressed ? 12U : 4U) ||
      mapBytes > mapByteBudget || quint64(hunkCount) * entryBytes > mapByteBudget ||
      mapBytes < quint64(hunkCount) * entryBytes || requested >= hunkCount) {
    error = "CHD map bounds invalid";
    return false;
  }
  if (!compressed) {
    // The app opens with no parent and rejects any parent SHA in the header.
    // An uncompressed zero slot is zero-filled, not a recursive reference.
    terminal = requested;
    return !cancel;
  }
  std::array<quint32, 32> visited{};
  auto current = requested;
  for (size_t depth = 0; depth < visited.size() && !cancel; ++depth) {
    for (size_t i = 0; i < depth; ++i)
      if (visited[i] == current) {
        error = "CHD self-reference cycle; metadata unverified";
        return false;
      }
    visited[depth] = current;
    const auto *entry = map + quint64(current) * entryBytes;
    if (entry[0] <= 4) {
      // Normalized codec slots 0..3 and COMPRESSION_NONE=4 read/decode
      // directly. Every other indirect form is rejected below.
      terminal = current;
      return true;
    }
    if (entry[0] != 5) {
      error =
          "CHD parent or unsupported indirect reference; metadata unverified";
      return false;
    }
    quint64 target = 0;
    for (int byte = 4; byte < 10; ++byte)
      target = (target << 8) | entry[byte];
    if (target >= hunkCount) {
      error = "CHD self-reference target out of bounds; metadata unverified";
      return false;
    }
    current = static_cast<quint32>(target);
  }
  error = cancel
              ? "CHD metadata scan cancelled"
              : "CHD self-reference exceeds 32 map steps; metadata unverified";
  return false;
}
} // namespace ac::scan::detail
