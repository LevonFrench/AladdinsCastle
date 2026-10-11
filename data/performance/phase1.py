"""Prepare private experiment profiles and summarize presentation intervals. Never launches."""
import argparse
import configparser
import hashlib
import json
import math
from pathlib import Path
import secrets
import statistics
import subprocess
import sys

HERE = Path(__file__).resolve().parent
REPO = HERE.parent.parent
PLAN = json.loads((HERE / "chronicles-phase1.json").read_text())
NATIVE_PLAN = json.loads((HERE / "native-vbi-phase1.json").read_text())
DSE_CANDIDATE = json.loads((HERE / "deadspace-code-candidate.json").read_text())
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
    native = experiment in NATIVE_PLAN["experiments"]
    admitted = NATIVE_PLAN if native else PLAN
    if game_id not in admitted["games"] or experiment not in admitted["experiments"]:
        raise ValueError("Game or experiment is outside the bounded Phase 1 run set")
    if native and admitted["games"][game_id]["format"] == "wbfs":
        raise ValueError("WBFS preparation awaits the Hub identity contract; no guessed header offset")
    output = private_path(output, private_root)
    if output.exists():
        raise ValueError("Use a new run directory; never overwrite a profile or receipt")
    executable, disc = Path(executable).resolve(), Path(disc).resolve()
    if sha(executable) != PIN["executable_sha256"]:
        raise ValueError("Executable does not match pinned release")
    identity = header(disc)
    if identity["game_id"] != game_id:
        raise ValueError("Disc ID differs from exact selected title")
    code_on = not native and experiment != "baseline"
    config = {
        "Core": {"EnableCheats": str(code_on), "OverclockEnable": "False", "Overclock": "1.0",
                 "VIOverclockEnable": str(experiment == "code-vbi90"),
                 "VIOverclock": "1.5" if experiment == "code-vbi90" else "1.0",
                 "EmulationSpeed": "1.0", "GFXBackend": "D3D"},
        "Display": {"Fullscreen": "False", "RenderWindowWidth": "1280", "RenderWindowHeight": "720", "RenderWindowAutoSize": "False"},
        "Interface": {"ConfirmStop": "False"},
        "General": {"ISOPaths": "0", "RecursiveISOPaths": "False"},
        "Analytics": {"Enabled": "False", "PermissionAsked": "True"},
        "AutoUpdate": {"UpdateTrack": ""}}
    gfx = {"Hardware": {"VSync": "False"}, "VR": {"EnableOpenXR": "False"},
           "Settings": {"ShowFPS": "True", "ShowVPS": "True", "ShowSpeed": "True",
                        "ShowFTimes": "True", "LogRenderTimeToFile": "True",
                        "InternalResolution": "3", "MSAA": "1", "SSAA": "False",
                        "ShaderCompilationMode": "2", "WaitForShadersBeforeStarting": "True",
                        "wideScreenHack": "False", "DumpFramesAsImages": "False"},
           "Enhancements": {"MaxAnisotropy": "4", "PostProcessingShader": ""}}
    native_mode = NATIVE_PLAN["experiments"][experiment] if native else None
    native_value = NATIVE_PLAN["modes"][native_mode] if native else None
    if native:
        gfx["VR"].update({"EnableOpenXR": "True", "ForcedVBIFrequency": str(native_value),
                          "AutoVBIFromHMD": "False", "EagerHeartbeat": "False", "MirrorView": "0"})
    user = output / "User"
    (user / "Config").mkdir(parents=True)
    (user / "GameSettings").mkdir()
    (user / "Logs").mkdir()
    (user / "Config/Dolphin.ini").write_text(ini(config), encoding="utf-8")
    (user / "Config/GFX.ini").write_text(ini(gfx), encoding="utf-8")
    if native:
        (user / "Config/Logger.ini").write_text(ini({
            "Options": {"WriteToFile": "True", "WriteToConsole": "False", "Verbosity": "4"},
            "Logs": {"OpenXR": "True", "Video": "True", "CORE": "True", "Wiimote": "True", "CI": "True"}}), encoding="utf-8")
    # Wii PAL60 setting belongs to SYSCONF; override at launch, never copy an existing NAND.
    candidate = PLAN["games"][game_id] if code_on else None
    game_ini = ""
    if code_on:
        game_ini = f"[Gecko]\n${candidate['code_name']}\n{candidate['code']}\n\n[Gecko_Enabled]\n${candidate['code_name']}\n"
    (user / f"GameSettings/{game_id}.ini").write_text(game_ini, encoding="utf-8")
    overrides = {f"Dolphin.Core.{k}": v for k, v in config["Core"].items()}
    overrides.update({"Graphics.VR.EnableOpenXR": "False", "Graphics.Hardware.VSync": "False",
                      "SYSCONF.IPL.E60": "True"})
    if native:
        overrides.update(NATIVE_PLAN["constant_overrides"])
        overrides["Graphics.VR.ForcedVBIFrequency"] = str(native_value)
    args = ["--user", str(user), "--batch", "--exec", str(disc)]
    for key, value in overrides.items():
        args += ["-C", f"{key}={value}"]
    record = {"schema_version": 1,
              "status": "prepared-awaiting-scripted-input" if native else "prepared-unmeasured",
              "experiment": experiment,
              "executable": str(executable), "executable_sha256": PIN["executable_sha256"],
              "release": PIN["release"], "source_commit": PIN["source_commit"],
              "disc": str(disc), "disc_bytes": disc.stat().st_size, **identity,
              "user": str(user), "arguments": args, "candidate_code": candidate if code_on else None,
              "settings_sha256": {str(p.relative_to(user)): sha(p) for p in user.rglob("*.ini")},
              "shipped_game_settings": "unchanged; inherited from executable Sys directory",
              "counter_observations": [], "stretch": None, "measured_results": None}
    if native:
        record.update({"route": "native-vbi", "forced_vbi_mode": native_mode,
                       "forced_vbi_value": native_value, "input_contract": None,
                       "runtime_dependencies": NATIVE_PLAN["runtime_dependencies"],
                       "maximum_attempt_seconds": NATIVE_PLAN["maximum_attempt_seconds"]})
    (output / "run.json").write_text(json.dumps(record, indent=2) + "\n", encoding="utf-8")
    return record


