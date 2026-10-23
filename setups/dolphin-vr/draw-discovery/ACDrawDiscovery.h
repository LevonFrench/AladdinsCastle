// SPDX-License-Identifier: GPL-2.0-or-later
// Private offline metadata discovery. No pixel readback, shader changes or XR calls.
#pragma once

#include <array>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "Common/CommonTypes.h"
#include "VideoCommon/TextureCacheBase.h"
#include "VideoCommon/VR/OpenXRManager.h"

namespace ACDrawDiscovery
{
using Clock = std::chrono::steady_clock;

inline bool ProbeEnabled()
{
  const char* value = std::getenv("AC_STEREO_PROBE");
  return value && std::strcmp(value, "1") == 0;
}

struct TextureSnapshot
{
  bool valid = false;
  u64 entry_id = 0, raw_cache_hash = 0, bound_hash = 0;
  u32 native_width = 0, native_height = 0, width = 0, height = 0, layers = 0;
  bool efb = false, xfb = false;
};

struct Draw
{
  u32 order = 0, indices = 0, primitive = 0, projection_type = 0;
  std::array<float, 6> projection{}, viewport{};
  u32 scissor_tl = 0, scissor_br = 0, scissor_offset = 0;
  u32 zmode = 0, blendmode = 0, alpha_test = 0, genmode = 0;
  std::array<u64, 3> submitted_uid{}, cached_families{};
  std::array<float, 4> gs_stereo{}, gs_screen{};
  bool uber = false, custom_ps = false, custom_pipeline_selected = false, pink_ps = false;
};

struct State
{
  bool configured = false, active_draw = false, open_attempted = false, closed = false;
  double start = 0, duration = 0;
  const char* draw_path = nullptr;
  const char* texture_path = nullptr;
  std::FILE* draws = nullptr;
  std::FILE* textures = nullptr;
  Clock::time_point capture_origin{};
  u64 row_count = 0, boundary = 0, last_present_ticks = 0;
  u32 ticks_per_second = 0, used_mask = 0;
  std::array<u64, 3> lookup_uid{};
  std::array<TextureSnapshot, 8> loaded{};

