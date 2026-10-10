#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""Read-only exact generic material evidence guards; no engine/content access."""
import argparse
import hashlib
import json
import pathlib
import subprocess

FILES = {"engine/tex_bake.c", "engine/tex_bake.h", "engine/eng.c", "engine/eng.h",
         "engine/quad_gl.c", "engine/fog_hw.c", "engine/fog_hw.h"}
HERE = pathlib.Path(__file__).resolve().parent

def verify(manifest, source):
    if set(manifest["files"]) != FILES:
        raise ValueError("Material evidence allowlist mismatch")
    for name, expected in manifest["files"].items():
        path = source / name
        if path.is_symlink() or not path.resolve().is_relative_to(source.resolve()):
            raise ValueError("Material evidence redirects outside source")
        digest = hashlib.sha256(path.read_text(encoding="utf-8").encode()).hexdigest()
        if digest != expected:
            raise ValueError(f"Material evidence drift: {name}")

def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--source", type=pathlib.Path, required=True)
    args = parser.parse_args()
    source = args.source.resolve()
    manifest = json.loads((HERE.parent / "patches/materials-source.json").read_text(encoding="utf-8"))
    commit = subprocess.check_output(["git", "-C", str(source), "rev-parse", "HEAD"], text=True).strip()
    if commit != manifest["base_commit"]:
        raise ValueError("Material engine commit mismatch")
    dirty = subprocess.check_output(["git", "--no-optional-locks", "-C", str(source), "status", "--porcelain", "--", *sorted(FILES)], text=True)
    if dirty:
        raise ValueError("Selected material reference files are modified")
    verify(manifest, source)
    print(f"Verified {len(FILES)} generic material evidence hashes at {commit}")

if __name__ == "__main__":
    try:
        main()
    except (ValueError, OSError, subprocess.CalledProcessError) as error:
        raise SystemExit(str(error))
