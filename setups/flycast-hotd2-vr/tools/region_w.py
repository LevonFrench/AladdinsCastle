# Depth (W) of the polygons inside a screen box of a hotd2-vr rip pass:
# python region_w.py <passNNNN.bin> x0 y0 x1 y1
import sys, struct


def read_pass(path):
    data = open(path, 'rb').read()
    magic, fbw, fbh, fx, fy, hudw, stock, lists = struct.unpack_from('<4sIIffffI', data, 0)
    off = 32
    polys = []
    for li in range(lists):
        (count,) = struct.unpack_from('<I', data, off); off += 4
        for _ in range(count):
            isp, tsp, tcw, pcw, n = struct.unpack_from('<5I', data, off); off += 20
            verts = []
            for _ in range(n):
                x, y, z, u, v = struct.unpack_from('<5f', data, off); off += 28
                verts.append((x, y, z))
            polys.append((li, isp, tsp, tcw, pcw, verts))
    return (fbw, fbh, fx, fy), polys


head, polys = read_pass(sys.argv[1])
x0, y0, x1, y1 = map(float, sys.argv[2:6])
ws = sorted(1 / v[2] for p in polys for v in p[5]
            if x0 <= v[0] <= x1 and y0 <= v[1] <= y1 and v[2] > 0 and 0.5 < 1 / v[2] < 10000)
print('focal %.1f n %d  W p10 %.2f median %.2f p90 %.2f' % (head[2], len(ws), ws[len(ws) // 10], ws[len(ws) // 2], ws[9 * len(ws) // 10]))
