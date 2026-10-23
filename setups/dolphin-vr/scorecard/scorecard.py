"""Private Wii W0 evidence aggregation. Reads JSON only; never launches a game."""
import argparse
import copy
from datetime import datetime, timezone
import hashlib
import json
import math
from pathlib import Path, PurePosixPath, PureWindowsPath
import re
import sys


VERSION = 1
FRAME_FIELDS = dict.fromkeys((
    "target_hz", "wall_duration_s", "emulated_duration_s", "present_count",
    "eligible_stereo_frame_count", "layers_min", "layers_max", "expected_wall_frames", "fresh_content_frames",
    "repeated_content_frames", "elapsed_wall_ticks", "tick_frequency_hz",
    ))
METRICS = {
    "stereo": dict.fromkeys(("distinct_eyes", "near_px", "far_px", "eye_width_px",
                             "near_px_1920", "far_px_1920", "method")),
    "placement_2d": dict.fromkeys(("classification", "disparity_px_1920", "method")),
    "scale": dict.fromkeys(("known_object", "assumed_size_m", "assumed_size_uncertainty_m",
                            "measured_game_units", "units_per_metre", "uncertainty", "method")),
    "frames": {name: dict(FRAME_FIELDS) for name in ("native", "72", "90", "real_compositor")},
    "image": dict.fromkeys(("aliasing", "artefacts", "method")),
    "controls": dict.fromkeys(("bindings", "aim_error_percent", "aim_points", "method")),
    "launch": dict.fromkeys(("print_only", "exit_code", "command", "output_path")),
}


def utc_now():
    return datetime.now(timezone.utc).isoformat()


def absolute(path):
    return isinstance(path, str) and (PurePosixPath(path).is_absolute() or PureWindowsPath(path).is_absolute())


def private_output(path):
    path = Path(path).resolve()
    parts = path.parts
    # A managed checkout can itself sit beneath a parent's .local. Only .local
    # inside the closest checkout protects against committing generated data.
    for ancestor in (path, *path.parents):
        if (ancestor / ".git").exists():
            parts = path.relative_to(ancestor).parts
            break
    if ".local" not in [p.lower() for p in parts]:
        raise ValueError("output must be inside a private .local directory")
    return path


def read_json(path):
    # Reject nonstandard JSON NaN/Infinity, including inside extension fields.
    def invalid(value):
        raise ValueError("non-finite JSON number: " + value)
    return json.loads(Path(path).read_text(encoding="utf-8-sig"), parse_constant=invalid)


