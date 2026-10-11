#!/usr/bin/env python
"""Summarize private raster metadata without assigning HUD/world roles."""
import argparse
import collections
import csv
import json
import math
from pathlib import Path


def summarize(path, start=0, stop=None, limit=32):
    groups = collections.Counter()
    projections = collections.Counter()
    frames = set()
    rows = 0
    with path.open(encoding="utf-8", newline="") as file:
        for row in csv.DictReader(file):
            frame = int(row["frame"])
            if frame < start or (stop is not None and frame >= stop):
                continue
            frames.add(frame)
            rows += 1
            kind = int(row["projection_type"])
            projections[kind] += 1
            state = tuple(float(row[f"p{i}"]) for i in range(6))
            viewport = tuple(float(row[name]) for name in (
                "viewport_wd", "viewport_ht", "viewport_zrange", "viewport_x",
                "viewport_y", "viewport_farz"))
            # Preserve exact raw values; group NaNs consistently.
            canonical = lambda values: tuple(
                v if math.isfinite(v) else "nan" if math.isnan(v)
                else "inf" if v > 0 else "-inf" for v in values)
            zmode = int(row["z_mode"])
            blend = int(row["blend_mode"])
            key = (kind, canonical(state), canonical(viewport), zmode, blend,
                   int(row["primitive"]), row["hfov_deg"], row["vfov_deg"],
                   row["vs_uid_hash"], row["ps_uid_hash"])
            groups[key] += 1
    common = []
    for key, count in groups.most_common(limit):
        kind, projection, viewport, zmode, blend, primitive, hfov, vfov, vs, ps = key
        number = lambda value: float(value) if math.isfinite(float(value)) else None
        common.append({"records": count, "projection_type": kind,
                       "raw_projection": projection, "viewport": viewport,
                       "depth_test": bool(zmode & 1), "depth_write": bool(zmode & 16),
                       "blend_enabled": bool(blend & 1), "primitive": primitive,
                       "hfov_deg": number(hfov), "vfov_deg": number(vfov),
                       "vs_uid_hash": vs, "ps_uid_hash": ps,
                       "render_role": "unknown"})
    return {"records": rows, "sampled_frames": len(frames),
            "frame_range": [min(frames), max(frames)] if frames else None,
            "projection_records": dict(sorted(projections.items())),
            "unique_state_groups": len(groups), "common_states": common,
            "limits": "Sampled submitted raster draws; no pixel/geometry evidence. "
                      "Orthographic is not proof of HUD. Frame/elapsed columns are not FPS."}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("log", type=Path)
    parser.add_argument("--start-frame", type=int, default=0)
    parser.add_argument("--stop-frame", type=int)
    parser.add_argument("--limit", type=int, default=32)
    args = parser.parse_args()
    path = args.log.resolve(strict=True)
    if ".local" not in path.parts:
        raise SystemExit("Draw logs belong under .local")
    if args.limit < 1 or args.limit > 1000:
        raise SystemExit("Limit must be between 1 and 1000")
    print(json.dumps(summarize(path, args.start_frame, args.stop_frame, args.limit),
                     indent=2, allow_nan=False))


if __name__ == "__main__":
    main()
