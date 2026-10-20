#!/usr/bin/env python
"""Contact sheet of probe screenshots: python -B make_sheet.py <dir> <out.png> [--cols N] [--width W] [--match TEXT]

Shots are named by the probe script (e.g. d004 → 'd004'); Dolphin may add a prefix or suffix,
so the label is the file stem. Sorted by name.
"""
import argparse
from pathlib import Path

from PIL import Image, ImageDraw


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('dir')
    ap.add_argument('out')
    ap.add_argument('--cols', type=int, default=6)
    ap.add_argument('--width', type=int, default=320)
    ap.add_argument('--match', default='')
    a = ap.parse_args()
    files = sorted(p for p in Path(a.dir).glob('*.png') if a.match in p.name and p.resolve() != Path(a.out).resolve())
    if not files:
        raise SystemExit('no pictures')
    thumbs = []
    for p in files:
        im = Image.open(p).convert('RGB')
        h = round(im.height * a.width / im.width)
        thumbs.append((p.stem, im.resize((a.width, h))))
    th = max(t.height for _, t in thumbs) + 16
    rows = (len(thumbs) + a.cols - 1) // a.cols
    sheet = Image.new('RGB', (a.cols * a.width, rows * th), 'black')
    d = ImageDraw.Draw(sheet)
    for i, (name, t) in enumerate(thumbs):
        x, y = (i % a.cols) * a.width, (i // a.cols) * th
        sheet.paste(t, (x, y + 16))
        d.text((x + 3, y + 2), name, fill='yellow')
    sheet.save(a.out)
    print(a.out, len(thumbs))


if __name__ == '__main__':
    main()
