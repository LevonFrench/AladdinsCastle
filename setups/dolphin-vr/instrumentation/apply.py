#!/usr/bin/env python
"""Apply original, bounded metadata instrumentation to an owned pinned clone."""
import argparse
import hashlib
from pathlib import Path
import subprocess

PIN = "85c4266b29eb308d173fefa0f21f5cb77df3516a"


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("source", type=Path)
    args = parser.parse_args()
    source = args.source.resolve(strict=True)
    if ".local" not in source.parts:
        raise SystemExit("Use a private source clone under .local")
    head = subprocess.check_output(["git", "-C", str(source), "rev-parse", "HEAD"], text=True).strip()
    if head != PIN:
        raise SystemExit("Wrong source pin")
    common = source / "Source/Core/VideoCommon"
    edits = {
        "VertexManagerBase.h": [
            ('#pragma once\n', '#pragma once\n\n#include "VideoCommon/ACDrawTrace.h"\n'),
            ('  u32 m_draw_counter = 0;\n', '  u32 m_draw_counter = 0;\n  ACDrawTrace::Logger m_ac_draw_trace;\n'),
        ],
        "VertexManagerBase.cpp": [
            ('void VertexManagerBase::OnEndFrame()\n{\n',
             'void VertexManagerBase::OnEndFrame()\n{\n  m_ac_draw_trace.EndFrame();\n'),
            ('  // Track the total emulated state draws\n',
             '  // Log only the primary submitted draw, excluding the coverage prepass.\n'
             '  if (m_ac_draw_trace.IsSampling())\n'
             '  {\n'
             '    const auto& regular = m_current_pipeline_config;\n'
             '    const auto& uber = m_current_uber_pipeline_config;\n'
             '    const u64 vs_hash = m_current_pipeline_is_uber ?\n'
             '        Common::ComputeCRC32(uber.vs_uid.GetUidDataRaw(), uber.vs_uid.GetUidDataSize()) :\n'
             '        Common::ComputeCRC32(regular.vs_uid.GetUidDataRaw(), regular.vs_uid.GetUidDataSize());\n'
             '    const u64 ps_hash = m_current_pipeline_is_uber ?\n'
             '        Common::ComputeCRC32(uber.ps_uid.GetUidDataRaw(), uber.ps_uid.GetUidDataSize()) :\n'
             '        Common::ComputeCRC32(regular.ps_uid.GetUidDataRaw(), regular.ps_uid.GetUidDataSize());\n'
             '    m_ac_draw_trace.Record({m_draw_counter, static_cast<u32>(primitive_type),\n'
             '      m_index_generator.GetIndexLen(), static_cast<u32>(xfmem.projection.type),\n'
             '      xfmem.projection.rawProjection,\n'
             '      {xfmem.viewport.wd, xfmem.viewport.ht, xfmem.viewport.zRange,\n'
             '       xfmem.viewport.xOrig, xfmem.viewport.yOrig, xfmem.viewport.farZ},\n'
             '      bpmem.scissorTL.hex, bpmem.scissorBR.hex, bpmem.scissorOffset.hex,\n'
             '      bpmem.zmode.hex, bpmem.blendmode.hex, bpmem.genMode.hex, vs_hash, ps_hash});\n'
             '  }\n\n'
             '  // Track the total emulated state draws\n'),
        ],
    }
    outputs = {}
    for name, substitutions in edits.items():
        path = common / name
        original = subprocess.check_output(["git", "-C", str(source), "show",
                                            f"{PIN}:Source/Core/VideoCommon/{name}"])
        desired = original.decode("utf-8").replace("\r\n", "\n")
        for old, new in substitutions:
            if desired.count(old) != 1:
                raise SystemExit(f"Ambiguous patch anchor: {name}")
            desired = desired.replace(old, new)
        current = path.read_text(encoding="utf-8")
        if current not in (original.decode("utf-8").replace("\r\n", "\n"), desired):
            raise SystemExit(f"Refusing to overwrite existing source edits: {name}")
        outputs[path] = desired.encode("utf-8")
    logger = Path(__file__).with_name("ACDrawTrace.h").read_bytes()
    destination = common / "ACDrawTrace.h"
    if destination.exists() and destination.read_bytes() != logger:
        raise SystemExit("Existing logger differs; inspect before replacing")
    outputs[destination] = logger
    # Preflight all inputs before any write; repeated application is idempotent.
    for path, data in outputs.items():
        if not path.exists() or path.read_bytes() != data:
            path.write_bytes(data)
        print(f"{path.name} SHA256 {hashlib.sha256(data).hexdigest()}")


if __name__ == "__main__":
    main()
