#!/usr/bin/env python
"""Measure a cursor by a colour feature that the background lacks.

usage: python measure_feature.py <run> --rgb rmin,rmax,gmin,gmax,bmin,bmax [--point centroid|top]
Galaxy's star pointer: the saturated blue star (centroid). Wii Sports Resort's hand: its saturated
blue outline (top = fingertip). Prints per-point error and the per-axis fit like measure_cursor.py.
"""
import argparse, glob, os, re
from PIL import Image


def fit(pairs):
    xs = [p[0] - 0.5 for p in pairs]; ys = [p[1] - 0.5 for p in pairs]; n = len(xs)
    mx, my = sum(xs) / n, sum(ys) / n
    sxx = sum((x - mx) ** 2 for x in xs)
    s = sum((x - mx) * (y - my) for x, y in zip(xs, ys)) / sxx if sxx else 1.0
    return s, my - s * mx


ap = argparse.ArgumentParser(); ap.add_argument('run'); ap.add_argument('--rgb', required=True)
ap.add_argument('--point', default='centroid', choices=['centroid', 'top'])
a = ap.parse_args(); lim = [int(v) for v in a.rgb.split(',')]
rows = []
for f in sorted(glob.glob(os.path.join(a.run, 'aim-*_*_*.png'))):
    m = re.match(r'aim-(\d+)_(-?[\d.]+)_(-?[\d.]+)\.png', os.path.basename(f))
    if not m: continue
    im = Image.open(f).convert('RGB'); w, h = im.size; px = im.load()
    pts = [(x, y) for y in range(0, h, 2) for x in range(0, w, 2)
           if lim[0] <= px[x, y][0] <= lim[1] and lim[2] <= px[x, y][1] <= lim[3] and lim[4] <= px[x, y][2] <= lim[5]]
    u, v = float(m.group(2)), float(m.group(3)); aim = ((u + 1) / 2, (1 - v) / 2)
    if len(pts) < 5:
        print(m.group(1), 'not found'); continue
    if a.point == 'top':
        y0 = min(p[1] for p in pts); band = [p[0] for p in pts if p[1] <= y0 + 0.012 * h]
        cur = (sum(band) / len(band) / w, y0 / h)
    else:
        cur = (sum(p[0] for p in pts) / len(pts) / w, sum(p[1] for p in pts) / len(pts) / h)
    rows.append((aim, cur))
    print(f'{m.group(1)} aim=({aim[0]:.3f},{aim[1]:.3f}) cursor=({cur[0]:.3f},{cur[1]:.3f}) err=({cur[0]-aim[0]:+.3f},{cur[1]-aim[1]:+.3f}) n={len(pts)}')
sx, kx = fit([(r[0][0], r[1][0]) for r in rows]); sy, ky = fit([(r[0][1], r[1][1]) for r in rows])
print(f'max error {max(max(abs(c[0]-a_[0]), abs(c[1]-a_[1])) for a_, c in rows):.3f}; x scale {sx:.3f} shift {kx:+.3f}, y scale {sy:.3f} shift {ky:+.3f}')