def digest(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest()


def initialize(manifest):
    manifest = Path(manifest).resolve()
    source = read_json(manifest)
    if not isinstance(source, dict) or not source:
        raise ValueError("manifest must be a nonempty object keyed by game")
    games = {}
    for key, game in source.items():
        if not isinstance(game, dict) or not isinstance(game.get("views"), dict) or not game["views"]:
            raise ValueError("game needs nonempty views: " + key)
        views = {}
        historical = {"status": game.get("status"), "controls": game.get("controls"), "view_notes": {}}
        for name, view in game["views"].items():
            if not isinstance(view, dict):
                raise ValueError("invalid view: " + key + "/" + name)
            baseline = {k: copy.deepcopy(v) for k, v in view.items() if k != "notes"}
            ini = view.get("gameIni")
            stem = PureWindowsPath(ini["path"]).stem if isinstance(ini, dict) and ini.get("path") else ""
            views[name] = {"baseline": baseline,
                           "game_id_from_ini_filename": stem if re.fullmatch(r"[A-Z0-9]{4,6}", stem) else None,
                           "metrics": copy.deepcopy(METRICS), "runs": []}
            historical["view_notes"][name] = copy.deepcopy(view.get("notes", []))
        games[key] = {"title": game.get("title", key), "evidence_level": "E0",
                      "historical_claims_unverified": historical, "views": views}
    return {"schema_version": VERSION, "created_at": utc_now(), "updated_at": utc_now(),
            "source": {"manifest_path": str(manifest), "sha256": digest(manifest), "evidence_level": "E0"},
            "acceptance": "Unknown. E2 offline evidence does not establish E3 owner acceptance.",
            "games": games}


def merge_metrics(supplied):
    if not isinstance(supplied, dict):
        raise ValueError("metrics must be an object")
    unknown = set(supplied) - set(METRICS)
    if unknown:
        raise ValueError("unknown metric groups: " + ", ".join(sorted(unknown)))
    metrics = copy.deepcopy(METRICS)
    for group, fields in supplied.items():
        if not isinstance(fields, dict):
            raise ValueError("metric group must be an object: " + group)
        if group == "frames":
            if set(fields) - set(metrics[group]):
                raise ValueError("frames channels must be native, 72, 90, real_compositor")
            for channel, values in fields.items():
                if not isinstance(values, dict):
                    raise ValueError("frame channel must be an object")
                if "stereo_layer_count" in values or "stereo_layers_eligible" in values:
                    raise ValueError("ambiguous stereo fields: use layers_min/layers_max and eligible_stereo_frame_count")
                metrics[group][channel].update(values)
        else:
            metrics[group].update(fields)
    return metrics


def number(value, name, positive=False):
    if value is None:
        return None
    if isinstance(value, bool) or not isinstance(value, (int, float)) or not math.isfinite(value):
        raise ValueError(name + " must be a finite number or null")
    if positive and value <= 0:
        raise ValueError(name + " must be positive")
    return value


def normalize(metrics, evidence_kind):
    stereo = metrics["stereo"]
    width = number(stereo.get("eye_width_px"), "eye_width_px", positive=True)
    for name in ("near", "far"):
        px = number(stereo.get(name + "_px"), name + "_px")
        normalized = number(stereo.get(name + "_px_1920"), name + "_px_1920")
        if px is not None and width is not None:
            derived = px * 1920 / width
            if normalized is not None and not math.isclose(normalized, derived, rel_tol=1e-8, abs_tol=1e-8):
                raise ValueError("inconsistent normalized disparity: " + name)
            stereo[name + "_px_1920"] = derived
    if stereo.get("distinct_eyes") is not None and not isinstance(stereo["distinct_eyes"], bool):
        raise ValueError("distinct_eyes must be boolean or null")
    placement = metrics["placement_2d"].get("classification")
    if placement not in (None, "face", "room_screen", "hidden", "doubled", "mixed", "unknown"):
        raise ValueError("unsupported placement classification")
    # These are hypotheses about real object sizes, not measured physical scale acceptance.
    scale = metrics["scale"]
    for name in ("assumed_size_m", "measured_game_units", "units_per_metre"):
        number(scale.get(name), name, positive=True)
    uncertainty = number(scale.get("assumed_size_uncertainty_m"), "assumed_size_uncertainty_m")
    if uncertainty is not None and uncertainty < 0:
        raise ValueError("size uncertainty cannot be negative")
    for channel, values in metrics["frames"].items():
        for name in FRAME_FIELDS:
            value = number(values.get(name), channel + "." + name)
            if value is not None and value < 0:
                raise ValueError("negative frame metric: " + name)
        if channel in ("72", "90"):
            if values["target_hz"] not in (None, int(channel)):
                raise ValueError("frame channel and target_hz disagree")
            # A rate label alone records no run or expectation.
            if any(v is not None for v in values.values()):
                values["target_hz"] = int(channel)
        rate, duration = values["target_hz"], values["wall_duration_s"]
        if rate is not None and rate <= 0:
            raise ValueError("target_hz must be positive")
        if duration is not None and rate is not None:
            expected = duration * rate
            if values["expected_wall_frames"] is not None and not math.isclose(values["expected_wall_frames"], expected):
                raise ValueError("expected_wall_frames disagrees with duration and rate")
            values["expected_wall_frames"] = expected
        low, high = values["layers_min"], values["layers_max"]
        if low is not None and high is not None and low > high:
            raise ValueError("layers_min exceeds layers_max")
        if channel == "real_compositor" and evidence_kind != "owner" and any(v is not None for v in values.values()):
            raise ValueError("offline/source receipts must leave real_compositor unknown")
    launch = metrics["launch"]
    if launch.get("print_only") not in (None, True):
        raise ValueError("launch evidence must be print-only")
    if launch.get("output_path") is not None and not absolute(launch["output_path"]):
        raise ValueError("launch output_path must be absolute")
    return metrics


def import_receipt(card, path):
    path = Path(path).resolve()
    receipt = read_json(path)
    if not isinstance(receipt, dict) or receipt.get("schema_version", VERSION) != VERSION:
        raise ValueError("unsupported receipt schema")
    key, view = receipt.get("game_key"), receipt.get("view")
    if key not in card["games"] or view not in card["games"][key]["views"]:
        raise ValueError("receipt game_key/view is absent from baseline")
    if receipt.get("phase", "baseline") not in ("baseline", "before", "after"):
        raise ValueError("phase must be baseline, before or after")
    if receipt.get("stage") not in (None, "title", "menu", "gameplay"):
        raise ValueError("stage must be title, menu or gameplay")
    level, kind = receipt.get("evidence_level"), receipt.get("evidence_kind")
    if (level, kind) not in (("E0", "source"), ("E2", "offline"), ("E3", "owner")):
        raise ValueError("evidence pair must be E0/source, E2/offline or E3/owner")
    if level == "E3":
        confirmation = receipt.get("owner_confirmation", {})
        if not isinstance(confirmation, dict) or not all(isinstance(confirmation.get(k), str) and confirmation[k].strip()
                                                        for k in ("by", "at", "statement")):
            raise ValueError("E3 requires explicit owner_confirmation: by, at, statement")
    evidence = receipt.get("evidence_paths")
    if not isinstance(evidence, list) or not evidence or not all(absolute(p) for p in evidence):
        raise ValueError("receipt needs nonempty absolute evidence_paths")
    metrics = normalize(merge_metrics(receipt.get("metrics", {})), kind)
    if kind == "source" and any(metrics[g] != METRICS[g] for g in ("stereo", "placement_2d", "scale", "frames", "image", "controls")):
        raise ValueError("E0/source may record launch-plan evidence only; measured groups need E2/offline")
    run = copy.deepcopy(receipt)
    run.update({"schema_version": VERSION, "phase": receipt.get("phase", "baseline"),
                "receipt_path": str(path), "receipt_sha256": digest(path), "metrics": metrics})
    runs = card["games"][key]["views"][view]["runs"]
    if any(r["receipt_sha256"] == run["receipt_sha256"] for r in runs):
        return False
    supersession_target(run, runs)
    runs.append(run)
    # No merged 'latest metrics': before/after and title/menu/gameplay remain independent.
    card["updated_at"] = utc_now()
    return True


def display(value):
    if value is None:
        return "unknown"
    if isinstance(value, bool):
        return "yes" if value else "no"
    if isinstance(value, (dict, list)):
        return json.dumps(value, ensure_ascii=False, allow_nan=False)
    return str(value)


def escape(value):
    return display(value).replace("|", "\\|").replace("\r", " ").replace("\n", " ")


def receipt_identity(path, sha256):
    # Compare recorded identity only; do not reread/reinterpret old evidence.
    windows = PureWindowsPath(path)
    normalized = windows.as_posix().casefold() if windows.is_absolute() else PurePosixPath(path).as_posix()
    return normalized, sha256.lower()


def supersession_reference(run):
    path_key, hash_key = "supersedes_receipt_path", "supersedes_receipt_sha256"
    references = []
    if path_key in run or hash_key in run:
        references.append((run.get(path_key), run.get(hash_key)))
    if "supersedes" in run:
        nested = run["supersedes"]
        if not isinstance(nested, dict):
            raise ValueError("supersession must be an object")
        references.append((nested.get("receipt_path"), nested.get("receipt_sha256")))
    if not references:
        return None
    for path, sha256 in references:
        if not absolute(path) or not isinstance(sha256, str) or not re.fullmatch(r"[0-9a-fA-F]{64}", sha256):
            raise ValueError("supersession requires an absolute receipt path and SHA-256")
    if any(receipt_identity(*item) != receipt_identity(*references[0]) for item in references[1:]):
        raise ValueError("conflicting supersession declarations")
    return references[0]


def supersession_target(run, previous):
    reference = supersession_reference(run)
    if reference is None:
        return None
    path, sha256 = reference
    identity = receipt_identity(path, sha256)
    for old in previous:
        if receipt_identity(old["receipt_path"], old["receipt_sha256"]) != identity:
            continue
        if all(old.get(key, "baseline" if key == "phase" else None) == run.get(key, "baseline" if key == "phase" else None)
               for key in ("game_key", "view", "phase")):
            return old
    raise ValueError("supersession target must be an earlier receipt in the same game/view/phase with matching path and SHA-256")


def superseded_receipts(runs):
    superseded = {}
    for index, run in enumerate(runs):
        old = supersession_target(run, runs[:index])
        if old is not None:
            superseded[receipt_identity(old["receipt_path"], old["receipt_sha256"])] = run
    return superseded


def scoped_runs(game):
    for view_name, view in game["views"].items():
        superseded = superseded_receipts(view["runs"])
        for index, run in enumerate(view["runs"], 1):
            if receipt_identity(run["receipt_path"], run["receipt_sha256"]) in superseded:
                continue
            scope = "/".join((view_name, run["phase"], run.get("stage") or "unassigned"))
            yield scope + " r" + str(index) + " " + run["evidence_level"], run


def overview_cell(game, group):
    """Summarize reported fields without combining different run scopes."""
    entries = []
    fields = {
        "stereo": (("distinct_eyes", "eyes differ"), ("near_px_1920", "near@1920"),
                   ("far_px_1920", "far@1920"), ("near_px", "raw near"), ("far_px", "raw far")),
        "placement_2d": (("classification", "placement"), ("disparity_px_1920", "px@1920")),
        "scale": (("known_object", "object"), ("units_per_metre", "units/m"),
                  ("assumed_size_uncertainty_m", "object +/-m"), ("uncertainty", "uncertainty")),
        "image": (("aliasing", "aliasing"), ("artefacts", "artefacts")),
        "controls": (("aim_error_percent", "aim error %"), ("aim_points", "aim points")),
        "launch": (("print_only", "print-only"), ("exit_code", "exit"),
                   ("program_exists", "program exists"), ("disc_exists", "disc exists")),
    }
    for scope, run in scoped_runs(game):
        values = run["metrics"].get(group, {})
        parts = []
        if group == "frames":
            for channel, frame in values.items():
                if not any(value is not None for value in frame.values()):
                    continue
                observed = []
                for key, label in (("target_hz", "target Hz"), ("wall_duration_s", "wall s"),
                                   ("emulated_duration_s", "emu s"), ("present_count", "source events"),
                                   ("expected_wall_frames", "expected wall frames"),
                                   ("eligible_stereo_frame_count", "eligible stereo events"),
                                   ("layers_min", "layers min"), ("layers_max", "layers max"),
                                   ("fresh_stereo_xfb_frames", "fresh stereo XFB"),
                                   ("fresh_mono_or_blank_xfb_frames", "fresh mono/blank XFB"),
                                   ("fifo_unknown_opcode_mentions_entire_run", "FIFO warnings entire run"),
                                   ("capture_policy", "capture policy")):
                    if frame.get(key) is not None:
                        observed.append(label + "=" + display(frame[key]))
                if observed:
                    parts.append(channel + ": " + ", ".join(observed))
        else:
            for key, label in fields[group]:
                if values.get(key) is not None:
                    # Prefer normalized disparity when it exists; retain raw
                    # measurements in the detailed receipt below.
                    if group == "stereo" and key in ("near_px", "far_px") and values.get(key + "_1920") is not None:
                        continue
                    parts.append(label + "=" + display(values[key]))
            if group == "controls" and values.get("bindings"):
                parts.append("binding observations recorded")
        if parts:
            entries.append(escape(scope + ": " + "; ".join(parts)))
    return "<br>".join(entries) if entries else "unknown"


def overview_stages(game):
    stages = [escape(scope) for scope, run in scoped_runs(game)
              if run["evidence_level"] in ("E2", "E3")]
    return "<br>".join(stages) if stages else "unknown"


def markdown(card):
    lines = ["# Wii W0 scorecard", "", "Generated: " + card["updated_at"], "",
             "Source baseline E0: `" + card["source"]["manifest_path"] + "`",
             "Manifest SHA-256: `" + card["source"]["sha256"] + "`", "",
             "Unknown means unmeasured. No pass is inferred. E0 = source/launch plan; E2 = offline evidence; E3 requires an explicit owner's receipt.",
             "Offline present counts and eligible stereo frame counts do not measure real compositor delivery or headset comfort.",
             "Near/far disparity is pixels normalized to 1920 pixels per eye; scale is game units per metre with object-size assumptions.",
             "Overview scopes are view/phase/stage and per-view receipt ordinal. Unassigned means the stage was not verified. No values are merged across receipts.",
             "Frame source-event counts and expected wall frames are separate observations; source availability is not actual delivery. Exact timing scopes and receipt paths follow below.", "",
             "| Game | Views | Current evidence | Stages / scoped receipts | Stereo | 2D | Scale | Frames | Image | Controls | Launch |",
             "|---|---|---|---|---|---|---|---|---|---|---|"]
    for game in card["games"].values():
        runs = [run for _, run in scoped_runs(game)]
        levels = sorted(set(r["evidence_level"] for r in runs))
        evidence = "E0 baseline" + ("; receipts " + ", ".join(levels) + " (scope below)" if levels else "")
        cells = [escape(game["title"]), escape(", ".join(game["views"])), escape(evidence), overview_stages(game)]
        cells += [overview_cell(game, group) for group in ("stereo", "placement_2d", "scale", "frames", "image", "controls", "launch")]
        lines.append("| " + " | ".join(cells) + " |")
    lines += ["", "Overview values report supplied evidence only. Missing fields remain unknown in each detailed receipt; no game-wide pass or owner acceptance is inferred."]
    for key, game in card["games"].items():
        lines += ["", "## " + escape(game["title"]) + " (`" + key + "`)", "",
                  "E0 identity: launcher key and title. IDs inferred from INI filenames are unverified against disc headers."]
        for name, view in game["views"].items():
            superseded = superseded_receipts(view["runs"])
            lines += ["", "### View `" + name + "`", "", "E0 config baseline (copied from manifest; referenced files not read):", "", "```json",
                      json.dumps({"game_id_from_ini_filename": view["game_id_from_ini_filename"], **view["baseline"]}, indent=2, ensure_ascii=False), "```"]
            if not view["runs"]:
                lines += ["", "Missing: stereo, 2D placement, scale/object uncertainty, native/72/90 frame timing, real compositor delivery, image quality, controls/aim, print-only launch result.",
                          "Title/menu/gameplay captures: unknown. Before/after comparison: unknown. E3 owner acceptance: unknown."]
            for run in view["runs"]:
                lines += ["", "#### " + run["phase"] + " / " + str(run.get("stage") or "stage unknown") + " / " + run["evidence_level"] + " " + run["evidence_kind"], "",
                          "Receipt: `" + run["receipt_path"] + "` (SHA-256 `" + run["receipt_sha256"] + "`)", "",
                          "Evidence paths (reported provenance; existence/content not verified by this importer):"]
                lines += ["- `" + p + "`" for p in run["evidence_paths"]]
                replacement = superseded.get(receipt_identity(run["receipt_path"], run["receipt_sha256"]))
                if replacement is not None:
                    lines += ["", "Superseded by `" + replacement["receipt_path"] + "` (SHA-256 `" +
                              replacement["receipt_sha256"] + "`); retained as history and excluded from the current overview."]
                reference = supersession_reference(run)
                if reference is not None:
                    lines += ["", "Supersedes `" + reference[0] + "` (SHA-256 `" +
                              reference[1] + "`) within this game/view/phase."]
                if run.get("owner_confirmation"):
                    lines += ["", "Owner confirmation: " + escape(run["owner_confirmation"])]
                if run.get("notes"):
                    lines += ["", "Notes: " + escape(run["notes"])]
                for group, fields in run["metrics"].items():
                    lines += ["", "**" + group + "**"]
                    lines += ["- " + field + ": " + escape(value) for field, value in fields.items()]
        lines += ["", "### Historical claims (unverified; not current W0 measurements)", "", "```json",
                  json.dumps(game["historical_claims_unverified"], indent=2, ensure_ascii=False), "```"]
    return "\n".join(lines) + "\n"


def save(card, out):
    out.mkdir(parents=True, exist_ok=True)
    # Validate full serialization before touching either output.
    data = json.dumps(card, indent=2, ensure_ascii=False, allow_nan=False) + "\n"
    report = markdown(card)
    (out / "scorecard.json").write_text(data, encoding="utf-8")
    (out / "SCORECARD.md").write_text(report, encoding="utf-8")


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("action", choices=("init", "import", "render"))
    parser.add_argument("--manifest", type=Path)
    parser.add_argument("--out", type=Path, required=True)
    parser.add_argument("--receipt", type=Path, action="append", default=[])
    args = parser.parse_args(argv)
    try:
        out = private_output(args.out)
        if args.action == "init":
            if not args.manifest:
                raise ValueError("init requires --manifest")
            if (out / "scorecard.json").exists() or (out / "SCORECARD.md").exists():
                raise ValueError("init refuses to overwrite existing scorecard outputs")
            card = initialize(args.manifest)
        else:
            card = read_json(out / "scorecard.json")
            if card.get("schema_version") != VERSION:
                raise ValueError("unsupported scorecard schema")
        if args.action == "import" and not args.receipt:
            raise ValueError("import requires at least one --receipt")
        for receipt in args.receipt:
            import_receipt(card, receipt)
        save(card, out)
        print(str(out / "SCORECARD.md"))
        print(str(out / "scorecard.json"))
        return 0
    except (ValueError, TypeError, KeyError, OSError) as error:
        print("scorecard: " + str(error), file=sys.stderr)
        return 2


if __name__ == "__main__":
    sys.exit(main())
