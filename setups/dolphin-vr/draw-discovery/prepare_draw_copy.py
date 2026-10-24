"""Create an anchored instrumented source copy; never build, launch or edit input."""
from __future__ import annotations

import argparse
import hashlib
import json
from pathlib import Path
import re

HERE = Path(__file__).resolve().parent
MAX_SOURCE_BYTES = 4 * 1024 * 1024


def private_output(path: Path) -> bool:
    # Managed worktrees may themselves sit inside another checkout's .local.
    for parent in path.parents:
        if (parent / ".git").exists():
            return ".local" in path.relative_to(parent).parts
    return ".local" in path.parts


def replace_exact(text: str, before: str, after: str, count: int = 1) -> str:
    actual = text.count(before)
    if actual != count:
        raise ValueError(f"anchor count {actual}, expected {count}: {before[:110]!r}")
    return text.replace(before, after)


def transform(text: str) -> str:
    if "ACDrawDiscovery" in text:
        raise ValueError("source is already instrumented")
    text = replace_exact(text, '#include "Core/ConfigManager.h"\n',
                         '#include "Core/ConfigManager.h"\n#include "ACDrawDiscovery.h"\n')
    anchor = "  const auto used_textures = UsedTextures();\n"
    text = replace_exact(text, anchor, anchor + """  // Private synthetic-only bounded metadata; no new texture loads.
  u32 ac_used_texture_mask = 0;
  for (const u32 stage : used_textures)
    ac_used_texture_mask |= 1u << stage;
  ACDrawDiscovery::BeginDraw(m_ticks_elapsed, system.GetSystemTimers().GetTicksPerSecond(),
                             ac_used_texture_mask);
""")
    anchor = "        const auto cache_entry = g_texture_cache->Load(i);\n"
    text = replace_exact(text, anchor, anchor +
                         "        ACDrawDiscovery::CaptureTexture(i, cache_entry);\n", count=2)
    anchor = "        // Shader Hunter: register shader hashes and check for skip.\n"
    text = replace_exact(text, anchor, """        if (ACDrawDiscovery::Active())
        {
          const auto ac_crc = [](const auto& uid) -> u64 {
            return Common::ComputeCRC32(uid.GetUidDataRaw(),
                                        static_cast<u32>(uid.GetUidDataSize()));
          };
          // These are the regular lookup UIDs used by ShaderHunter before overrides.
          ACDrawDiscovery::CaptureLookup(ac_crc(m_current_pipeline_config.vs_uid),
                                         ac_crc(m_current_pipeline_config.ps_uid),
                                         ac_crc(m_current_pipeline_config.gs_uid));
        }
""" + anchor)
    anchor = "void VertexManagerBase::OnEndFrame()\n{\n"
    text = replace_exact(text, anchor, anchor + "  ACDrawDiscovery::EndFrame();\n")
    anchor = "  // Track the total emulated state draws\n"
    text = replace_exact(text, anchor, """  // Record only the primary submitted draw, after constants and all overrides.
  // No coverage prepass row, GPU completion claim, pixel readback or XR call.
  if (ACDrawDiscovery::Active())
  {
    ACDrawDiscovery::Draw ac_draw;
    ac_draw.order = m_draw_counter + 1;
    ac_draw.indices = m_index_generator.GetIndexLen();
    ac_draw.primitive = static_cast<u32>(primitive_type);
    ac_draw.projection_type = static_cast<u32>(xfmem.projection.type);
    ac_draw.projection = xfmem.projection.rawProjection;
    const auto& ac_vp = xfmem.viewport;
    ac_draw.viewport = {ac_vp.wd, ac_vp.ht, ac_vp.zRange, ac_vp.xOrig, ac_vp.yOrig, ac_vp.farZ};
    ac_draw.scissor_tl = bpmem.scissorTL.hex;
    ac_draw.scissor_br = bpmem.scissorBR.hex;
    ac_draw.scissor_offset = bpmem.scissorOffset.hex;
    ac_draw.zmode = bpmem.zmode.hex;
    ac_draw.blendmode = bpmem.blendmode.hex;
    ac_draw.alpha_test = bpmem.alpha_test.hex;
    ac_draw.genmode = bpmem.genMode.hex;
    const auto ac_crc = [](const auto& uid) -> u64 {
      return Common::ComputeCRC32(uid.GetUidDataRaw(), static_cast<u32>(uid.GetUidDataSize()));
    };
    // Base pipeline UIDs: custom/pink replacement flags below delimit their meaning.
    ac_draw.submitted_uid = m_current_pipeline_is_uber ?
        std::array<u64, 3>{ac_crc(m_current_uber_pipeline_config.vs_uid),
                           ac_crc(m_current_uber_pipeline_config.ps_uid),
                           ac_crc(m_current_uber_pipeline_config.gs_uid)} :
        std::array<u64, 3>{ac_crc(m_current_pipeline_config.vs_uid),
                           ac_crc(m_current_pipeline_config.ps_uid),
                           ac_crc(m_current_pipeline_config.gs_uid)};
    ac_draw.uber = m_current_pipeline_is_uber;
    ac_draw.custom_ps = !custom_pixel_shader_contents.shaders.empty();
    ac_draw.custom_pipeline_selected = current_pipeline != m_current_pipeline_object;
    ac_draw.pink_ps = m_force_pink_ps && static_cast<bool>(m_pink_pixel_shader);
    const auto ac_lookup = ACDrawDiscovery::LookupUID();
    auto& ac_hunter = ShaderHunter::GetInstance();
    ac_draw.cached_families = {
        ac_hunter.GetShaderFamilySignature(ShaderHunter::ShaderType::Vertex, ac_lookup[0]).value_or(0),
        ac_hunter.GetShaderFamilySignature(ShaderHunter::ShaderType::Pixel, ac_lookup[1]).value_or(0),
        ac_hunter.GetShaderFamilySignature(ShaderHunter::ShaderType::Geometry, ac_lookup[2]).value_or(0)};
    for (u32 i = 0; i < 4; ++i)
    {
      ac_draw.gs_stereo[i] = geometry_shader_manager.constants.stereoparams[i];
      ac_draw.gs_screen[i] = geometry_shader_manager.constants.vr_screen[i];
    }
    ACDrawDiscovery::Record(ac_draw);
  }

""" + anchor)
    return text


