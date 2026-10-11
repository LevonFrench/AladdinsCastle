#!/usr/bin/env python
"""Add diagnostic metadata to the exact pin + preserved trace + facing source baseline."""
import argparse
import hashlib
from pathlib import Path
import subprocess

PIN = '85c4266b29eb308d173fefa0f21f5cb77df3516a'


def replace(text, old, new):
    if text.count(old) != 1:
        raise SystemExit('Patch anchor missing or ambiguous: ' + old[:100])
    return text.replace(old, new)


def patch_vertex_header(text):
    text = replace(text, '#include "VideoCommon/ACDrawTrace.h"',
                   '#include "VideoCommon/ACDrawTrace.h"\n#include "VideoCommon/ACVRRouteTrace.h"')
    return replace(text, '  ACDrawTrace::Logger m_ac_draw_trace;',
                   '  ACDrawTrace::Logger m_ac_draw_trace;\n'
                   '  ACVRRouteTrace::Logger m_ac_vr_route_trace;\n'
                   '  int m_ac_vr_auto_efb_stage = -1;')


def patch_vertex(text):
    text = replace(text, '#include "VideoCommon/GeometryShaderManager.h"',
                   '#include "VideoCommon/GeometryShaderManager.h"\n#include "VideoCommon/Present.h"')
    text = replace(text, 'void VertexManagerBase::Flush()\n{',
                   'void VertexManagerBase::Flush()\n{\n  m_ac_vr_auto_efb_stage = -1;')
    text = replace(text, '            if (downscaled || blend_active)\n            {\n'
                   '              geometry_shader_manager.vr_stereo_override = 0.0f;',
                   '            if (downscaled || blend_active)\n            {\n'
                   '              m_ac_vr_auto_efb_stage = static_cast<int>(i);\n'
                   '              geometry_shader_manager.vr_stereo_override = 0.0f;')
    text = replace(text, '  m_ac_draw_trace.EndFrame();',
                   '  m_ac_draw_trace.EndFrame();\n  m_ac_vr_route_trace.EndFrame();')
    text = replace(text, '  // Now we can upload uniforms, as nothing else will override them.\n'
                   '  geometry_shader_manager.SetConstants(primitive_type);', '''  std::optional<ACVRRouteTrace::Sample> route_trace;
#ifdef ENABLE_VR
  if (m_ac_vr_route_trace.IsSampling())
  {
    route_trace.emplace();
    route_trace->explicit_override = geometry_shader_manager.vr_stereo_override;
    route_trace->pane_override = geometry_shader_manager.vr_pane_screen_override;
    route_trace->auto_efb_stage = m_ac_vr_auto_efb_stage;
  }
#endif
  // Now we can upload uniforms, as nothing else will override them.
  geometry_shader_manager.SetConstants(primitive_type);''')
    text = replace(text, '  // Log only the primary submitted draw, excluding the coverage prepass.', '''#ifdef ENABLE_VR
  if (route_trace)
  {
    auto& trace = *route_trace;
    trace.draw = m_draw_counter;
    trace.projection_type = static_cast<u32>(xfmem.projection.type);
    trace.active = g_ActiveConfig.stereo_mode == StereoMode::OpenXR && VR::g_openxr &&
                   VR::g_openxr->IsSessionRunning();
    trace.flat = g_ActiveConfig.vr_flat_screen;
    trace.virtual_screen = g_ActiveConfig.vr_virtual_screen;
    trace.panes = g_ActiveConfig.vr_panes_on_screen;
    trace.detect_targets = g_ActiveConfig.vr_detect_render_targets;
    trace.route = trace.active ? geometry_shader_manager.constants.stereoparams[3] : NAN;
    trace.units_per_meter = g_ActiveConfig.vr_units_per_meter;
    const bool trace_vr_tables = trace.active && !trace.flat && std::isfinite(trace.route) &&
                                 trace.route != 0.0f;
    trace.effective_units_per_meter = trace_vr_tables ?
        geometry_shader_manager.GetVRRouteTraceUnitsPerMeter() : NAN;
    trace.display_aspect = g_presenter ? g_presenter->CalculateDrawAspectRatio(false) : NAN;
    trace.projection = xfmem.projection.rawProjection;
    trace.viewport = {xfmem.viewport.wd, xfmem.viewport.ht, xfmem.viewport.zRange,
        xfmem.viewport.xOrig, xfmem.viewport.yOrig, xfmem.viewport.farZ};
    const auto frame = VR::GetVRFrameRegion();
    trace.frame = {frame.valid ? 1 : 0, frame.left, frame.top, frame.width, frame.height};
    trace.viewport_class = static_cast<u32>(VR::ClassifyVRViewport(xfmem.viewport,
        bpmem.scissorOffset.x << 1, bpmem.scissorOffset.y << 1));
    trace.screen = geometry_shader_manager.constants.vr_screen;
    trace.depth = geometry_shader_manager.constants.depth_params;
    if (trace_vr_tables)
    {
      for (int row = 0; row < 4; ++row)
        for (int axis = 0; axis < 4; ++axis)
          trace.eye_rows[row * 4 + axis] = geometry_shader_manager.constants.eye_projection[row][axis];
      for (int row = 0; row < 2; ++row)
        for (int axis = 0; axis < 4; ++axis)
          trace.eye_rows[16 + row * 4 + axis] = geometry_shader_manager.constants.eye_z_row[row][axis];
    }
    else
    {
      trace.screen.fill(NAN); trace.depth.fill(NAN); trace.eye_rows.fill(NAN);
    }
    trace.scissor_tl = bpmem.scissorTL.hex;
    trace.scissor_br = bpmem.scissorBR.hex;
    trace.scissor_offset = bpmem.scissorOffset.hex;
    trace.z_mode = bpmem.zmode.hex;
    trace.blend_mode = bpmem.blendmode.hex;
    trace.gen_mode = bpmem.genMode.hex;
    const auto& regular = m_current_pipeline_config;
    const auto& uber = m_current_uber_pipeline_config;
    trace.vs_uid = m_current_pipeline_is_uber ?
        Common::ComputeCRC32(uber.vs_uid.GetUidDataRaw(), uber.vs_uid.GetUidDataSize()) :
        Common::ComputeCRC32(regular.vs_uid.GetUidDataRaw(), regular.vs_uid.GetUidDataSize());
    trace.ps_uid = m_current_pipeline_is_uber ?
        Common::ComputeCRC32(uber.ps_uid.GetUidDataRaw(), uber.ps_uid.GetUidDataSize()) :
        Common::ComputeCRC32(regular.ps_uid.GetUidDataRaw(), regular.ps_uid.GetUidDataSize());
    for (const u32 stage : UsedTextures())
    {
      if (stage < trace.textures.size())
      {
        trace.used_texture_mask |= 1u << stage;
        const auto t = g_texture_cache->GetBoundVRRouteTrace(stage);
        trace.textures[stage] = {t[0],t[1],t[2],t[3],t[4],t[5],t[6]};
      }
    }
    m_ac_vr_route_trace.Record(trace);
  }
#endif

  // Log only the primary submitted draw, excluding the coverage prepass.''')
    return text


