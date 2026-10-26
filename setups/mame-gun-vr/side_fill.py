#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Measure the side areas of offline theatre captures (open frame and soft edges).

Reads capture folders written by `GunViewer --offline-proof` (native-proof.json and
left-picture.png; left-plain.png too when the run used --soft-edges). The side areas
are the two strips between a 4:3 frame and a 16:9 frame of the picture's height, the
same strips the Namco System 22 open frame fills with game geometry. Reported:

  side_fill      share of side-strip pixels with picture (r+g+b > 30, as the status
                 page's open-frame measure counts it)
  side_level     mean brightness of the side strips over the 4:3 picture's mean
  edge_step      how hard the picture's outer left/right edge is: mean brightness
                 change across it (3 px in, 3 px out) over the 4:3 picture's mean
                 brightness; near 0 is no visible edge (a dark game edge is soft too)

`plain` is the same frame drawn without soft edges. Soft-edge fill is the game's own
edge carried outwards, not new game picture; the report keeps the two apart.

    python side_fill.py <sheet.png> <capture-folder> [<capture-folder> ...]

Requires Pillow and NumPy. Prints one JSON object per folder and writes a contact
sheet (plain above, as drawn below, side strips outlined).
"""
import json
import sys
from pathlib import Path

import numpy as np
from PIL import Image, ImageDraw

DISTANCE, EYE_X = 2.5, -.032   # the offline proof's fixed left eye, 90 degree square view


def to_px(x, y, w, h):
    return (x - EYE_X) / DISTANCE * w / 2 + w / 2 - .5, h / 2 - y / DISTANCE * h / 2 - .5


def measure(image, meta):
    rgb = np.asarray(image.convert("RGB")).astype(float)
    h, w = rgb.shape[:2]
    sw, sh = meta["screen_width_m"], meta["screen_height_m"]
    lit = rgb.sum(axis=2) > 30
    luma = rgb @ np.array([.2126, .7152, .0722])
    top, bottom = (int(round(v)) for v in (to_px(0, sh / 2, w, h)[1], to_px(0, -sh / 2, w, h)[1]))
    x43, x169 = sh * 4 / 3 / 2, sh * 16 / 9 / 2
    strips = []
    for a, b in ((-x169, -x43), (x43, x169)):
        x0, x1 = int(round(to_px(a, 0, w, h)[0])), int(round(to_px(b, 0, w, h)[0]))
        strips.append((x0, x1))
    side = np.concatenate([lit[top:bottom, x0:x1].ravel() for x0, x1 in strips])
    side_luma = np.concatenate([luma[top:bottom, x0:x1].ravel() for x0, x1 in strips])
    c0, c1 = int(round(to_px(-x43, 0, w, h)[0])), int(round(to_px(x43, 0, w, h)[0]))
    centre = luma[top:bottom, c0:c1].mean()
    # the picture's own outer edges (wider than 4:3 for an open frame)
    left, right = int(round(to_px(-sw / 2, 0, w, h)[0])), int(round(to_px(sw / 2, 0, w, h)[0]))
    rows = slice(top + 4, bottom - 4)
    inner = np.concatenate([luma[rows, left + 2:left + 5].mean(axis=1), luma[rows, right - 5:right - 2].mean(axis=1)])
    outer = np.concatenate([luma[rows, left - 5:left - 2].mean(axis=1), luma[rows, right + 2:right + 5].mean(axis=1)])
    step = float(np.abs(inner - outer).mean() / max(centre, 1.0))
    return dict(side_fill=round(float(side.mean()), 4), side_level=round(float(side_luma.mean() / max(centre, 1.0)), 4),
                edge_step=round(step, 4)), (top, bottom, strips)


def main(argv):
    if len(argv) < 3:
        raise SystemExit(__doc__)
    sheet_path, folders = Path(argv[1]), [Path(a) for a in argv[2:]]
    tiles = []
    for folder in folders:
        meta = json.loads((folder / "native-proof.json").read_text())
        drawn = Image.open(folder / "left-picture.png")
        result = {"capture": folder.as_posix(), "emulated_seconds": round(meta["emulated_seconds"], 2),
                  "physical_aspect": round(meta["physical_aspect"], 4), "soft_edges": meta.get("soft_edges", False)}
        result["drawn"], box = measure(drawn, meta)
        pair = [drawn]
        if (folder / "left-plain.png").exists():
            plain = Image.open(folder / "left-plain.png")
            result["plain"], _ = measure(plain, meta)
            pair.insert(0, plain)
        print(json.dumps(result), flush=True)
        top, bottom, strips = box
        crop = (0, max(0, top - 160), drawn.width, min(drawn.height, bottom + 160))
        column = []
        for picture in pair:
            tile = picture.convert("RGB").crop(crop)
            d = ImageDraw.Draw(tile)
            for x0, x1 in strips:
                d.rectangle((x0, top - crop[1], x1 - 1, bottom - crop[1] - 1), outline=(255, 40, 40), width=2)
            column.append(tile)
        label = "%s  %.0f s" % (folder.parent.name if folder.name.startswith("t") else folder.name, meta["emulated_seconds"])
        ImageDraw.Draw(column[0]).text((8, 6), label, fill=(255, 255, 0))
        tiles.append(column)
    tw, th = tiles[0][0].size
    rows = max(len(c) for c in tiles)
    sheet = Image.new("RGB", (tw * len(tiles), th * rows), (20, 20, 20))
    for i, column in enumerate(tiles):
        for j, tile in enumerate(column):
            sheet.paste(tile, (i * tw, j * th))
    if sheet.width > 4096:
        sheet = sheet.resize((4096, int(sheet.height * 4096 / sheet.width)))
    sheet.save(sheet_path)
    print(sheet_path.as_posix())


if __name__ == "__main__":
    main(sys.argv)