def attach_input(run_directory, profile, manifest, script, entrypoint, port=26760, private_root=None):
    """Bind the lead's generated profile and checked timeline; never opens a socket."""
    run_directory = private_path(run_directory, private_root)
    record_path = run_directory / "run.json"
    record = json.loads(record_path.read_text())
    native = record.get("route") == "native-vbi" and record.get("status") == "prepared-awaiting-scripted-input"
    disabled = record.get("route") == "disabled-code-candidate" and record.get("status") == "prepared-disabled-code-candidate"
    if not (native or disabled) or record.get("input_contract"):
        raise ValueError("Input attaches once, to a fresh native or disabled preview profile only")
    if type(port) is not int or not 1024 <= port <= 65535:
        raise ValueError("Invalid DSU port")
    profile, manifest, script, entrypoint = map(lambda p: Path(p).resolve(), (profile, manifest, script, entrypoint))
    if entrypoint.name != "scripted.py":
        raise ValueError("Expected the lead scripted.py")
    game_key = NATIVE_PLAN["games"][record["game_id"]]["control_key"]
    if profile.name == "split-right.ini" and record["game_id"] in ("RBUP08", "SBDP08"):
        profile_key, native_key = "flat-dolphin/split-right.ini", None
    else:
        profile_key = f"games/{game_key}/scripted-dsu-right.ini"
        native_key = f"games/{game_key}/dolphinxr-right.ini"
    if profile != (manifest.parent / profile_key).resolve():
        raise ValueError("Profile path must match this exact game's shared manifest entry")
    profile_bytes = profile.read_bytes()
    profile_digest = hashlib.sha256(profile_bytes).hexdigest()
    manifest_bytes = manifest.read_bytes()
    contract = json.loads(manifest_bytes)
    if contract.get("version") != "shell-stick/1" or contract.get("files", {}).get(profile_key, "").lower() != profile_digest:
        raise ValueError("Generated scripted profile differs from its shared manifest")
    native_bytes = None
    if native_key:
        native_bytes = (manifest.parent / native_key).read_bytes()
        native_digest = hashlib.sha256(native_bytes).hexdigest()
        if contract.get("files", {}).get(native_key, "").lower() != native_digest:
            raise ValueError("Paired native profile differs from the same game's shared manifest")
        paired = configparser.ConfigParser(interpolation=None)
        paired.read_string(native_bytes.decode("utf-8-sig"))
        if paired.get("Wiimote1", "Source", fallback=None) != "3" or paired.get("Wiimote1", "Device", fallback=None) != "OpenXR/0/OpenXR Controller":
            raise ValueError("Paired native profile must identify the supplied OpenXR slot")
    parsed = configparser.ConfigParser(interpolation=None)
    parsed.optionxform = str
    parsed.read_string(profile_bytes.decode("utf-8-sig"))
    expected = {"Source": "1", "Device": "DSUClient/0/AladdinsCastle",
                "Buttons/A": "`Cross`", "Buttons/B": "`R2`", "Buttons/+": "`Options`"}
    if any(parsed.get("Wiimote1", key, fallback=None) != value for key, value in expected.items()):
        raise ValueError("Scripted Wii input does not match the supplied slot/button contract")
    if any(parsed.get(section, "Source", fallback=None) != "0" for section in ("Wiimote2", "Wiimote3", "Wiimote4", "BalanceBoard")):
        raise ValueError("Scripted profile must disable the other remotes")
    if script.stat().st_size > 1024 * 1024:
        raise ValueError("Timeline exceeds the lead parser's size bound")
    raw = script.read_bytes()
    timeline = json.loads(raw)
    if type(timeline.get("seconds")) not in (int, float) or not 0 < timeline["seconds"] <= 180:
        raise ValueError("This run set bounds timelines to 180 seconds")
    entrypoint_digest = sha(entrypoint)
    producer = Path(sys.executable).resolve()
    producer_digest = sha(producer)
    checked = subprocess.run([str(producer), str(entrypoint), "--script", str(script), "--check"],
                             check=True, capture_output=True, text=True)
    result = json.loads(checked.stdout)
    if result.get("valid") is not True or result.get("sha256") != hashlib.sha256(raw).hexdigest():
        raise ValueError("Lead timeline check did not confirm exact script bytes")
    if sha(entrypoint) != entrypoint_digest:
        raise ValueError("Lead entrypoint changed during timeline validation")
    user = private_path(record["user"], private_root)
    for name, digest in record["settings_sha256"].items():
        if sha(private_path(user / name, private_root)) != digest:
            raise ValueError("Prepared settings changed before attaching input")
    script_copy = run_directory / "input-script.json"
    if script_copy.exists():
        raise ValueError("Script copy already exists")
    (user / "Config/WiimoteNew.ini").write_bytes(profile_bytes)
    (user / "Config/DSUClient.ini").write_text(ini({"Server": {
        "Enabled": "True", "Entries": f"AladdinsCastle:127.0.0.1:{port};"}}), encoding="utf-8")
    script_copy.write_bytes(raw)
    if native_bytes is not None:
        (run_directory / "native-control-reference.ini").write_bytes(native_bytes)
    record["input_contract"] = {"profile_sha256": profile_digest, "profile_key": profile_key,
                                "manifest_sha256": hashlib.sha256(manifest_bytes).hexdigest(),
                                "native_profile_key": native_key,
                                "native_profile_sha256": native_digest if native_key else None,
                                "script": str(script_copy), "script_sha256": sha(script_copy),
                                "entrypoint": str(entrypoint), "entrypoint_sha256": entrypoint_digest,
                                "ready_protocol": 1, "session_token": secrets.token_hex(16),
                                "producer_executable": str(producer), "producer_executable_sha256": producer_digest,
                                "port": port, "timeline_seconds": timeline["seconds"],
                                "validation": "lead --check only; no socket, buttons or game progress"}
    record["settings_sha256"] = {str(p.relative_to(user)): sha(p) for p in user.rglob("*.ini")}
    if native:
        record["status"] = "prepared-unmeasured"
    record_path.write_text(json.dumps(record, indent=2) + "\n", encoding="utf-8")
    return record


