"""Bounded offline CSV validation and candidate identity inventory; no role/rule inference."""
from __future__ import annotations
import argparse
from collections import Counter, defaultdict
import csv
import hashlib
import io
import json
import math
from pathlib import Path

from prepare_draw_copy import private_output

FLOATS = [*(f"p{i}" for i in range(6)), "last_present_emulated_s",
          "vp_wd", "vp_ht", "vp_zrange", "vp_xorig", "vp_yorig", "vp_farz",
          *(f"gs_stereo{i}" for i in range(4)), *(f"gs_screen{i}" for i in range(4))]
DRAW_COLS = "record_id xfb_boundary last_present_ticks ticks_per_second last_present_emulated_s host_elapsed_ns draw_order index_count primitive projection_type p0 p1 p2 p3 p4 p5 vp_wd vp_ht vp_zrange vp_xorig vp_yorig vp_farz scissor_tl scissor_br scissor_offset zmode blendmode alpha_test genmode lookup_vs_crc lookup_ps_crc lookup_gs_crc submitted_base_vs_crc submitted_base_ps_crc submitted_base_gs_crc uber custom_ps_requested custom_pipeline_selected pink_ps_selected cached_vs_family cached_ps_family cached_gs_family used_texture_mask gs_stereo0 gs_stereo1 gs_stereo2 gs_stereo3 gs_screen0 gs_screen1 gs_screen2 gs_screen3 role".split()
TEX_COLS = "record_id stage snapshot_valid entry_id native_width native_height width height layers is_efb_copy is_xfb_copy raw_cache_hash bound_hash_at_load bound_hash_at_submit hash_changed metadata_phase".split()
HASHES = ["lookup_vs_crc", "lookup_ps_crc", "lookup_gs_crc", "submitted_base_vs_crc", "submitted_base_ps_crc", "submitted_base_gs_crc", "cached_vs_family", "cached_ps_family", "cached_gs_family"]
TEX_ID = TEX_COLS[2:3] + TEX_COLS[4:]
MAX_CSV_BYTES = 128 * 1024 * 1024


def read_snapshot(path: Path) -> bytes:
    if path.stat().st_size > MAX_CSV_BYTES:
        raise ValueError(f"oversize CSV: {path}")
    # Bound the actual read too: a producer may append after stat(). Hash and
    # parse these exact bytes, never a later reopening of the producer's file.
    with path.open("rb") as stream:
        data = stream.read(MAX_CSV_BYTES + 1)
    if len(data) > MAX_CSV_BYTES:
        raise ValueError(f"oversize CSV: {path}")
    return data


def rows(path: Path, columns: list[str], cap: int, *, snapshot: bytes | None = None):
    data = read_snapshot(path) if snapshot is None else snapshot
    if len(data) > MAX_CSV_BYTES:
        raise ValueError(f"oversize CSV: {path}")
    csv.field_size_limit(4096)
    with io.StringIO(data.decode("utf-8"), newline="") as stream:
        reader = csv.DictReader(stream)
        if reader.fieldnames != columns:
            raise ValueError(f"unexpected schema: {path}")
        for number, row in enumerate(reader, 1):
            if number > cap or None in row or any(v is None for v in row.values()):
                raise ValueError(f"row cap/shape failure: {path}:{number+1}")
            result = {}
            for key, value in row.items():
                if key in ("role", "metadata_phase"):
                    result[key] = value
                elif key in FLOATS:
                    result[key] = float(value)
                    if not math.isfinite(result[key]):
                        raise ValueError(f"nonfinite {key}: {path}:{number+1}")
                else:
                    result[key] = int(value, 10)
                    if result[key] < 0:
                        raise ValueError(f"negative integer {key}: {path}:{number+1}")
            yield result


