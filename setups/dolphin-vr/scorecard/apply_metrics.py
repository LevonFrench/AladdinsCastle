"""Patch a private COPY of Present.cpp with offline XFB metadata tracing.

Usage: python -B apply_metrics.py /absolute/private/copied/Present.cpp
Never pass the shared DolphinXR source file. No OpenXR calls are added.
"""
import argparse
from pathlib import Path


HELPER = r'''
// AladdinsCastle offline XFB metadata trace; synthetic probe only, no runtime calls.
namespace
{
void ACStereoMetricsRecord(u64 ticks, bool duplicate, const char* reason, u64 texture_id,
                           u32 layers, u32 width, u32 height, u32 vi_width, u32 vi_height)
{
  using MetricsClock = std::chrono::steady_clock;
  struct Trace
  {
    std::FILE* file = nullptr;
    MetricsClock::time_point origin = MetricsClock::now();
    u64 rows = 0;
    Trace()
    {
      const char* path = std::getenv("AC_STEREO_METRICS_CSV");
      if (std::getenv("AC_STEREO_PROBE") && path && *path)
      {
        // Exclusive creation: a previous receipt must never be overwritten.
        file = std::fopen(path, "wx");
        if (file)
          std::fputs("wall_ns,emulated_ticks,ticks_per_second,duplicate,reason,texture_id,layers,width,height,vi_width,vi_height,native_vi_hz,forced_vbi_frequency,effective_vbi_target_hz,vi_overclock_enabled,vi_overclock\n", file);
      }
    }
    ~Trace() { if (file) std::fclose(file); }
  };
  static Trace trace;
  if (!trace.file)
    return;
  const auto elapsed = MetricsClock::now() - trace.origin;
  if (elapsed >= std::chrono::seconds(1800) || trace.rows >= 250000)
  {
    std::fclose(trace.file);
    trace.file = nullptr;
    return;
  }
  const auto ns = std::chrono::duration_cast<std::chrono::nanoseconds>(elapsed).count();
  const u32 hz = Core::System::GetInstance().GetSystemTimers().GetTicksPerSecond();
  const double native_vi_hz = Core::System::GetInstance().GetVideoInterface().GetNominalRefreshRate();
  const int forced_vbi_frequency = Config::Get(Config::GFX_VR_FORCED_VBI_FREQUENCY);
  const double effective_vbi_target_hz = VideoInterface::GetVRForcedVBITargetHz();
  const bool vi_overclock_enabled = Config::Get(Config::MAIN_VI_OVERCLOCK_ENABLE);
  const double vi_overclock = Config::Get(Config::MAIN_VI_OVERCLOCK);
  std::fprintf(trace.file, "%lld,%llu,%u,%u,%s,%llu,%u,%u,%u,%u,%u,%.9f,%d,%.9f,%u,%.9f\n",
               static_cast<long long>(ns), static_cast<unsigned long long>(ticks), hz,
               duplicate ? 1u : 0u, reason, static_cast<unsigned long long>(texture_id),
               layers, width, height, vi_width, vi_height, native_vi_hz, forced_vbi_frequency,
               effective_vbi_target_hz, vi_overclock_enabled ? 1u : 0u, vi_overclock);
  // Bounded metadata only. Flush each row so an owned timed stop preserves the tail.
  std::fflush(trace.file);
  ++trace.rows;
}
}  // namespace

'''


def patch(text):
    if "void ACStereoMetricsRecord(" in text:
        raise ValueError("already patched")
    edits = [
        ('#include "VideoCommon/Present.h"\n', '#include "VideoCommon/Present.h"\n\n#include <chrono>\n#include <cstdio>\n#include <cstdlib>\n#include "Core/HW/SystemTimers.h"\n'),
        ('namespace VideoCommon\n{\n', 'namespace VideoCommon\n{\n' + HELPER),
        ('    present_info.reason = PresentInfo::PresentReason::VideoInterface;\n  }\n\n',
         '    present_info.reason = PresentInfo::PresentReason::VideoInterface;\n  }\n\n' + call('is_duplicate', 'vi')),
        ('  FetchXFB(xfb_addr, fb_width, fb_stride, fb_height, ticks);\n',
         '  const bool ac_metrics_duplicate = FetchXFB(xfb_addr, fb_width, fb_stride, fb_height, ticks);\n'),
        ('      .intended_present_time = m_next_swap_estimated_time,\n  };\n\n',
         '      .intended_present_time = m_next_swap_estimated_time,\n  };\n\n' + call('ac_metrics_duplicate', 'immediate')),
    ]
    for old, new in edits:
        count = text.count(old)
        if count != 1:
            raise ValueError(f"anchor expected once, found {count}: {old[:80]!r}")
        text = text.replace(old, new)
    return text


def call(duplicate, reason):
    return f'''  ACStereoMetricsRecord(ticks, {duplicate}, "{reason}", m_last_xfb_id,
                        m_xfb_entry ? m_xfb_entry->texture->GetLayers() : 0,
                        m_xfb_entry ? m_xfb_entry->texture->GetConfig().width : 0,
                        m_xfb_entry ? m_xfb_entry->texture->GetConfig().height : 0,
                        fb_width, fb_height);

'''


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("copy", type=Path)
    args = parser.parse_args()
    target = args.copy.resolve()
    if target.name != "Present.cpp" or "metrics-src" not in target.parts:
        parser.error("target must be a Present.cpp COPY inside a private metrics-src folder")
    raw = target.read_bytes()
    text = raw.decode("utf-8").replace("\r\n", "\n")
    output = patch(text)
    if b"\r\n" in raw:
        output = output.replace("\n", "\r\n")
    target.write_bytes(output.encode("utf-8"))
    print(f"Patched metadata-only offline trace: {target}")


if __name__ == "__main__":
    main()
