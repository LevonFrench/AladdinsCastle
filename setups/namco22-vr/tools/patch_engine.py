#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""Exact, opt-in hook scaffolding on selected pinned engine source files only.

No engine launch, game content, tree walk, download, or edit of the reference.
Output is a private source overlay, not a complete or playable engine build.
"""
import argparse
import hashlib
import json
import pathlib
import subprocess

FILES = {"engine/ss22_run.c", "engine/ss22_video.c", "engine/geo_hw.c", "engine/ss22_gl.c"}
HEADERS = {"engine/ss22_gl.h", "engine/geo_hw.h", "engine/hud_edges.h", "engine/ss22_board.h"}
HERE = pathlib.Path(__file__).resolve().parent

def digest(text):
    return hashlib.sha256(text.encode("utf-8")).hexdigest()

def render(manifest, source):
    if set(manifest["files"]) != FILES:
        raise ValueError("Manifest file allowlist mismatch")
    if set(manifest["headers"]) != HEADERS:
        raise ValueError("Manifest header allowlist mismatch")
    for name, expected in manifest["headers"].items():
        path = source / name
        if path.is_symlink() or not path.resolve().is_relative_to(source.resolve()) or digest(path.read_text(encoding="utf-8")) != expected:
            raise ValueError(f"Header drift: {name}")
    rendered = {}
    for name, spec in manifest["files"].items():
        path = source / name
        if path.is_symlink() or not path.resolve().is_relative_to(source.resolve()):
            raise ValueError("Source redirects outside selected source root")
        original = path.read_text(encoding="utf-8")  # normalized newlines
        if digest(original) != spec["sha256_lf"]:
            raise ValueError(f"Source drift: {name}")
        text = original
        for edit in spec["edits"]:
            marker = f"/* ACVR-PATCH:{edit['id']} */"
            if marker in text or text.count(edit["old"]) != 1:
                raise ValueError(f"Anchor/marker drift: {name}: {edit['id']}")
            text = text.replace(edit["old"], marker + "\n" + edit["new"], 1)
        rendered[name] = text
    return rendered

def write_overlay(rendered, output, checkout):
    root = checkout.resolve()
    local = root / ".local"
    if local.resolve() != local or not output.resolve().is_relative_to(local):
        raise ValueError("Output must resolve under this checkout's .local directory")
    for name, text in rendered.items():
        path = output / name
        if path.is_symlink() or not path.resolve().is_relative_to(output.resolve()):
            raise ValueError("Output redirects outside overlay")
        if path.exists() and path.read_text(encoding="utf-8") != text:
            raise ValueError(f"Existing output drift: {name}")
    # All files and edits have passed before any writes. Existing identical output
    # is verified per edit/file; do not skip based on a single whole-file marker.
    for name, text in rendered.items():
        path = output / name
        if not path.exists():
            path.parent.mkdir(parents=True, exist_ok=True)
            with path.open("x", encoding="utf-8", newline="\n") as stream:
                stream.write(text)

def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--source", type=pathlib.Path, required=True)
    parser.add_argument("--output", type=pathlib.Path)
    args = parser.parse_args()
    manifest = json.loads((HERE.parent / "patches/manifest.json").read_text(encoding="utf-8"))
    source = args.source.resolve()
    commit = subprocess.check_output(["git", "-C", str(source), "rev-parse", "HEAD"], text=True).strip()
    if commit != manifest["base_commit"]:
        raise ValueError("Engine commit mismatch")
    # Inspect only selected engine code; do not scan assets or game directories.
    dirty = subprocess.check_output(["git", "--no-optional-locks", "-C", str(source), "status", "--porcelain", "--", *sorted(FILES | HEADERS)], text=True)
    if dirty:
        raise ValueError("Selected reference engine files are modified")
    rendered = render(manifest, source)
    if args.output:
        if args.output.resolve().is_relative_to(source) or source.is_relative_to(args.output.resolve()):
            raise ValueError("Reference/output trees must be separate")
        overlay = dict(rendered)
        overlay["UPSTREAM-LICENSE"] = (HERE.parent / "patches/UPSTREAM-LICENSE").read_text(encoding="utf-8")
        write_overlay(overlay, args.output, pathlib.Path.cwd())
    print(f"Validated {sum(len(x['edits']) for x in manifest['files'].values())} exact hooks on {len(rendered)} engine files and {len(HEADERS)} header guards at {commit}")

if __name__ == "__main__":
    try:
        main()
    except (ValueError, OSError, subprocess.CalledProcessError) as error:
        raise SystemExit(str(error))