def dual(value: int, width: int = 16) -> dict:
    return {"decimal": value, "hex": f"0x{value:0{width}x}"}


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--input", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--start", type=float, required=True)
    parser.add_argument("--duration", type=float, required=True)
    parser.add_argument("--marker", type=Path, required=True, help="caller's headset safety marker")
    parser.add_argument("--max-candidates", type=int, default=128)
    parser.add_argument("--capture-at-window-start", action="store_true")
    args = parser.parse_args()
    if args.marker.exists():
        raise SystemExit("HEADSET-ON: no CSV workload")
    if not math.isfinite(args.start) or args.start < 0 or not 0 < args.duration <= 5 or not 1 <= args.max_candidates <= 1024:
        raise ValueError("invalid bounded window/candidate count")
    if args.output.exists():
        raise FileExistsError("preserve existing results")
    if not private_output(args.output.resolve()):
        raise ValueError("output must be in a private .local directory")
    draw_path, tex_path = args.input / "draws.csv", args.input / "textures.csv"
    snapshots = {draw_path: read_snapshot(draw_path), tex_path: read_snapshot(tex_path)}
    draws = list(rows(draw_path, DRAW_COLS, 20000, snapshot=snapshots[draw_path]))
    if not draws:
        raise ValueError("no submitted rows")
    errors = []
    def check(ok: bool, message: str):
        if not ok and len(errors) < 100:
            errors.append(message)
    previous_host = previous_ticks = -1
    previous_boundary = previous_order = -1
    for expected, row in enumerate(draws):
        rid = row["record_id"]
        check(rid == expected, f"noncontiguous record ID {rid}, expected {expected}")
        check(row["ticks_per_second"] > 0, f"zero clock rate {rid}")
        check(args.start <= row["last_present_emulated_s"] < args.start + args.duration, f"outside emulated window {rid}")
        check(row["host_elapsed_ns"] >= previous_host and row["host_elapsed_ns"] < 60_000_000_000, f"host order/cap {rid}")
        check(row["last_present_ticks"] >= previous_ticks, f"emulated tick reversal {rid}")
        check(row["xfb_boundary"] >= previous_boundary, f"boundary reversal {rid}")
        if row["xfb_boundary"] == previous_boundary:
            check(row["draw_order"] > previous_order, f"nonincreasing state draw order {rid}")
        check(row["used_texture_mask"] <= 255 and row["role"] == "unknown", f"mask/role failure {rid}")
        if row["ticks_per_second"]:
            check(abs(row["last_present_ticks"] / row["ticks_per_second"] - row["last_present_emulated_s"]) <= 1e-8, f"tick/seconds mismatch {rid}")
        for flag in ("uber", "custom_ps_requested", "custom_pipeline_selected", "pink_ps_selected"):
            check(row[flag] in (0, 1), f"invalid flag {flag} {rid}")
        previous_host, previous_ticks = row["host_elapsed_ns"], row["last_present_ticks"]
        previous_boundary, previous_order = row["xfb_boundary"], row["draw_order"]
    textures = defaultdict(list)
    seen = set()
    texture_flags = Counter()
    tex_count = 0
    for tex in rows(tex_path, TEX_COLS, 160000, snapshot=snapshots[tex_path]):
        tex_count += 1
        rid, stage = tex["record_id"], tex["stage"]
        check((rid, stage) not in seen, f"duplicate texture key {rid}/{stage}")
        seen.add((rid, stage))
        check(0 <= rid < len(draws) and 0 <= stage < 8, f"unjoined texture {rid}/{stage}")
        check(tex["metadata_phase"] == "afterLoad_beforeOverrides", f"unknown texture phase {rid}/{stage}")
        for flag in ("snapshot_valid", "is_efb_copy", "is_xfb_copy"):
            check(tex[flag] in (0, 1), f"invalid texture flag {rid}/{stage}/{flag}")
        expected_change = int(tex["bound_hash_at_load"] != tex["bound_hash_at_submit"]) if tex["snapshot_valid"] else 2
        check(tex["hash_changed"] == expected_change, f"changed-hash flag mismatch {rid}/{stage}")
        if tex["snapshot_valid"]:
            check(all(tex[k] > 0 for k in ("width", "height", "layers")), f"invalid positive snapshot shape {rid}/{stage}")
        if 0 <= rid < len(draws) and stage < 8:
            check(bool(draws[rid]["used_texture_mask"] & (1 << stage)), f"texture not in used mask {rid}/{stage}")
            textures[rid].append(tex)
        texture_flags[(tex["snapshot_valid"], tex["is_efb_copy"], tex["is_xfb_copy"], tex["layers"], tex["hash_changed"])] += 1
    groups = {}
    key_fields = [*HASHES, "projection_type", *(f"p{i}" for i in range(6)), "vp_wd", "vp_ht", "vp_zrange", "vp_xorig", "vp_yorig", "vp_farz", "scissor_tl", "scissor_br", "scissor_offset", "zmode", "blendmode", "alpha_test", "genmode", "primitive", "uber", "custom_ps_requested", "custom_pipeline_selected", "pink_ps_selected", "used_texture_mask", *(f"gs_stereo{i}" for i in range(4)), *(f"gs_screen{i}" for i in range(4))]
    for row in draws:
        rid = row["record_id"]
        texs = sorted(textures[rid], key=lambda t: t["stage"])
        check(len(texs) == row["used_texture_mask"].bit_count(), f"missing texture stages {rid}")
        key = (tuple(row[k] for k in key_fields), tuple((t["stage"], *(t[k] for k in TEX_ID)) for t in texs))
        if key not in groups:
            groups[key] = {"count": 0, "first_record_id": rid, "last_record_id": rid, "first_emulated_s": row["last_present_emulated_s"], "last_emulated_s": row["last_present_emulated_s"], "state": {k: row[k] for k in key_fields}, "textures": [{k: t[k] for k in ("stage", *TEX_ID)} for t in texs]}
        group = groups[key]
        group["count"] += 1
        group["last_record_id"] = rid
        group["last_emulated_s"] = row["last_present_emulated_s"]
    candidates = sorted(groups.values(), key=lambda g: (-g["count"], g["first_record_id"]))[:args.max_candidates]
    for index, group in enumerate(candidates):
        group["candidate_id"] = f"draw-{index:04d}"
        for key in HASHES:
            group["state"][key] = dual(group["state"][key], 8 if key.endswith("crc") else 16)
        for tex in group["textures"]:
            for key in ("raw_cache_hash", "bound_hash_at_load", "bound_hash_at_submit"):
                tex[key] = dual(tex[key])
    result = {
        "schema": "ac-draw-inventory-v1", "valid": not errors, "errors": errors,
        "analyzer_sha256": hashlib.sha256(Path(__file__).read_bytes()).hexdigest(),
        "input_read_mode": "bounded byte snapshots; hashes describe parsed bytes, not later file contents",
        "input_sha256": {str(p.resolve()): hashlib.sha256(snapshots[p]).hexdigest() for p in (draw_path, tex_path)},
        "window": {"requested_start": args.start, "requested_duration": args.duration, "observed_first_emulated_s": draws[0]["last_present_emulated_s"], "observed_last_emulated_s": draws[-1]["last_present_emulated_s"], "observed_host_span_s": (draws[-1]["host_elapsed_ns"]-draws[0]["host_elapsed_ns"])/1e9},
        "submitted_draw_rows": len(draws), "texture_rows": tex_count, "boundaries": len({r["xfb_boundary"] for r in draws}), "row_cap_reached": len(draws) == 20000,
        "gs_stereo3_counts": dict(sorted(Counter(str(r["gs_stereo3"]) for r in draws).items())),
        "projection_type_counts": dict(Counter(str(r["projection_type"]) for r in draws)),
        "unavailable_family_rows": sum(any(r[k] == 0 for k in HASHES[6:]) for r in draws),
        "replacement_flag_counts": {k: sum(r[k] for r in draws) for k in ("uber", "custom_ps_requested", "custom_pipeline_selected", "pink_ps_selected")},
        "texture_flag_counts": [{"snapshot_valid": k[0], "is_efb_copy": k[1], "is_xfb_copy": k[2], "layers": k[3], "hash_changed": k[4], "rows": n} for k,n in sorted(texture_flags.items())],
        "identity_group_count": len(groups), "candidate_limit": args.max_candidates, "omitted_identity_groups": max(0, len(groups)-len(candidates)), "candidates": candidates,
        "roles_inferred": False, "rules_generated": False, "actual_compositor_delivery_measured": False,
        "caveats": ["CPU primary draw submissions, not GPU completion or changing-content FPS", "time is cached last-present event, not exact per-draw emulated time", "viewport/scissor are raw GX state, not final host raster viewport", "texture dimensions/layers are afterLoad_beforeOverrides snapshots; equal hash does not prove unchanged final layers", "candidate identity is not evidence of HUD/menu/world role", "CSV formatting/flush overhead is unmeasured; no performance claim"] + (["Program screenshot scheduled at metadata window start; observation may be perturbed"] if args.capture_at_window_start else []),
    }
    if args.marker.exists():
        raise SystemExit("HEADSET-ON: stop before result write")
    args.output.mkdir(parents=True)
    with (args.output / "inventory.json").open("x", encoding="utf-8", newline="\n") as stream:
        stream.write(json.dumps(result, indent=2, allow_nan=False)+"\n")
    md = ["# Draw metadata inventory", "", f"Validation: **{'PASS' if result['valid'] else 'FAIL'}**; {len(draws)} submitted draw rows, {tex_count} texture rows, {result['boundaries']} XFB boundaries.", f"Identity groups: {len(groups)}; emitted {len(candidates)}; omitted {result['omitted_identity_groups']}.", f"GS stereo.w counts: `{result['gs_stereo3_counts']}`.", "", "No semantic roles or rules inferred. No timing/GPU/compositor claim.", "", *[f"- {c}" for c in result['caveats']], "", "| Candidate | Rows | Lookup VS / PS / GS | Projection type | Raw viewport wd,ht,x,y | GS stereo.w |", "| --- | ---: | --- | ---: | --- | ---: |"]
    for group in candidates:
        state = group["state"]
        md.append(f"| {group['candidate_id']} | {group['count']} | {' / '.join(state[k]['hex'] for k in HASHES[:3])} | {state['projection_type']} | {state['vp_wd']},{state['vp_ht']},{state['vp_xorig']},{state['vp_yorig']} | {state['gs_stereo3']} |")
    if errors:
        md += ["", "Validation errors:", *[f"- {e}" for e in errors]]
    with (args.output / "INVENTORY.md").open("x", encoding="utf-8", newline="\n") as stream:
        stream.write("\n".join(md)+"\n")
    print(json.dumps({k: result[k] for k in ("valid", "errors", "submitted_draw_rows", "texture_rows", "boundaries", "identity_group_count", "gs_stereo3_counts", "texture_flag_counts")}))
    print(args.output.resolve())
    raise SystemExit(0 if result["valid"] else 1)


if __name__ == "__main__":
    main()
