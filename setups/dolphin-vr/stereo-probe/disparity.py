"""Measure per-eye disparity in a side-by-side (left eye | right eye) capture.

Usage: python -I disparity.py <sbs.png> [--grid 12x9] [--out annotated.png] [--json out.json]

For each textured patch of the left eye, find the horizontal shift d (pixels, eye resolution)
that best matches the right eye: right(x - d) ~ left(x). With parallel eye cameras a point at
finite distance has d > 0 (seen further right by the left eye); d = 0 is infinity or a mono
(identical) image; d < 0 means the eyes are swapped or the content is drawn in front of
convergence at infinity, which a parallel rig cannot produce for world geometry.
"""
import argparse
import json

import numpy as np
from PIL import Image, ImageDraw

ap = argparse.ArgumentParser()
ap.add_argument("png")
ap.add_argument("--grid", default="12x9")
ap.add_argument("--maxd", type=int, default=240)
ap.add_argument("--out")
ap.add_argument("--json")
a = ap.parse_args()

img = Image.open(a.png).convert("RGB")
W, H = img.size
w = W // 2
g = np.asarray(img.convert("L"), dtype=np.float32)
L, R = g[:, :w], g[:, w:2 * w]
mono_diff = float(np.mean(np.abs(L - R)))

gx, gy = (int(v) for v in a.grid.split("x"))
ph, pw = H // gy, w // gx
rows = []
for j in range(gy):
    for i in range(gx):
        y0, x0 = j * ph, i * pw
        pl = L[y0:y0 + ph, x0:x0 + pw]
        if pl.std() < 6.0:
            continue
        best, bestd, second = None, None, None
        scores = []
        for d in range(-a.maxd, a.maxd + 1, 1):
            xs = x0 - d
            if xs < 0 or xs + pw > w:
                scores.append(np.inf)
                continue
            pr = R[y0:y0 + ph, xs:xs + pw]
            s = float(np.mean(np.abs(pl - pr)))
            scores.append(s)
        sc = np.array(scores)
        k = int(np.argmin(sc))
        bestd = k - a.maxd
        best = sc[k]
        # confidence: best vs the best score at least 6 px away
        mask = np.abs(np.arange(sc.size) - k) >= 6
        alt = float(np.min(sc[mask])) if mask.any() else np.inf
        conf = (alt - best) / (alt + 1e-6)
        rows.append({"col": i, "row": j, "x": x0 + pw // 2, "y": y0 + ph // 2, "d": bestd,
                     "sad": round(best, 2), "conf": round(float(conf), 3)})

good = [r for r in rows if r["conf"] >= 0.15 and r["sad"] < 25]
ds = np.array([r["d"] for r in good]) if good else np.array([0])
summary = {
    "file": a.png, "eye_size": [w, H], "mean_abs_LR_diff": round(mono_diff, 2),
    "patches": len(rows), "confident": len(good),
    "d_median": float(np.median(ds)), "d_min": int(ds.min()), "d_max": int(ds.max()),
    "d_p10": float(np.percentile(ds, 10)), "d_p90": float(np.percentile(ds, 90)),
    "negative_share": round(float(np.mean(ds < -2)), 3), "zero_share": round(float(np.mean(np.abs(ds) <= 1)), 3),
}
by_row = {}
for r in good:
    by_row.setdefault(r["row"], []).append(r["d"])
summary["median_d_by_row_top_to_bottom"] = {k: float(np.median(v)) for k, v in sorted(by_row.items())}
print(json.dumps(summary, indent=1))
if a.json:
    json.dump({"summary": summary, "patches": rows}, open(a.json, "w"), indent=1)
if a.out:
    left = img.crop((0, 0, w, H)).copy()
    dr = ImageDraw.Draw(left)
    for r in rows:
        ok = r in good
        col = (0, 255, 0) if ok and r["d"] > 2 else (255, 255, 0) if ok and abs(r["d"]) <= 2 else (255, 0, 0) if ok else (128, 128, 128)
        dr.rectangle([r["x"] - pw // 2, r["y"] - ph // 2, r["x"] + pw // 2, r["y"] + ph // 2], outline=col)
        dr.text((r["x"] - 14, r["y"] - 6), str(r["d"]) if ok else "?", fill=col)
    left.save(a.out)