def prepare_deadspace_candidate(executable, disc, output, private_root=None):
    identity = header(disc)
    if identity["game_id"] != DSE_CANDIDATE["required_game_id"] or identity["revision"] != DSE_CANDIDATE["required_revision"]:
        raise ValueError("Dead Space candidate requires exact RZJE69 revision 0; no other region or revision")
    record = prepare(executable, disc, "RZJE69", "native-off", output, private_root)
    run = private_path(output, private_root)
    user = Path(record["user"])
    candidate = DSE_CANDIDATE
    (user / "GameSettings/RZJE69.ini").write_text(
        f"[ActionReplay]\n${candidate['name']}\n{candidate['code']}\n\n[ActionReplay_Disabled]\n${candidate['name']}\n", encoding="utf-8")
    parsed = configparser.ConfigParser(interpolation=None)
    parsed.optionxform = str
    parsed.read(user / "Config/GFX.ini")
    parsed["VR"]["EnableOpenXR"] = "False"
    (user / "Config/GFX.ini").write_text(ini({s: dict(parsed[s]) for s in parsed.sections()}), encoding="utf-8")
    record["arguments"] = ["Graphics.VR.EnableOpenXR=False" if arg == "Graphics.VR.EnableOpenXR=True" else arg for arg in record["arguments"]]
    record.update({"route": "disabled-code-candidate", "experiment": "deadspace-code-candidate",
                   "status": "prepared-disabled-code-candidate", "candidate_code": candidate,
                   "runtime_dependencies": candidate["runtime_dependencies"]})
    record["settings_sha256"] = {str(p.relative_to(user)): sha(p) for p in user.rglob("*.ini")}
    (run / "run.json").write_text(json.dumps(record, indent=2) + "\n", encoding="utf-8")
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
    attach = sub.add_parser("attach-input")
    for name in ("run-directory", "profile", "manifest", "script", "entrypoint"):
        attach.add_argument("--" + name, required=True)
    attach.add_argument("--port", type=int, default=26760)
    attach.add_argument("--private-root")
    dse = sub.add_parser("prepare-deadspace-candidate")
    for name in ("executable", "disc", "output"):
        dse.add_argument("--" + name, required=True)
    dse.add_argument("--private-root")
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
    elif args.command == "prepare-deadspace-candidate":
        result = prepare_deadspace_candidate(args.executable, args.disc, args.output, args.private_root)
        print(json.dumps({"status": result["status"], "game_id": result["game_id"], "revision": result["revision"]}))
    elif args.command == "attach-input":
        result = attach_input(args.run_directory, args.profile, args.manifest, args.script, args.entrypoint, args.port, args.private_root)
        print(json.dumps({"status": result["status"], "game_id": result["game_id"], "experiment": result["experiment"]}))
    else:
        result = summarize(args.log, args.start_seconds, args.duration_seconds, args.kind)
        output = private_path(args.output, args.private_root)
        if output.exists():
            raise ValueError("Summary output already exists")
        output.write_text(json.dumps(result, indent=2) + "\n", encoding="utf-8")
        print(json.dumps(result))


if __name__ == "__main__":
    main()
