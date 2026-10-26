#!/usr/bin/env python3
"""CPU-only check of the PCSX2 VR universal-controls chord resolver.

Extracts pcsx2/Input/PS2VRStickChord.h from unattended-capture.patch, builds
stick_chord_test.cpp with g++ (skipped when g++ is missing) and runs it. Also pins
the patch's bindings: no menu or system button, Start/Select on the stick clicks.
Finishes in seconds; opens no emulator, loader or runtime.
"""
import re
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


def check_bindings(text):
    controllers = new_file(text, "pcsx2/GS/PS2VRControllers.h")
    assert "input/menu/click" not in controllers, "left menu button must stay unbound"
    assert "spare start" not in controllers.lower() and "spare_start" not in controllers.lower(), "no spare Start alias"
    assert "/system/" not in controllers and "/input/menu" not in controllers
    assert '"/user/hand/left/input/thumbstick/click")' in controllers  # U1
    assert '"/user/hand/right/input/thumbstick/click")' in controllers  # U2
    for path in ("left/input/y/click", "left/input/x/click", "left/input/b/click", "left/input/a/click"):
        assert path in controllers, path  # L3 / R3 exception, Touch and Index
    assert "struct QuitHold" not in new_file(text, "pcsx2/Input/PS2VRGun.h"), "old Y quit timer"
    assert "quit_y" not in controllers


def main():
    text = PATCH.read_text(encoding="utf-8")
    check_bindings(text)
    print("PASS bindings: stick clicks only, no menu/system button, L3/R3 on left Y/X")
    gxx = shutil.which("g++")
    if not gxx:
        print("SKIP chord logic: g++ not found")
        return 0
    with tempfile.TemporaryDirectory() as tmp:
        tmp = Path(tmp)
        header = tmp / "inc" / "Input" / "PS2VRStickChord.h"
        header.parent.mkdir(parents=True)
        header.write_text(new_file(text, "pcsx2/Input/PS2VRStickChord.h"), encoding="utf-8")
        exe = tmp / "stick_chord_test.exe"
        subprocess.run([gxx, "-std=c++17", "-Wall", "-Werror", "-I", str(tmp / "inc"),
                        str(HERE / "stick_chord_test.cpp"), "-o", str(exe)], check=True)
        out = subprocess.run([str(exe)], check=True, capture_output=True, text=True).stdout
        print(out.strip())
    return 0


if __name__ == "__main__":
    sys.exit(main())
