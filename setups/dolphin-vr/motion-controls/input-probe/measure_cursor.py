#!/usr/bin/env python
"""Measure where a game draws its pointer cursor for each scripted aim point.

usage: python measure_cursor.py <run folder> [--color r,g,b] [--tol N] [--out result.json]

Baseline: the per-pixel median of all aim pictures (the cursor moves between them, so the median
is the screen without it; some games keep drawing the cursor when the pointer leaves the screen).
Measured pictures: every <run>/aim-<i>_<u>_<v>.png.
The cursor is the set of pixels that changed against the baseline AND are close to the cursor colour
(default: picked automatically as the most common changed colour in the centre picture). Its
centroid, as a fraction of the picture (x right, y down), is compared with the aim point
x = (u + 1) / 2, y = (1 - v) / 2. Prints a table and writes JSON with the per-axis least-squares
trim (scale and shift about the centre) that would map the aim point onto the drawn cursor.
"""
import argparse
import glob
import json
import os
import re
from collections import Counter

from PIL import Image, ImageChops


def load(path):
    return Image.open(path).convert('RGB')


def mask_points(img, base, color, tol, diff_min=40):
    w, h = img.size
    a, b = img.load(), base.load()
    xs, ys = [], []
    for y in range(0, h, 2):
        for x in range(0, w, 2):
            p, q = a[x, y], b[x, y]
            if abs(p[0] - q[0]) + abs(p[1] - q[1]) + abs(p[2] - q[2]) < diff_min:
                continue
            if color and sum(abs(p[i] - color[i]) for i in range(3)) > tol:
                continue
            xs.append(x)
            ys.append(y)
    return xs, ys


def dominant_changed_color(img, base):
    w, h = img.size
    a, b = img.load(), base.load()
    c = Counter()
    for y in range(0, h, 3):
        for x in range(0, w, 3):
            p, q = a[x, y], b[x, y]
            if abs(p[0] - q[0]) + abs(p[1] - q[1]) + abs(p[2] - q[2]) >= 60:
                c[(p[0] // 16 * 16 + 8, p[1] // 16 * 16 + 8, p[2] // 16 * 16 + 8)] += 1
    return c.most_common(1)[0][0] if c else None


def fit(pairs):
    # target = s * (aim - 0.5) + 0.5 + k  -> least squares for s, k
    n = len(pairs)
    xs = [p[0] - 0.5 for p in pairs]
    ys = [p[1] - 0.5 for p in pairs]
    mx, my = sum(xs) / n, sum(ys) / n
    sxx = sum((x - mx) ** 2 for x in xs)
    s = sum((x - mx) * (y - my) for x, y in zip(xs, ys)) / sxx if sxx else 1.0
    return s, my - s * mx


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('run')
    ap.add_argument('--color')
    ap.add_argument('--tol', type=int, default=120)
    ap.add_argument('--out')
    ap.add_argument('--point', default='centroid', choices=['centroid', 'top'],
                    help='top = fingertip of a hand cursor (topmost changed pixels)')
    args = ap.parse_args()
    shots = []
    for f in glob.glob(os.path.join(args.run, 'aim-*_*_*.png')):
        m = re.match(r'aim-(\d+)_(-?[\d.]+)_(-?[\d.]+)\.png', os.path.basename(f))
        if m:
            shots.append((int(m.group(1)), float(m.group(2)), float(m.group(3)), f))
    shots.sort()
    import numpy as np
    stack = np.stack([np.asarray(load(f)) for _, _, _, f in shots])
    base = Image.fromarray(np.median(stack, axis=0).astype('uint8'))
    color = tuple(int(v) for v in args.color.split(',')) if args.color else None
    if color is None:
        centre = next(s for s in shots if s[1] == 0 and s[2] == 0)
        color = dominant_changed_color(load(centre[3]), base)
    rows, worst = [], 0.0
    for i, u, v, f in shots:
        img = load(f)
        if img.size != base.size:
            img = img.resize(base.size)
        xs, ys = mask_points(img, base, color, args.tol)
        w, h = base.size
        aim = ((u + 1) / 2, (1 - v) / 2)
        if len(xs) < 5:
            rows.append({'point': i, 'u': u, 'v': v, 'aim': aim, 'cursor': None})
            print(f'{i} u={u:+.2f} v={v:+.2f} aim=({aim[0]:.3f},{aim[1]:.3f}) cursor not found')
            continue
        if args.point == 'top':
            y0 = min(ys)
            band = [x for x, y in zip(xs, ys) if y <= y0 + 0.012 * h]
            cur = (sum(band) / len(band) / w, y0 / h)
        else:
            cur = (sum(xs) / len(xs) / w, sum(ys) / len(ys) / h)
        err = (cur[0] - aim[0], cur[1] - aim[1])
        worst = max(worst, abs(err[0]), abs(err[1]))
        rows.append({'point': i, 'u': u, 'v': v, 'aim': aim, 'cursor': cur, 'error': err, 'pixels': len(xs)})
        print(f'{i} u={u:+.2f} v={v:+.2f} aim=({aim[0]:.3f},{aim[1]:.3f}) cursor=({cur[0]:.3f},{cur[1]:.3f}) '
              f'err=({err[0]:+.3f},{err[1]:+.3f}) px={len(xs)}')
    good = [r for r in rows if r['cursor']]
    sx, kx = fit([(r['aim'][0], r['cursor'][0]) for r in good])
    sy, ky = fit([(r['aim'][1], r['cursor'][1]) for r in good])
    print(f'colour {color}; max error {worst:.3f}; cursor = aim*scale+shift about centre: '
          f'x scale {sx:.3f} shift {kx:+.3f}, y scale {sy:.3f} shift {ky:+.3f}')
    if args.out:
        json.dump({'run': os.path.abspath(args.run), 'colour': color, 'points': rows, 'max_error': worst,
                   'x_scale': sx, 'x_shift': kx, 'y_scale': sy, 'y_shift': ky}, open(args.out, 'w'), indent=1)


if __name__ == '__main__':
    main()