def prepare(source: Path, out: Path, expected_sha256: str) -> dict:
    source = source.resolve(strict=True)
    out = out.resolve()
    if not private_output(out):
        raise ValueError("output must be in a private .local directory")
    if out.exists():
        raise FileExistsError("preserve existing output; choose a fresh private directory")
    if not re.fullmatch(r"[0-9a-fA-F]{64}", expected_sha256):
        raise ValueError("expected source SHA-256 must contain 64 hex digits")
    if source.stat().st_size > MAX_SOURCE_BYTES:
        raise ValueError("source exceeds 4 MiB bound")
    raw = source.read_bytes()
    if len(raw) > MAX_SOURCE_BYTES:
        raise ValueError("source exceeds 4 MiB bound")
    source_hash = hashlib.sha256(raw).hexdigest()
    if source_hash != expected_sha256.lower():
        raise ValueError("source SHA-256 differs from caller's reviewed input")
    original = raw.decode("utf-8")
    transformed = transform(original.replace("\r\n", "\n"))
    helper_source = HERE / "ACDrawDiscovery.h"
    if not helper_source.is_file():
        raise ValueError("missing helper or unsafe source/destination")
    helper_bytes = helper_source.read_bytes()
    output, helper, receipt = (out / name for name in
                               ("VertexManagerBase.cpp", "ACDrawDiscovery.h", "source-copy.json"))
    # Complete all anchor/hash checks before creating anything. Preserve partial failures.
    out.mkdir(parents=True, exist_ok=False)
    with output.open("xb") as stream:
        stream.write(transformed.encode("utf-8"))
    with helper.open("xb") as stream:
        stream.write(helper_bytes)
    evidence = {
        "schema": "ac-draw-discovery-source-copy-v1",
        "source": str(source), "source_sha256": source_hash,
        "output": str(output), "output_sha256": hashlib.sha256(output.read_bytes()).hexdigest(),
        "helper": str(helper), "helper_sha256": hashlib.sha256(helper.read_bytes()).hexdigest(),
        "helper_source": str(helper_source),
        "preparer_sha256": hashlib.sha256(Path(__file__).read_bytes()).hexdigest(),
        "executed_or_built": False,
        "emulated_time_basis": "cached after_present_event timestamp; not CPU CoreTiming::GetTicks",
        "texture_metadata_phase": "afterLoad_beforeOverrides",
        "draw_metadata_phase": "after primary DrawCurrentBatch; before draw stats",
    }
    with receipt.open("x", encoding="utf-8", newline="\n") as stream:
        json.dump(evidence, stream, indent=2)
        stream.write("\n")
    return evidence


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--source", type=Path, required=True)
    parser.add_argument("--source-sha256", required=True, help="SHA-256 of the caller-reviewed input")
    parser.add_argument("--out", type=Path, required=True, help="fresh private .local output directory")
    args = parser.parse_args()
    evidence = prepare(args.source, args.out, args.source_sha256)
    output = evidence["output"]
    receipt = Path(output).parent / "source-copy.json"
    print(output)
    print(receipt)


if __name__ == "__main__":
    main()
