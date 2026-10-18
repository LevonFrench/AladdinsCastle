#!/usr/bin/env python3
"""Validate private no-runtime screen captures; never open an emulator or loader.

Requires Pillow and NumPy. Input has baseline/, no-gun/, and combined/ folders
from matched replays, each with live.log and run.log.json.
"""
import argparse
import json
from pathlib import Path

import numpy as np
from PIL import Image


def require(condition, message):
    if not condition:
        raise ValueError(message)


def pixels(path):
    with Image.open(path) as image:
        return np.asarray(image.convert("RGBA"))


def validate(root):
    result = {}
    reference = None
    for variant in ("baseline", "no-gun", "combined"):
        folder = root / variant
        run = json.loads((folder / "run.log.json").read_text())
        require(run["exit_code"] == 0 and not run["timed_out"],
                f"{variant}: replay did not close normally")
        require(run["hidden"] and run["priority"] == "BelowNormal",
                f"{variant}: hidden/priority gate missing")
        require(run["environment"].get("PCSX2_OPENXR") == "0",
                f"{variant}: OpenXR was not explicitly disabled")
        matched = (run["command"][-1], {
            key: value for key, value in run["environment"].items()
            if key.startswith("PCSX2_STEREO") or key == "PCSX2_LIVE_SAVE_FRAME"
        })
        if reference is None:
            reference = matched
        require(matched == reference, f"{variant}: mismatched dump/profile/frame")
        log = (folder / "live.log").read_text()
        require("OpenXR disabled: loader is not loaded" in log,
                f"{variant}: missing no-loader confirmation")
        require("Live stereo closed" in log and "GL error drained" not in log,
                f"{variant}: incomplete or erroneous GL replay")
        if variant == "combined":
            require(log.count("draw=1 errors=0") == 8,
                    "combined: expected four successful passes per eye")
    for eye in ("left", "right"):
        for form in ("shared", "blit"):
            filename = f"live-{form}-{eye}.png"
            require(np.array_equal(pixels(root / "baseline" / filename),
                                   pixels(root / "no-gun" / filename)),
                    f"{eye}/{form}: no-gun output changed")
            require(np.array_equal(pixels(root / "no-gun" / filename),
                                   pixels(root / "combined" / filename)),
                    f"{eye}/{form}: enabling gun changed the source picture")
        folder = root / "combined"
        picture = pixels(folder / f"screen-{eye}-picture.png")
        front = pixels(folder / f"screen-{eye}-front.png")
        behind = pixels(folder / f"screen-{eye}-behind.png")
        near = np.any(front != picture, axis=2)
        far = np.any(behind != picture, axis=2)
        require(int(near.sum()) > 100, f"{eye}: missing near gun")
        require(int(far.sum()) == 0, f"{eye}: gun leaked through screen")
        require(np.all(picture[:, :, 3][near] == 255),
                f"{eye}: near gun did not cover opaque picture pixels")
        dot = pixels(folder / f"screen-{eye}-dot.png")
        red = ((dot[:, :, 0] > 240) & (dot[:, :, 1] < 60) &
               (dot[:, :, 2] < 30) & np.any(dot != front, axis=2))
        require(int(red.sum()) > 4, f"{eye}: missing aim dot")
        result[eye] = dict(no_gun_changed_pixels=0,
                           no_gun_quad_blit_changed_pixels=0,
                           gun_in_front_pixels=int(near.sum()),
                           gun_behind_screen_pixels=int(far.sum()),
                           dot_pixels=int(red.sum()))
    order = json.loads((root / "combined" / "screen-screen-order.json").read_text())
    require(order["mode"] == "screen" and not order["submitted_to_runtime"],
            "expected offline screen-mode composition")
    layers = order["order_back_to_front"]
    require(len(layers) == 1 and layers[0]["type"] == 35 and layers[0]["flags"] == 0,
            "expected one opaque projection layer with both eye views")
    return result


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("capture_directory", type=Path)
    args = parser.parse_args()
    print(json.dumps(validate(args.capture_directory), indent=2))
