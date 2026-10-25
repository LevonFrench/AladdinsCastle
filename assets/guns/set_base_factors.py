#!/usr/bin/env python3
"""Set the body and accent baseColorFactor of built GLBs to 1.0 without Blender.

Colour contract: the TOML tint is the colour, so the two tinted roles carry a
neutral base factor of 1.0 (alpha untouched); dark and glass keep their fixed
colours. Only the GLB JSON chunk is rewritten. The binary chunk is copied
byte-exact, the JSON is re-padded with spaces to 4 bytes and both length fields
are fixed. Idempotent. GPL-3.0-only script; models CC0.

Usage:
  python assets/guns/set_base_factors.py           rewrite assets/guns/*.glb and assets/hands/*.glb
  python assets/guns/set_base_factors.py --check   exit 1 if any file still needs the change
  python assets/guns/set_base_factors.py a.glb ... only these files
"""
import json
from pathlib import Path
import struct
import sys

ROOT = Path(__file__).resolve().parents[2]
TINTED = ('body', 'accent')
ONE = [1, 1, 1]


def rewrite(data):
    """Return the GLB bytes with body/accent base factors at 1.0 (data itself when already so)."""
    magic, version, total = struct.unpack('<4sII', data[:12])
    if magic != b'glTF' or version != 2 or total != len(data):
        raise ValueError('not a GLB 2 file')
    json_length, json_type = struct.unpack('<I4s', data[12:20])
    if json_type != b'JSON':
        raise ValueError('first chunk is not JSON')
    document = json.loads(data[20:20 + json_length].decode('utf-8'))
    changed = False
    for material in document['materials']:
        if material.get('name') not in TINTED:
            continue
        pbr = material.setdefault('pbrMetallicRoughness', {})
        factor = pbr.get('baseColorFactor', [1, 1, 1, 1])
        if list(factor[:3]) != ONE:
            pbr['baseColorFactor'] = ONE + [factor[3] if len(factor) > 3 else 1]
            changed = True
    if not changed:
        return data
    text = json.dumps(document, separators=(',', ':'), ensure_ascii=False).encode('utf-8')
    text += b' ' * (-len(text) % 4)
    rest = data[20 + json_length:]  # binary chunk header and payload, untouched
    return (struct.pack('<4sII', b'glTF', 2, 12 + 8 + len(text) + len(rest)) +
            struct.pack('<I4s', len(text), b'JSON') + text + rest)


def main(argv):
    check = '--check' in argv
    paths = [Path(a) for a in argv if not a.startswith('--')]
    if not paths:
        paths = sorted((ROOT / 'assets/guns').glob('*.glb')) + sorted((ROOT / 'assets/hands').glob('*.glb'))
    stale = 0
    for path in paths:
        old = path.read_bytes()
        new = rewrite(old)
        if new == old:
            continue
        stale += 1
        if not check:
            path.write_bytes(new)
        print(('needs change: ' if check else 'updated: ') + path.name)
    if check and stale:
        sys.exit(1)
    print(f'{len(paths)} GLB files checked, {stale} {"stale" if check else "updated"}')


if __name__ == '__main__':
    main(sys.argv[1:])
