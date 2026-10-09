#!/usr/bin/env python3
"""Generate a tiny synthetic ISO/CHDv5 fixture. Requires an already supplied chdman.

Only synthetic bytes are generated. No owned game files or directory names are read.
Usage: python tools/make_synthetic_chd_fixture.py --request <private chdman JSON>
"""
import argparse
import base64
import json
import pathlib
import struct
import subprocess

p = argparse.ArgumentParser()
p.add_argument("--request", type=pathlib.Path, required=True)
p.add_argument("--output", type=pathlib.Path, default=pathlib.Path(".local/synthetic-chd"))
args = p.parse_args()
tool = json.loads(args.request.read_text(encoding="utf-8-sig"))["chdman"]
args.output.mkdir(parents=True, exist_ok=True)
iso = args.output / "synthetic.iso"
chd = args.output / "synthetic.chd"
b = bytearray(32 * 2048)
b[16 * 2048:16 * 2048 + 7] = b"\1CD001\1"
b[16 * 2048 + 156] = 34
struct.pack_into("<I", b, 16 * 2048 + 158, 20)
struct.pack_into("<I", b, 16 * 2048 + 166, 2048)
name = b"SYSTEM.CNF;1"
b[20 * 2048] = 33 + len(name)
struct.pack_into("<I", b, 20 * 2048 + 2, 21)
struct.pack_into("<I", b, 20 * 2048 + 10, 64)
b[20 * 2048 + 32] = len(name)
b[20 * 2048 + 33:20 * 2048 + 33 + len(name)] = name
boot = b"BOOT2 = cdrom0:\\SLUS_202.19;1\r\n"
b[21 * 2048:21 * 2048 + len(boot)] = boot
iso.write_bytes(b)
result = subprocess.run([tool, "createraw", "-i", str(iso.resolve()), "-o", str(chd.resolve()), "-hs", "4096", "-us", "2048", "-c", "zlib", "-f"], stdout=subprocess.PIPE, stderr=subprocess.PIPE)
if result.returncode:
    raise SystemExit("Synthetic CHD generator failed; inspect private tool output locally")
encoded = base64.b64encode(chd.read_bytes()).decode("ascii")
(args.output / "fixture.b64").write_text(encoded + "\n", encoding="ascii")
print(f"Synthetic compressed CHDv5 fixture: {chd.stat().st_size} bytes")
