# How much of the area around the original 4:3 frame holds game polygons, by kind, from a rip pass (the polygons as
# the game submitted them: screen px and 1/W). The ring is the frame scaled by EXT (2: one half frame beyond each
# side), minus the frame. Kinds by the polygon's median W: world (10 .. 50000), near (< 10, HUD and near-plane
# clamps), overlay (the 2D plane's W), background (>= 50000, the game's clear quad).
#   python outside_frame.py <pass0000.bin> [...]
# (A rip pass is written by ACVR_RIP="seconds:1,1,0" on a PC run; it holds game data: keep it private.)
import sys, struct
import numpy as np

EXT = 2.0


def read_pass(path):
    d = open(path, 'rb').read()
    magic, fbw, fbh, fx, fy, hudw, stock, lists = struct.unpack_from('<4sIIffffI', d, 0)
    off = 32
    polys = []
    for li in range(lists):
        (c,) = struct.unpack_from('<I', d, off)
        off += 4
        for _ in range(c):
            isp, tsp, tcw, pcw, n = struct.unpack_from('<5I', d, off)
            off += 20
            vs = []
            for _ in range(n):
                x, y, z, u, v = struct.unpack_from('<5f', d, off)
                off += 28
                vs.append((x, y, z))
            polys.append((li, pcw, vs))
    return (fbw, fbh, hudw), polys


def raster(mask, t, cell, ox, oy):
    (x0, y0), (x1, y1), (x2, y2) = t
    xs = [x0, x1, x2]
    ys = [y0, y1, y2]
    gx0 = max(0, int((min(xs) - ox) / cell))
    gx1 = min(mask.shape[1] - 1, int((max(xs) - ox) / cell) + 1)
    gy0 = max(0, int((min(ys) - oy) / cell))
    gy1 = min(mask.shape[0] - 1, int((max(ys) - oy) / cell) + 1)
    if gx1 < gx0 or gy1 < gy0:
        return
    gx, gy = np.meshgrid(np.arange(gx0, gx1 + 1), np.arange(gy0, gy1 + 1))
    px = ox + (gx + .5) * cell
    py = oy + (gy + .5) * cell
    den = (y1 - y2) * (x0 - x2) + (x2 - x1) * (y0 - y2)
    if abs(den) < 1e-9:
        return
    a = ((y1 - y2) * (px - x2) + (x2 - x1) * (py - y2)) / den
    b = ((y2 - y0) * (px - x2) + (x0 - x2) * (py - y2)) / den
    c = 1 - a - b
    mask[gy0:gy1 + 1, gx0:gx1 + 1] |= (a >= 0) & (b >= 0) & (c >= 0)


def analyse(path, cell=4):
    (fw, fh, hudw), polys = read_pass(path)
    ox, oy = -(EXT - 1) / 2 * fw, -(EXT - 1) / 2 * fh
    W, H = int(EXT * fw / cell), int(EXT * fh / cell)
    masks = {k: np.zeros((H, W), bool) for k in ('world', 'hud', 'overlay', 'bg')}
    for li, pcw, vs in polys:
        if len(vs) < 3 or any(not (v[2] > 0) or not np.isfinite(v[0]) or not np.isfinite(v[1]) for v in vs):
            continue
        w = sorted(1 / v[2] for v in vs)[len(vs) // 2]
        k = 'overlay' if abs(w - hudw) < 0.002 else 'hud' if w < 10 else 'bg' if w >= 50000 else 'world'
        for i in range(len(vs) - 2):   # strips
            raster(masks[k], tuple((vs[i + j][0], vs[i + j][1]) for j in range(3)), cell, ox, oy)
    gx = (np.arange(W) + .5) * cell + ox
    gy = (np.arange(H) + .5) * cell + oy
    inside = (gx[None, :] >= 0) & (gx[None, :] < fw) & (gy[:, None] >= 0) & (gy[:, None] < fh)
    ring = ~inside
    out = {k: 100.0 * (m & ring).sum() / ring.sum() for k, m in masks.items()}
    out['any'] = 100.0 * ((masks['world'] | masks['hud'] | masks['overlay'] | masks['bg']) & ring).sum() / ring.sum()
    return out


if __name__ == '__main__':
    for p in sys.argv[1:]:
        o = analyse(p)
        print('%s  ring: world %5.1f%%  near %5.1f%%  overlay %5.1f%%  background %5.1f%%  any %5.1f%%'
              % (p.replace('\\', '/').split('/')[-1], o['world'], o['hud'], o['overlay'], o['bg'], o['any']))