  State()
  {
    const char* begin = std::getenv("AC_DRAW_DISCOVERY_START_S");
    const char* length = std::getenv("AC_DRAW_DISCOVERY_DURATION_S");
    draw_path = std::getenv("AC_DRAW_DISCOVERY_CSV");
    texture_path = std::getenv("AC_DRAW_DISCOVERY_TEXTURE_CSV");
    if (!ProbeEnabled() || !begin || !length || !draw_path || !*draw_path ||
        !texture_path || !*texture_path)
      return;
    char* end_begin = nullptr;
    char* end_length = nullptr;
    start = std::strtod(begin, &end_begin);
    duration = std::strtod(length, &end_length);
    configured = end_begin != begin && end_length != length && *end_begin == '\0' &&
                 *end_length == '\0' && std::isfinite(start) && start >= 0 &&
                 std::isfinite(duration) && duration > 0 && duration <= 5;
  }
  void Close(const char* reason)
  {
    if (draws) std::fclose(draws);
    if (textures) std::fclose(textures);
    draws = textures = nullptr;
    closed = true;
    active_draw = false;
    std::fprintf(stderr, "AC_DRAW_DISCOVERY stop=%s submitted_rows=%llu\n", reason,
                 static_cast<unsigned long long>(row_count));
  }
  ~State()
  {
    if (draws || textures) Close("shutdown");
  }
};

inline State& Get()
{
  static State state;
  return state;
}

inline bool SyntheticOnly()
{
#ifdef ENABLE_VR
  return ProbeEnabled() && !VR::g_openxr;
#else
  return false;
#endif
}

inline void BeginDraw(u64 last_present_ticks, u32 ticks_per_second, u32 used_mask)
{
  State& state = Get();
  state.active_draw = false;
  if (!state.configured || state.closed || !ticks_per_second)
    return;
  state.loaded = {};
  state.lookup_uid = {};
  state.last_present_ticks = last_present_ticks;
  state.ticks_per_second = ticks_per_second;
  state.used_mask = used_mask;
  if (!SyntheticOnly())
  {
    state.Close("synthetic_guard_failed");
    return;
  }
  const double seconds = static_cast<double>(last_present_ticks) / ticks_per_second;
  if (seconds < state.start)
    return;
  if (seconds >= state.start + state.duration)
  {
    if (state.open_attempted) state.Close("emulated_window_end");
    return;
  }
  if (!state.open_attempted)
  {
    state.open_attempted = true;
    state.draws = std::fopen(state.draw_path, "wx");
    state.textures = std::fopen(state.texture_path, "wx");
    if (!state.draws || !state.textures)
    {
      state.Close("exclusive_open_failed");
      return;
    }
    state.capture_origin = Clock::now();
    std::fputs("record_id,xfb_boundary,last_present_ticks,ticks_per_second,last_present_emulated_s,host_elapsed_ns,draw_order,index_count,primitive,projection_type,p0,p1,p2,p3,p4,p5,vp_wd,vp_ht,vp_zrange,vp_xorig,vp_yorig,vp_farz,scissor_tl,scissor_br,scissor_offset,zmode,blendmode,alpha_test,genmode,lookup_vs_crc,lookup_ps_crc,lookup_gs_crc,submitted_base_vs_crc,submitted_base_ps_crc,submitted_base_gs_crc,uber,custom_ps_requested,custom_pipeline_selected,pink_ps_selected,cached_vs_family,cached_ps_family,cached_gs_family,used_texture_mask,gs_stereo0,gs_stereo1,gs_stereo2,gs_stereo3,gs_screen0,gs_screen1,gs_screen2,gs_screen3,role\n", state.draws);
    std::fputs("record_id,stage,snapshot_valid,entry_id,native_width,native_height,width,height,layers,is_efb_copy,is_xfb_copy,raw_cache_hash,bound_hash_at_load,bound_hash_at_submit,hash_changed,metadata_phase\n", state.textures);
    std::fputs("AC_DRAW_DISCOVERY opened synthetic-only metadata; emutime_basis=last_present_event\n", stderr);
  }
  if (state.row_count >= 20000 || Clock::now() - state.capture_origin >= std::chrono::seconds(60))
  {
    state.Close("row_or_host_cap");
    return;
  }
  state.active_draw = true;
}

inline void CaptureTexture(u32 stage, const ::TCacheEntry* entry)
{
  State& state = Get();
  if (!state.active_draw || stage >= state.loaded.size() || !entry || !entry->texture)
    return;
  state.loaded[stage] = {true, entry->id, entry->hash,
                         g_texture_cache->GetBoundTextureHash(stage),
                         entry->native_width, entry->native_height,
                         entry->GetWidth(), entry->GetHeight(), entry->GetNumLayers(),
                         entry->is_efb_copy, entry->is_xfb_copy};
}

inline void CaptureLookup(u64 vs, u64 ps, u64 gs)
{
  State& state = Get();
  if (state.active_draw) state.lookup_uid = {vs, ps, gs};
}

inline bool Active() { return Get().active_draw; }
inline std::array<u64, 3> LookupUID() { return Get().lookup_uid; }

inline void Record(const Draw& draw)
{
  State& state = Get();
  if (!state.active_draw || !state.draws || !state.textures)
    return;
  if (!SyntheticOnly())
  {
    state.Close("synthetic_guard_failed");
    return;
  }
  // A texture load, shader compile or driver wait may outlast admission.
  // Use the checked timestamp for the row so it cannot cross the cap between
  // this check and a second clock read.
  const auto record_time = Clock::now();
  if (state.row_count >= 20000 || record_time - state.capture_origin >= std::chrono::seconds(60))
  {
    state.Close("row_or_host_cap_at_submit");
    return;
  }
  const auto id = static_cast<unsigned long long>(state.row_count++);
  const auto ns = std::chrono::duration_cast<std::chrono::nanoseconds>(record_time - state.capture_origin).count();
  std::fprintf(state.draws, "%llu,%llu,%llu,%u,%.9f,%lld,%u,%u,%u,%u",
               id, static_cast<unsigned long long>(state.boundary),
               static_cast<unsigned long long>(state.last_present_ticks), state.ticks_per_second,
               static_cast<double>(state.last_present_ticks) / state.ticks_per_second,
               static_cast<long long>(ns), draw.order, draw.indices, draw.primitive, draw.projection_type);
  for (float value : draw.projection) std::fprintf(state.draws, ",%.9g", value);
  for (float value : draw.viewport) std::fprintf(state.draws, ",%.9g", value);
  std::fprintf(state.draws, ",%u,%u,%u,%u,%u,%u,%u", draw.scissor_tl, draw.scissor_br,
               draw.scissor_offset, draw.zmode, draw.blendmode, draw.alpha_test, draw.genmode);
  for (u64 value : state.lookup_uid) std::fprintf(state.draws, ",%llu", static_cast<unsigned long long>(value));
  for (u64 value : draw.submitted_uid) std::fprintf(state.draws, ",%llu", static_cast<unsigned long long>(value));
  std::fprintf(state.draws, ",%u,%u,%u,%u", draw.uber ? 1u : 0u, draw.custom_ps ? 1u : 0u,
               draw.custom_pipeline_selected ? 1u : 0u, draw.pink_ps ? 1u : 0u);
  for (u64 value : draw.cached_families) std::fprintf(state.draws, ",%llu", static_cast<unsigned long long>(value));
  std::fprintf(state.draws, ",%u", state.used_mask);
  for (float value : draw.gs_stereo) std::fprintf(state.draws, ",%.9g", value);
  for (float value : draw.gs_screen) std::fprintf(state.draws, ",%.9g", value);
  std::fputs(",unknown\n", state.draws);
  for (u32 stage = 0; stage < state.loaded.size(); ++stage)
  {
    if (!(state.used_mask & (1u << stage))) continue;
    const auto& texture = state.loaded[stage];
    const u64 bound_now = g_texture_cache->GetBoundTextureHash(stage);
    const u32 changed = texture.valid ? (texture.bound_hash != bound_now ? 1u : 0u) : 2u;
    std::fprintf(state.textures, "%llu,%u,%u,%llu,%u,%u,%u,%u,%u,%u,%u,%llu,%llu,%llu,%u,afterLoad_beforeOverrides\n",
                 id, stage, texture.valid ? 1u : 0u, static_cast<unsigned long long>(texture.entry_id),
                 texture.native_width, texture.native_height, texture.width, texture.height, texture.layers,
                 texture.efb ? 1u : 0u, texture.xfb ? 1u : 0u,
                 static_cast<unsigned long long>(texture.raw_cache_hash),
                 static_cast<unsigned long long>(texture.bound_hash),
                 static_cast<unsigned long long>(bound_now), changed);
  }
  if (std::ferror(state.draws) || std::ferror(state.textures)) state.Close("write_error");
}

inline void EndFrame()
{
  State& state = Get();
  if (!state.configured) return;
  ++state.boundary;
  state.active_draw = false;
  if (state.draws) std::fflush(state.draws);
  if (state.textures) std::fflush(state.textures);
  if ((state.draws || state.textures) && Clock::now() - state.capture_origin >= std::chrono::seconds(60))
    state.Close("host_cap");
}
}  // namespace ACDrawDiscovery