def patch_texture_header(text):
    return replace(text, '  bool IsBoundEfbCopy(u32 stage, u32* native_width, u32* native_height) const;',
                   '  bool IsBoundEfbCopy(u32 stage, u32* native_width, u32* native_height) const;\n'
                   '  // Diagnostic only: bound, EFB, XFB, native width/height, layers, stereo EFB reference.\n'
                   '  // Unavailable fields are -1; no texture content, names or emulated addresses.\n'
                   '  std::array<int, 7> GetBoundVRRouteTrace(u32 stage) const;')


def patch_texture(text):
    return replace(text, 'bool TextureCacheBase::ApplyVRPreserveStereoEFBFix(u32 stage)', '''std::array<int, 7> TextureCacheBase::GetBoundVRRouteTrace(u32 stage) const
{
  if (stage >= m_bound_textures.size() || !m_bound_textures[stage])
    return {0,-1,-1,-1,-1,-1,-1};
  const auto& entry = m_bound_textures[stage];
  int stereo_reference = 0;
  for (const auto* source : entry->references)
    if (source && source->is_efb_copy && source->texture && source->GetNumLayers() >= 2)
      stereo_reference = 1;
  return {1,entry->is_efb_copy ? 1 : 0,entry->is_xfb_copy ? 1 : 0,
          static_cast<int>(entry->native_width),static_cast<int>(entry->native_height),
          entry->texture ? static_cast<int>(entry->GetNumLayers()) : -1,stereo_reference};
}

bool TextureCacheBase::ApplyVRPreserveStereoEFBFix(u32 stage)''')


def patch_geometry_header(text):
    return replace(text, '  void SetConstants(PrimitiveType prim);',
                   '  void SetConstants(PrimitiveType prim);\n'
                   '  float GetVRRouteTraceUnitsPerMeter() const { return m_cached_units_per_meter; }')


EDITS = {
    'VideoCommon/VertexManagerBase.cpp': patch_vertex,
    'VideoCommon/VertexManagerBase.h': patch_vertex_header,
    'VideoCommon/TextureCacheBase.cpp': patch_texture,
    'VideoCommon/TextureCacheBase.h': patch_texture_header,
    'VideoCommon/GeometryShaderManager.h': patch_geometry_header,
}


def main():
    import json
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('source', type=Path)
    args = parser.parse_args()
    source = args.source.resolve(strict=True)
    if '.local' not in source.parts:
        raise SystemExit('Owned private source under .local required')
    if subprocess.check_output(['git','-C',str(source),'rev-parse','HEAD'],text=True).strip()!=PIN:
        raise SystemExit('Wrong pin')
    baseline = json.loads(Path(__file__).with_name('baseline-sha256.json').read_text(encoding='utf-8'))
    changes = {}
    for name, edit in EDITS.items():
        path = source/'Source/Core'/name
        current = path.read_text(encoding='utf-8')
        # Reconstruct the expected base from our reviewed patch context only when exact base matches.
        if hashlib.sha256(current.encode()).hexdigest() != baseline[name]:
            raise SystemExit('Baseline differs; refuse edits: '+name)
        changes[path] = edit(current).encode('utf-8')
    header = Path(__file__).with_name('ACVRRouteTrace.h').read_bytes()
    target = source/'Source/Core/VideoCommon/ACVRRouteTrace.h'
    if target.exists():
        raise SystemExit('Diagnostic helper already exists; preserve it')
    changes[target] = header
    for path, data in changes.items():
        path.write_bytes(data)
        print(path.name+' SHA256 '+hashlib.sha256(data).hexdigest())


if __name__ == '__main__':
    main()
