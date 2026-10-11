"""Prepare private experiment profiles and summarize presentation intervals. Never launches."""
import argparse
import hashlib
import json
import math
from pathlib import Path
import statistics

HERE = Path(__file__).resolve().parent
REPO = HERE.parent.parent
PLAN = json.loads((HERE / "chronicles-phase1.json").read_text())
PIN = json.loads((REPO / "setups/dolphin-vr/upstream-pin.json").read_text())


def private_path(path, root=None):
    path = Path(path).resolve()
    root = Path(root).resolve() if root else (REPO / ".local").resolve()
    if ".local" not in root.parts or (root != (REPO / ".local").resolve() and root.name != "framerate"):
        raise ValueError("External private root must be a framerate directory under .local")
    if not path.is_relative_to(root) or path == root:
        raise ValueError("Output must be inside the designated private root")
    return path


def sha(path):
    with Path(path).open("rb") as stream:
        return hashlib.file_digest(stream, "sha256").hexdigest()


def header(path):
    with Path(path).open("rb") as f:
        data = f.read(32)
    if len(data) != 32 or data[24:28] != bytes.fromhex("5d1c9ea3"):
        raise ValueError("Expected a Wii ISO header")
    return {"game_id": data[:6].decode("ascii"), "disc_number": data[6],
            "revision": data[7], "header_sha256": hashlib.sha256(data).hexdigest()}


def ini(sections):
    return "\n".join("[" + s + "]\n" + "\n".join(f"{k} = {v}" for k, v in values.items())
                     for s, values in sections.items()) + "\n"


def prepare(executable, disc, game_id, experiment, output, private_root=None):
    if game_id not in PLAN["games"] or experiment not in PLAN["experiments"]:
        raise ValueError("Only the two Chronicles PAL IDs and three Phase 1 experiments are admitted")
    output = private_path(output, private_root)
    if output.exists():
        raise ValueError("Use a new run directory; never overwrite a profile or receipt")
    executable, disc = Path(executable).resolve(), Path(disc).resolve()
    if sha(executable) != PIN["executable_sha256"]:
        raise ValueError("Executable does not match pinned release")
    identity = header(disc)
    if identity["game_id"] != game_id:
        raise ValueError("Disc ID differs from exact selected title")
    code_on = experiment != "baseline"
    config = {
        "Core": {"EnableCheats": str(code_on), "OverclockEnable": "False", "Overclock": "1.0",
                 "VIOverclockEnable": str(experiment == "code-vbi90"),
                 "VIOverclock": "1.5" if experiment == "code-vbi90" else "1.0",
                 "EmulationSpeed": "1.0", "GFXBackend": "D3D"},
        "Display": {"Fullscreen": "False"}, "Interface": {"ConfirmStop": "False"}}
    gfx = {"Hardware": {"VSync": "False"}, "VR": {"EnableOpenXR": "False"},
           "Settings": {"ShowFPS": "True", "ShowVPS": "True", "ShowSpeed": "True",
                        "ShowFTimes": "True", "LogRenderTimeToFile": "True",
                        "InternalResolution": "3", "MSAA": "1", "SSAA": "False",
                        "ShaderCompilationMode": "2", "WaitForShadersBeforeStarting": "True",
                        "wideScreenHack": "False", "DumpFramesAsImages": "False"},
           "Enhancements": {"MaxAnisotropy": "4", "PostProcessingShader": ""}}
    user = output / "User"
    (user / "Config").mkdir(parents=True)
    (user / "GameSettings").mkdir()
    (user / "Logs").mkdir()
    (user / "Config/Dolphin.ini").write_text(ini(config), encoding="utf-8")
    (user / "Config/GFX.ini").write_text(ini(gfx), encoding="utf-8")
    # Wii PAL60 setting belongs to SYSCONF; override at launch, never copy an existing NAND.
    candidate = PLAN["games"][game_id]
    game_ini = ""
    if code_on:
        game_ini = f"[Gecko]\n${candidate['code_name']}\n{candidate['code']}\n\n[Gecko_Enabled]\n${candidate['code_name']}\n"
    (user / f"GameSettings/{game_id}.ini").write_text(game_ini, encoding="utf-8")
    overrides = {f"Dolphin.Core.{k}": v for k, v in config["Core"].items()}
    overrides.update({"GFX.VR.EnableOpenXR": "False", "GFX.Hardware.VSync": "False",
                      "SYSCONF.IPL.E60": "True"})
    args = ["--user", str(user), "--batch", "--exec", str(disc)]
    for key, value in overrides.items():
        args += ["-C", f"{key}={value}"]
    record = {"schema_version": 1, "status": "prepared-unmeasured", "experiment": experiment,
              "executable": str(executable), "executable_sha256": PIN["executable_sha256"],
              "release": PIN["release"], "source_commit": PIN["source_commit"],
              "disc": str(disc), "disc_bytes": disc.stat().st_size, **identity,
              "user": str(user), "arguments": args, "candidate_code": candidate if code_on else None,
              "settings_sha256": {str(p.relative_to(user)): sha(p) for p in user.rglob("*.ini")},
              "shipped_game_settings": "unchanged; inherited from executable Sys directory",
              "counter_observations": [], "stretch": None, "measured_results": None}
    (output / "run.json").write_text(json.dumps(record, indent=2) + "\n", encoding="utf-8")
    return record


