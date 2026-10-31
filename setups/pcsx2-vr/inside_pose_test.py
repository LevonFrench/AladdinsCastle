#!/usr/bin/env python3
"""CPU-only check of the PCSX2 INSIDE pose bookkeeping and upscale cap.

Extracts pcsx2/GS/PS2VRInside.h from unattended-capture.patch, builds
inside_pose_test.cpp with g++ (skipped when g++ is missing) and runs it. Also pins
the patch wiring: the published pair carries the pose that drew the scanned-out
picture, and no per-draw getenv remains in the INSIDE classifier.
Finishes in seconds; opens no emulator, loader or runtime.
"""
import shutil
import subprocess
import sys
import tempfile
from pathlib import Path

HERE = Path(__file__).resolve().parent
PATCH = HERE / "unattended-capture.patch"


def new_file(text, path):
    start = text.index(f"diff --git a/{path} b/{path}\n")
    nxt = text.find("\ndiff --git ", start + 1)
    block = text[start:nxt if nxt >= 0 else len(text)]
    body = block.split("@@\n", 1)[1]
    return "\n".join(line[1:] for line in body.splitlines() if line.startswith("+")) + "\n"


def file_block(text, path):
    start = text.index(f"diff --git a/{path} b/{path}\n")
    nxt = text.find("\ndiff --git ", start + 1)
    return text[start:nxt if nxt >= 0 else len(text)]


def check_wiring(text):
    live = new_file(text, "pcsx2/GS/PS2StereoLive.h")
    assert "PS2VRInside::TakeDisplayed(0)" in live and "PS2VRInside::Find(left.last,drawn)" in live, "drawn-pose publication"
    assert "GL_LINEAR : GL_NEAREST" in live, "scaled eye blit is filtered"
    ogl = file_block(text, "pcsx2/GS/Renderers/OpenGL/GSDeviceOGL.cpp")
    assert "PS2VRInside::NoteDraw(" in ogl, "draws record their pose"
    added = "\n".join(l for l in ogl.splitlines() if l.startswith("+"))
    assert 'PS2VRInside::Env("' not in added, "INSIDE settings must be read once, not per draw"
    renderer = file_block(text, "pcsx2/GS/Renderers/Common/GSRenderer.cpp")
    assert "PS2VRInside::NoteDisplayed(" in renderer, "scanout records its target"
    qt = file_block(text, "pcsx2-qt/QtHost.cpp")
    assert "upscale>=1 && upscale<=4" in qt, "2x cap reaches the settings"


def main():
    text = PATCH.read_text(encoding="utf-8")
    check_wiring(text)
    print("PASS wiring: drawn-pose publication, per-draw pose record, scanout target, filtered blit, cached settings")
    gxx = shutil.which("g++")
    if not gxx:
        print("SKIP pose logic: g++ not found")
        return 0
    with tempfile.TemporaryDirectory() as tmp:
        tmp = Path(tmp)
        header = tmp / "inc" / "GS" / "PS2VRInside.h"
        header.parent.mkdir(parents=True)
        header.write_text(new_file(text, "pcsx2/GS/PS2VRInside.h"), encoding="utf-8")
        exe = tmp / "inside_pose_test.exe"
        subprocess.run([gxx, "-std=c++17", "-Wall", "-Werror", "-I", str(tmp / "inc"),
                        str(HERE / "inside_pose_test.cpp"), "-o", str(exe), "-pthread"], check=True)
        out = subprocess.run([str(exe)], check=True, capture_output=True, text=True).stdout
        print(out.strip())
    return 0


if __name__ == "__main__":
    sys.exit(main())
