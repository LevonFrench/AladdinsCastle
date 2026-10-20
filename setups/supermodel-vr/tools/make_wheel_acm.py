"""Writes the steering wheel the patched Supermodel draws in head-tracked mode (ACVR_CONTROLS=1), as a procedural,
closed mesh: a thick round rim, three spokes (left, right, bottom), a plain solid centre hub and a short column.
No logo, no texture. Every part is a closed solid with outward normals, so it looks solid from any side.

  python make_wheel_acm.py <out.acm>          (the patch reads Assets/acvr/wheel-solid.acm)

Metres, +Y up, -Z forward (away from the driver); the wheel turns about the Z axis through the origin.
File layout: see bake_control.py ('ACM1', parts, pivot origin, then per part: moving, colour, vertices, indices).
GPL-3.0-or-later, like the patch.
"""
import math
import struct
import sys

RIM_R, RIM_TUBE = 0.170, 0.021          # rim centre-line radius, rim tube radius (a 42 mm thick rim)
HUB_R, HUB_FRONT, HUB_BACK = 0.050, 0.030, -0.022
SPOKE_W, SPOKE_T, SPOKE_Z = 0.034, 0.016, 0.004
RUBBER, METAL, HUB, COLUMN = (0.045, 0.045, 0.05), (0.50, 0.52, 0.56), (0.10, 0.10, 0.11), (0.07, 0.07, 0.08)


class Mesh:
    def __init__(self):
        self.v, self.i = [], []

    def vert(self, p, n):
        length = math.sqrt(sum(c * c for c in n)) or 1.0
        self.v.append(tuple(p) + tuple(c / length for c in n))
        return len(self.v) - 1

    def tri(self, a, b, c):
        self.i += [a, b, c]

    def quad(self, a, b, c, d):
        self.i += [a, b, c, a, c, d]


def torus(R, r, seg=64, ring=20):
    m = Mesh()
    for i in range(seg + 1):
        a = 2 * math.pi * i / seg
        for j in range(ring + 1):
            b = 2 * math.pi * j / ring
            n = (math.cos(a) * math.cos(b), math.sin(a) * math.cos(b), math.sin(b))
            p = ((R + r * math.cos(b)) * math.cos(a), (R + r * math.cos(b)) * math.sin(a), r * math.sin(b))
            m.vert(p, n)
    for i in range(seg):
        for j in range(ring):
            a = i * (ring + 1) + j
            b = (i + 1) * (ring + 1) + j
            m.quad(a, b, b + 1, a + 1)
    return m


def cylinder(r, z0, z1, seg=40, bevel=0.0):
    """Closed cylinder along Z, both ends capped (flat caps with their own normals; optional bevel ring)."""
    m = Mesh()
    rings = [(r - bevel, z0), (r, z0 + bevel), (r, z1 - bevel), (r - bevel, z1)] if bevel else [(r, z0), (r, z1)]
    base = []
    for (rr, z) in rings:
        row = []
        for k in range(seg + 1):
            a = 2 * math.pi * k / seg
            nz = -1.0 if (bevel and z == z0) else (1.0 if (bevel and z == z1) else 0.0)
            row.append(m.vert((rr * math.cos(a), rr * math.sin(a), z), (math.cos(a), math.sin(a), nz * 0.7)))
        base.append(row)
    for q in range(len(base) - 1):
        for k in range(seg):
            m.quad(base[q][k], base[q][k + 1], base[q + 1][k + 1], base[q + 1][k])
    for (rr, z, nz) in ((rings[0][0], z0, -1.0), (rings[-1][0], z1, 1.0)):
        c = m.vert((0, 0, z), (0, 0, nz))
        ring = [m.vert((rr * math.cos(2 * math.pi * k / seg), rr * math.sin(2 * math.pi * k / seg), z), (0, 0, nz)) for k in range(seg + 1)]
        for k in range(seg):
            if nz > 0:
                m.tri(c, ring[k], ring[k + 1])
            else:
                m.tri(c, ring[k + 1], ring[k])
    return m


def box(center, axis_u, axis_v, axis_w, hu, hv, hw):
    """Closed box: centre, three orthonormal axes and half sizes along them. Outward normals, outward winding."""
    m = Mesh()
    for (n, a, b, h, ha, hb) in ((axis_u, axis_v, axis_w, hu, hv, hw), (axis_v, axis_w, axis_u, hv, hw, hu), (axis_w, axis_u, axis_v, hw, hu, hv)):
        for s in (1, -1):
            nn = [s * c for c in n]
            corners = []
            for (sa, sb) in ((-1, -1), (1, -1), (1, 1), (-1, 1)):
                corners.append([center[k] + nn[k] * h + a[k] * sa * ha * s + b[k] * sb * hb for k in range(3)])
            idx = [m.vert(p, nn) for p in corners]
            m.quad(*idx)
    return m


def main(out):
    parts = []
    parts.append((1, RUBBER, torus(RIM_R, RIM_TUBE)))
    spoke_len = RIM_R - HUB_R * 0.6
    for ang in (0.0, math.pi, -math.pi / 2):             # right, left, bottom
        d = (math.cos(ang), math.sin(ang), 0.0)
        side = (-d[1], d[0], 0.0)
        mid = HUB_R * 0.6 + spoke_len / 2
        parts.append((1, METAL, box((d[0] * mid, d[1] * mid, SPOKE_Z), d, side, (0, 0, 1), spoke_len / 2, SPOKE_W / 2, SPOKE_T / 2)))
    parts.append((1, HUB, cylinder(HUB_R, HUB_BACK, HUB_FRONT, bevel=0.006)))
    parts.append((0, COLUMN, cylinder(0.024, -0.26, HUB_BACK + 0.001)))
    with open(out, 'wb') as f:
        f.write(b'ACM1' + struct.pack('<I3f', len(parts), 0.0, 0.0, 0.0))
        for moving, colour, m in parts:
            f.write(struct.pack('<I3fII', moving, *colour, len(m.v), len(m.i)))
            for v in m.v:
                f.write(struct.pack('<6f', *v))
            f.write(struct.pack('<%dI' % len(m.i), *m.i))
    print(f'{out}: {len(parts)} parts, {sum(len(m.v) for _, _, m in parts)} vertices, {sum(len(m.i) for _, _, m in parts) // 3} triangles')


if __name__ == '__main__':
    main(sys.argv[1])
