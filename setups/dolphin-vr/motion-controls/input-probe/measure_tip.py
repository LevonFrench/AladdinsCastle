#!/usr/bin/env python
"""Measure an arrow cursor by its tip: for each aim-<i>_<u>_<v>.png, the top-left corner of the pixels
that are clearly blue (blue well above red, green above red), as a fraction of the picture.
Used for Metroid Prime Trilogy's blue arrow, whose tip points up-left; the colour test misses
the lightest pixels at the very tip, so the reported tip sits about 0.02 of the height low.

usage: python measure_tip.py <run folder> [--skip i,j]
"""
import argparse
import glob
import os
import re

from PIL import Image


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('run')
    ap.add_argument('--skip', default='')
    args = ap.parse_args()
    skip = {int(s) for s in args.skip.split(',') if s}
    rows = []
    for f in sorted(glob.glob(os.path.join(args.run, 'aim-*.png'))):
        m = re.match(r'aim-(\d+)_(-?[\d.]+)_(-?[\d.]+)\.png', os.path.basename(f))
        if not m or int(m.group(1)) in skip:
            continue
        im = Image.open(f).convert('RGB')
        w, h = im.size
        px = im.load()
        pts = [(x, y) for y in range(0, h, 2) for x in range(0, w, 2)
               if px[x, y][2] - px[x, y][0] > 45 and px[x, y][2] > 130 and px[x, y][1] - px[x, y][0] > 15]
        u, v = float(m.group(2)), float(m.group(3))
        aim = ((u + 1) / 2, (1 - v) / 2)
        if not pts:
            print(f'{m.group(1)} not found')
            continue
        tip = (min(p[0] for p in pts) / w, min(p[1] for p in pts) / h)
        rows.append((u, v, aim, tip))
        print(f'{m.group(1)} u={u:+.2f} v={v:+.2f} aim=({aim[0]:.3f},{aim[1]:.3f}) tip=({tip[0]:.3f},{tip[1]:.3f}) '
              f'err=({tip[0] - aim[0]:+.3f},{tip[1] - aim[1]:+.3f})')
    print('max error', max(max(abs(t[0] - a[0]), abs(t[1] - a[1])) for _, _, a, t in rows))


if __name__ == '__main__':
    main()
