// Copyright 2026 AladdinsCastle contributors
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <chrono>
#include <cstdint>
#include <ostream>
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#define AC_TRACE_CLOCK_RESTORE_NOMINMAX
#endif
#include <windows.h>
#ifdef AC_TRACE_CLOCK_RESTORE_NOMINMAX
#undef NOMINMAX
#undef AC_TRACE_CLOCK_RESTORE_NOMINMAX
#endif
#endif

namespace ACTraceClock
{
// UTC and steady_anchor are acquired inside the QPC bracket. This is a host
// clock correlation, never a fence or an association with an emulated GPU draw.
struct Anchor
{
  bool available = false;
  std::int64_t frequency = 0, before = 0, after = 0;
  std::uint64_t utc_filetime_100ns = 0;
  std::chrono::steady_clock::time_point steady_anchor{};
};
inline Anchor Now()
{
  Anchor value;
#ifdef _WIN32
  LARGE_INTEGER frequency{}, before{}, after{};
  FILETIME utc{};
  if (!QueryPerformanceFrequency(&frequency) || frequency.QuadPart <= 0 ||
      !QueryPerformanceCounter(&before)) return value;
  GetSystemTimePreciseAsFileTime(&utc);
  value.steady_anchor = std::chrono::steady_clock::now();
  if (!QueryPerformanceCounter(&after) || after.QuadPart < before.QuadPart) return value;
  value.frequency = frequency.QuadPart;
  value.before = before.QuadPart; value.after = after.QuadPart;
  value.utc_filetime_100ns = (static_cast<std::uint64_t>(utc.dwHighDateTime) << 32) |
                            utc.dwLowDateTime;
  value.available = true;
#endif
  return value;
}
inline void Fields(std::ostream& out, const Anchor& value)
{
  out << "\"clock_available\":" << (value.available ? "true" : "false")
      << ",\"monotonic_clock\":\"windows-qpc\",\"qpc_unit\":\"ticks\""
         ",\"utc_clock\":\"windows-filetime-utc\",\"utc_unit\":\"100ns-since-1601\"";
  if (value.available)
    out << ",\"qpc_frequency_hz\":" << value.frequency << ",\"qpc_before\":" << value.before
        << ",\"qpc_after\":" << value.after << ",\"utc_filetime_100ns\":" << value.utc_filetime_100ns;
  else
    out << ",\"qpc_frequency_hz\":null,\"qpc_before\":null,\"qpc_after\":null,\"utc_filetime_100ns\":null";
}
}  // namespace ACTraceClock
