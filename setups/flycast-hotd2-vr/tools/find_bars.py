"""Lists the polygons in a hotd2-vr rip pass that look like screen bars: wide, flat in W, dark.
Usage: python find_bars.py <passNNNN.bin> [...]"""
import struct
import sys


def read_pass(path):
    data = open(path, 'rb').read()
    magic, fbw, fbh, fx, fy, hudw, stock, lists = struct.unpack_from('<4sIIffffI', data, 0)
    off = 32
    polys = []
    for li in range(lists):
        (count,) = struct.unpack_from('<I', data, off)
        off += 4
        for _ in range(count):
            isp, tsp, tcw, pcw, n = struct.unpack_from('<5I', data, off)
            off += 20
            verts = []
            for _ in range(n):
                x, y, z, u, v = struct.unpack_from('<5f', data, off)
                col = data[off + 20:off + 24]
                off += 28
                verts.append((x, y, z, u, v, tuple(col)))
            polys.append((('OP', 'PT', 'TR')[li], isp, tsp, tcw, pcw, verts))
    return (fbw, fbh, fx, fy, hudw), polys


for path in sys.argv[1:]:
    head, polys = read_pass(path)
    print(f'== {path}: fb {head[0]}x{head[1]} focal {head[2]:.1f}x{head[3]:.1f} hudW {head[4]:.3f} polys {len(polys)}')
    for i, (lst, isp, tsp, tcw, pcw, verts) in enumerate(polys):
        if len(verts) < 3:
            continue
        xs = [v[0] for v in verts]; ys = [v[1] for v in verts]
        ws = [1 / v[2] if v[2] > 0 else float('inf') for v in verts]
        width = max(xs) - min(xs)
        flat = max(ws) - min(ws) < 0.01
        dark = all(v[5][0] < 16 and v[5][1] < 16 and v[5][2] < 16 for v in verts)
        if width > 500 and flat:
            textured = (pcw >> 3) & 1
            print(f'  #{i} {lst} n={len(verts)} x {min(xs):.0f}-{max(xs):.0f} y {min(ys):.0f}-{max(ys):.0f} '
                  f'W {ws[0]:.4f} textured={textured} tex={(tcw & 0x1FFFFF) << 3:06x} '
                  f'col={verts[0][5]} dark={dark}')
