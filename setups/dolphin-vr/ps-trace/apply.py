#!/usr/bin/env python
"""Add the pixel-shader trace / skip list (ACPsTrace.h) to an owned DolphinXR source tree.

    python -B apply.py <source>                    # a clone at the pinned commit, under .local
    python -B apply.py <source> --any-revision     # a private copy or a different DolphinXR revision

<source> is a folder that contains Source/Core/VideoCommon/VertexManagerBase.cpp. The tool edits
that file in place at three exact anchors and writes ACPsTrace.h beside it. It refuses a tree
outside .local, a pinned clone at another commit, an anchor that is missing or not unique, and a
file that is already half patched. Running it again on a patched file changes nothing. Line
endings (LF or CRLF) are kept. It does not build or run anything.
"""
import argparse
import hashlib
from pathlib import Path
import subprocess
import sys

PIN = "85c4266b29eb308d173fefa0f21f5cb77df3516a"
MARK = "AladdinsCastle ps-trace"

INCLUDE_ANCHOR = '#include "VideoCommon/VertexShaderManager.h"\n'
FRAME_ANCHOR = 'void VertexManagerBase::OnEndFrame()\n{\n'
DRAW_ANCHOR = '        bool hunter_skip = false;\n'

INCLUDE_NEW = INCLUDE_ANCHOR + '#include "VideoCommon/ACPsTrace.h"\n'
FRAME_NEW = FRAME_ANCHOR + '  ACPsTrace::Get().EndFrame();\n'
DRAW_NEW = (
    '        // ' + MARK + ': pixel-shader hash trace and skip list (off unless AC_PS_TRACE / AC_PS_SKIP).\n'
    '        bool ac_ps_skip = false;\n'
    '        if (ACPsTrace::Get().Active())\n'
    '        {\n'
    '          const auto& ac_vs = m_current_pipeline_config.vs_uid;\n'
    '          const auto& ac_ps = m_current_pipeline_config.ps_uid;\n'
    '          const u32 ac_vs_hash =\n'
    '              Common::ComputeCRC32(ac_vs.GetUidDataRaw(), static_cast<u32>(ac_vs.GetUidDataSize()));\n'
    '          const u32 ac_ps_hash =\n'
    '              Common::ComputeCRC32(ac_ps.GetUidDataRaw(), static_cast<u32>(ac_ps.GetUidDataSize()));\n'
    '          ACPsTrace::Get().Record({static_cast<u32>(xfmem.projection.type),\n'
    '                                   static_cast<u32>(m_current_primitive_type), num_indices,\n'
    '                                   ac_ps_hash, ac_vs_hash, xfmem.viewport.xOrig,\n'
    '                                   xfmem.viewport.yOrig, 2.0f * std::fabs(xfmem.viewport.wd),\n'
    '                                   2.0f * std::fabs(xfmem.viewport.ht), bpmem.blendmode.hex,\n'
    '                                   bpmem.zmode.hex, bpmem.genMode.hex});\n'
    '          ac_ps_skip = ACPsTrace::Get().ShouldSkip(ac_ps_hash);\n'
    '        }\n'
    '        bool hunter_skip = ac_ps_skip;\n')

EDITS = [(INCLUDE_ANCHOR, INCLUDE_NEW), (FRAME_ANCHOR, FRAME_NEW), (DRAW_ANCHOR, DRAW_NEW)]


def patch_text(text):
    """Return the patched text (LF endings). Raises ValueError on a bad or half-patched file."""
    if MARK in text:
        if any(new not in text for _, new in EDITS):
            raise ValueError("file is half patched; start from an unpatched copy")
        return text
    for old, _ in EDITS:
        if text.count(old) != 1:
            raise ValueError("anchor not found exactly once: " + old.strip().splitlines()[0])
    for old, new in EDITS:
        text = text.replace(old, new)
    return text


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("source", type=Path)
    parser.add_argument("--any-revision", action="store_true",
                        help="skip the pinned-commit check (private copies, other revisions)")
    args = parser.parse_args(argv)
    source = args.source.resolve(strict=True)
    if ".local" not in source.parts:
        raise SystemExit("Use a private source tree under .local")
    if not args.any_revision:
        head = subprocess.check_output(["git", "-C", str(source), "rev-parse", "HEAD"], text=True).strip()
        if head != PIN:
            raise SystemExit("Wrong source pin (use --any-revision for a private copy)")
    common = source / "Source/Core/VideoCommon"
    target = common / "VertexManagerBase.cpp"
    raw = target.read_bytes()
    crlf = b"\r\n" in raw
    text = raw.decode("utf-8").replace("\r\n", "\n")
    try:
        patched = patch_text(text)
    except ValueError as error:
        raise SystemExit(f"Refusing: {error}")
    data = patched.replace("\n", "\r\n").encode("utf-8") if crlf else patched.encode("utf-8")
    header = Path(__file__).with_name("ACPsTrace.h").read_bytes()
    destination = common / "ACPsTrace.h"
    if destination.exists() and destination.read_bytes() != header:
        raise SystemExit("Existing ACPsTrace.h differs; inspect before replacing")
    if data != raw:
        target.write_bytes(data)
    if not destination.exists():
        destination.write_bytes(header)
    print(f"{target.name} SHA256 {hashlib.sha256(data).hexdigest()}")
    print(f"{destination.name} SHA256 {hashlib.sha256(header).hexdigest()}")


if __name__ == "__main__":
    sys.exit(main())