def summarize(path, start_s, duration_s, kind="presentation"):
    if start_s < 0 or duration_s <= 0:
        raise ValueError("An explicit positive measured window is required")
    elapsed, selected = 0.0, []
    for line in Path(path).read_text().splitlines():
        ms = float(line)
        if not math.isfinite(ms) or ms <= 0:
            raise ValueError("Invalid interval; do not silently discard samples")
        begin = elapsed
        elapsed += ms / 1000
        # Include only complete intervals inside the named stretch.
        if begin >= start_s and elapsed <= start_s + duration_s:
            selected.append(ms)
    if elapsed < start_s + duration_s or not selected:
        raise ValueError("Log does not cover the requested measured window")
    values = sorted(selected)
    return {"kind": kind + "-interval-summary", "samples": len(values),
            "window_start_s": start_s, "window_duration_s": duration_s,
            "interval_mean_ms": statistics.mean(values), "interval_median_ms": statistics.median(values),
            "interval_p95_ms": values[math.ceil(len(values) * .95) - 1],
            "interval_max_ms": max(values),
            "limits": "Host presentation intervals include duplicate presentations. Not unique game FPS, CPU/GPU work time, normal game speed or headset pacing."}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    sub = parser.add_subparsers(dest="command", required=True)
    prep = sub.add_parser("prepare")
    for name in ("executable", "disc", "game-id", "experiment", "output"):
        prep.add_argument("--" + name, required=True)
    prep.add_argument("--private-root")
    summary = sub.add_parser("summarize")
    summary.add_argument("log")
    summary.add_argument("--start-seconds", type=float, required=True)
    summary.add_argument("--duration-seconds", type=float, required=True)
    summary.add_argument("--output", required=True)
    summary.add_argument("--private-root")
    summary.add_argument("--kind", choices=("presentation", "vblank"), default="presentation")
    args = parser.parse_args()
    if args.command == "prepare":
        result = prepare(args.executable, args.disc, args.game_id, args.experiment, args.output, args.private_root)
        print(json.dumps({"status": result["status"], "game_id": result["game_id"],
                          "revision": result["revision"], "experiment": result["experiment"]}))
    else:
        result = summarize(args.log, args.start_seconds, args.duration_seconds, args.kind)
        output = private_path(args.output, args.private_root)
        if output.exists():
            raise ValueError("Summary output already exists")
        output.write_text(json.dumps(result, indent=2) + "\n", encoding="utf-8")
        print(json.dumps(result))


if __name__ == "__main__":
    main()
