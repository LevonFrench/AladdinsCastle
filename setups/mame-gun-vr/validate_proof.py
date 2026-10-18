#!/usr/bin/env python3
"""Check native offline captures without opening a process or runtime.

Requires Pillow and NumPy; all input/output images stay in private storage.
"""
import argparse
import json
from pathlib import Path

import numpy as np
from PIL import Image


def require(condition, message):
    if not condition:
        raise ValueError(message)


def rgb(path):
    with Image.open(path) as image:
        return np.asarray(image.convert("RGB"))


def validate(folder):
    meta = json.loads((folder / "native-proof.json").read_text())
    require(meta["protocol"] == 4 and not meta["OpenXR_started"] and
            not meta["headset_tested"] and meta["GL_errors"] == 0,
            "expected an offline v4 proof with no GL errors")
    require(meta["trigger_pulses"] > 0 and meta["integer_prescale"] >= 1,
            "scripted shots and integer prescale required")
    source = rgb(folder / "source.png")
    source_h, source_w = source.shape[:2]
    require((source_w, source_h) == (meta["native_width"], meta["native_height"]),
            "source extent mismatch")
    result = {}
    for eye, eye_x in (("left", -.032), ("right", .032)):
        picture = rgb(folder / f"{eye}-picture.png")
        front = rgb(folder / f"{eye}-front.png")
        behind = rgb(folder / f"{eye}-behind.png")
        dot = rgb(folder / f"{eye}-dot.png")
        near = np.any(front != picture, axis=2)
        far = np.any(behind != picture, axis=2)
        marker = np.any(dot != picture, axis=2)
        require(near.sum() > 100 and far.sum() == 0 and marker.sum() > 4,
                f"{eye}: gun/dot depth mismatch")
        height, width = picture.shape[:2]
        yy, xx = np.indices((height, width))
        screen_width = 3.2
        screen_height = screen_width / meta["physical_aspect"]
        distance = 2.5
        u = .5 + (eye_x + (2 * (xx + .5) / width - 1) * distance) / screen_width
        v = .5 - (1 - 2 * (yy + .5) / height) * distance / screen_height
        roi = (u > .01) & (u < .99) & (v > .01) & (v < .99)
        sx = np.clip((u[roi] * source_w).astype(int), 0, source_w - 1)
        sy = np.clip((v[roi] * source_h).astype(int), 0, source_h - 1)
        expected = source[sy, sx].astype(float)
        actual = picture[roi].astype(float)
        upright = float(np.corrcoef(expected.ravel(), actual.ravel())[0, 1])
        flipped = float(np.corrcoef(source[source_h - 1 - sy, sx].ravel(),
                                    actual.ravel())[0, 1])
        require(upright > .97 and upright > flipped + .1,
                f"{eye}: picture mapping/orientation failed")
        cy, cx = np.argwhere(marker).mean(axis=0)
        cross_x = width * .5 * (1 + (screen_width * (meta["mame_cross_u"] - .5) - eye_x) / distance) - .5
        cross_y = height * .5 * (1 + screen_height * (meta["mame_cross_v"] - .5) / distance) - .5
        marker_error = float(np.hypot(cx - cross_x, cy - cross_y))
        require(marker_error < 3, f"{eye}: dot misses MAME crosshair")
        result[eye] = dict(near_gun_pixels=int(near.sum()), behind_gun_pixels=0,
                           upright_correlation=upright,
                           flipped_correlation=flipped,
                           dot_to_mame_crosshair_pixels=marker_error)
    tolerance = .5 / min(source_w, source_h) + .00001
    require(abs(meta["aim_u"] - meta["mame_cross_u"]) <= tolerance and
            abs(meta["aim_v"] - meta["mame_cross_v"]) <= tolerance,
            "aim exceeds half-native-pixel quantization")
    return result


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("capture_directory", type=Path)
    args = parser.parse_args()
    print(json.dumps(validate(args.capture_directory), indent=2))
