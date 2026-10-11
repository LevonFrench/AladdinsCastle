# SPDX-License-Identifier: GPL-3.0-only
"""Bounded, input-free ordinary-Dolphin WGC proof. All receipts stay in .local.

Preparation is safe without a runtime slot. Running requires the lead's slot;
this tool cannot detect an unrelated system prompt or grant its own permission.
"""
import argparse
import configparser
import ctypes
import hashlib
import json
import os
from pathlib import Path
import subprocess
import time

import setup


def sha256(path, header=False):
    with path.open("rb") as stream:
        if header:
            data = stream.read(32)
            if len(data) != 32 or data[:6].decode("ascii", errors="replace") not in setup.GAME_IDS:
                raise ValueError("Disc header is not one of the two scoped games")
            return hashlib.sha256(data).hexdigest()
        digest = hashlib.sha256()
        for block in iter(lambda: stream.read(1024 * 1024), b""):
            digest.update(block)
        return digest.hexdigest()


def admit(config, root=setup.ROOT):
    """Only private explicit paths, exact binary hashes and a scoped header."""
    config = setup.safe_user(config, root)
    state = json.loads(config.read_text(encoding="utf-8-sig"))
    if not isinstance(state, dict):
        raise ValueError("Configuration must be an object")
    for key, basename in (("dolphin", "Dolphin.exe"), ("capture", "acvr-dolphin-theatre.exe"), ("disc", None)):
        if not isinstance(state.get(key), str):
            raise ValueError("Missing explicit input path: " + key)
        path = Path(state[key])
        if not path.is_absolute() or not path.is_file() or (basename and path.name.lower() != basename.lower()):
            raise ValueError("Invalid explicitly supplied executable/disc")
        expected = state[key + ("_header_sha256" if key == "disc" else "_sha256")]
        if not isinstance(expected, str) or sha256(path, header=key == "disc") != expected.lower():
            raise ValueError("Input identity changed: " + key)
    if "dsu_profile" in state:
        profile = Path(state["dsu_profile"])
        manifest = json.loads((profile.parent.parent / "generated-files.json").read_text(encoding="utf-8"))
        if (not profile.is_absolute() or profile.parent.name != "flat-dolphin" or profile.name != "split-right.ini"
                or manifest.get("version") != "shell-stick/1"
                or manifest.get("files", {}).get("flat-dolphin/split-right.ini") != sha256(profile)
                or state.get("dsu_profile_sha256") != sha256(profile)):
            raise ValueError("DSU profile does not match the canonical generated manifest")
    return state


def prepare(config, run, root=setup.ROOT):
    state = admit(config, root)
    run = setup.safe_user(run, root)
    # Never resume or overwrite a previous run, including failed proofs.
    run.mkdir(parents=True, exist_ok=False)
    user = setup.create(run / "User", "one-gun", root)
    path = user / "Config/Dolphin.ini"
    ini = configparser.ConfigParser(interpolation=None)
    ini.optionxform = str
    ini.read_string(path.read_text(encoding="utf-8"))
    ini["Display"]["RenderToMain"] = "False"
    # Verified in pinned MainSettings.cpp: Interface keys, fresh User only.
    # Diagnostic OSD is not game content and must not satisfy the spatial gate.
    ini["Interface"] = {"OnScreenDisplayMessages": "False", "ShowActiveTitle": "True"}
    if "dsu_profile" in state:
        ini["Input"] = {"BackgroundInput": "True"}
        (user / "Config/WiimoteNew.ini").write_bytes(Path(state["dsu_profile"]).read_bytes())
        # Fresh per-game named profiles otherwise override the canonical global DSU bindings.
        for game_id in setup.GAME_IDS:
            (user / "GameSettings" / (game_id + ".ini")).write_bytes(setup.ini({"Controls": {}}))
    path.write_bytes(setup.ini({section: dict(ini[section]) for section in ini.sections()}))
    # Logs and all emulator outputs use the fresh user/cwd, not the installed User.
    (user / "Logs").mkdir()
    result = {"schema": 1, "config": str(config), "run": str(run), "user": str(user),
              "dolphin": setup.command(user, state["dolphin"], state["disc"], root),
              "capture": state["capture"], "inputs": state, "state": "prepared", "live_proof": False}
    with Path(state["disc"]).open("rb") as stream:
        result["game_id"] = stream.read(6).decode("ascii")
    (run / "prepared.json").write_text(json.dumps(result, indent=2) + "\n", encoding="utf-8")
    return result


def owned_window_count(pid):
    """Inspect only this child's HWND eligibility; no titles, input or desktop pixels."""
    api = ctypes.WinDLL("user32", use_last_error=True)
    callback_type = ctypes.WINFUNCTYPE(ctypes.c_bool, ctypes.c_void_p, ctypes.c_ssize_t)
    api.GetWindowThreadProcessId.argtypes = [ctypes.c_void_p, ctypes.POINTER(ctypes.c_ulong)]
    api.IsWindowVisible.argtypes = [ctypes.c_void_p]
    api.GetWindow.argtypes = [ctypes.c_void_p, ctypes.c_uint]
    api.GetWindow.restype = ctypes.c_void_p
    api.EnumWindows.argtypes = [callback_type, ctypes.c_ssize_t]
    count = 0

    @callback_type
    def visit(hwnd, _):
        nonlocal count
        owner = ctypes.c_ulong()
        api.GetWindowThreadProcessId(hwnd, ctypes.byref(owner))
        if owner.value == pid and api.IsWindowVisible(hwnd) and not api.GetWindow(hwnd, 4):
            count += 1
        return True

    if not api.EnumWindows(visit, 0):
        raise OSError("Cannot enumerate owned windows")
    return count


def stop_owned(process):
    termination = "already-exited"
    if process.poll() is None:
        termination = "terminate"
        process.terminate()
        try:
            process.wait(timeout=3)
        except subprocess.TimeoutExpired:
            termination = "kill"
            process.kill()
            process.wait(timeout=3)
    return {"exit": process.returncode, "termination": termination}


def supervise(plan, seconds=15, startup=90, settle=20, popen=subprocess.Popen,
              window_count=owned_window_count, clock=time.monotonic, sleep=time.sleep):
    """Keep both owned children alive until capture completes; always reap them."""
    if not 3 <= seconds <= 60 or not 1 <= startup <= 90 or not 0 <= settle <= 30:
        raise ValueError("Invalid bounded timing")
    run = Path(plan["run"])
    receipt = {"schema": 1, "state": "failed", "live_proof": False,
               "sbs_visual_check": "required", "rgb_gate": False, "owned_cleanup": []}
    children = []
    try:
        with (run / "dolphin.log").open("wb") as log, (run / "capture.log").open("wb") as capture_log:
            receipt["dolphin_launch_requested_unix"] = time.time()
            dolphin = popen(plan["dolphin"], cwd=plan["user"], stdout=log, stderr=subprocess.STDOUT)
            children.append(dolphin)
            receipt["dolphin_created_unix"] = time.time()
            receipt["dolphin_pid"] = dolphin.pid
            deadline = clock() + min(startup, 30)
            while True:
                if dolphin.poll() is not None:
                    raise RuntimeError("Owned Dolphin exited before capture")
                count = window_count(dolphin.pid)
                if count == 1:
                    break
                if count > 1:
                    raise RuntimeError("Ambiguous owned windows; possible dialog, stop for owner")
                if clock() >= deadline:
                    raise TimeoutError("No owned game window; stop and diagnose without input")
                sleep(0.1)
            args = [plan["capture"], "--pid", str(dolphin.pid), "--seconds", str(seconds),
                    "--capture-only", "--game-id", plan["game_id"], "--startup-seconds", str(startup),
                    "--proof-dir", str(run / "proof")]
            # An HWND can predate real rendering. Delay only by an explicit,
            # bounded setting; never treat elapsed time as game/SBS proof.
            if settle:
                ready_at = clock() + settle
                while clock() < ready_at:
                    if dolphin.poll() is not None:
                        raise RuntimeError("Owned Dolphin exited during settle")
                    if window_count(dolphin.pid) != 1:
                        raise RuntimeError("Owned window changed during settle; stop for owner")
                    sleep(0.1)
            receipt["settle_seconds"] = settle
            receipt["capture_launch_requested_unix"] = time.time()
            capture = popen(args, cwd=run, stdout=capture_log, stderr=subprocess.STDOUT)
            children.append(capture)
            receipt["capture_created_unix"] = time.time()
            receipt["capture_pid"] = capture.pid
            deadline = clock() + startup + seconds + 5
            while capture.poll() is None:
                if dolphin.poll() is not None:
                    raise RuntimeError("Owned Dolphin exited during capture")
                if clock() >= deadline:
                    raise TimeoutError("Capture exceeded bound; no automatic retry")
                sleep(0.1)
            receipt["capture_exit"] = capture.returncode
            if capture.returncode != 0:
                raise RuntimeError("Capture gate failed; retain private logs/proofs")
            proof = run / "proof"
            data = json.loads((proof / "capture.json").read_text(encoding="utf-8"))
            if (not isinstance(data, dict) or data.get("capture") != "Windows.Graphics.Capture"
                    or data.get("different_rgb") is not True
                    or data.get("spatial_candidate") is not True or data.get("game_window_selected") is not True
                    or type(data.get("interval_ms")) is not int or data["interval_ms"] < 1000):
                raise RuntimeError("Incomplete capture receipt")
            for name in ("frame-1.png", "frame-2.png"):
                with (proof / name).open("rb") as stream:
                    if stream.read(8) != b"\x89PNG\r\n\x1a\n":
                        raise RuntimeError("Missing PNG proof")
            receipt.update(state="captured-awaiting-inspection", rgb_gate=True)
    except (OSError, ValueError, RuntimeError) as error:
        receipt["error"] = str(error)
    finally:
        # Reverse order: release WGC before stopping its selected Dolphin process.
        for child in reversed(children):
            try:
                receipt["owned_cleanup"].append({"pid": child.pid, **stop_owned(child)})
            except (OSError, subprocess.TimeoutExpired) as error:
                receipt["cleanup_error"] = str(error)
                receipt["state"] = "cleanup-failed"
        receipt["normal_exit"] = bool(children) and len(receipt["owned_cleanup"]) == len(children) and all(
            item["termination"] == "already-exited" and item["exit"] == 0 for item in receipt["owned_cleanup"])
        receipt["finished_unix"] = time.time()
        (run / "result.json").write_text(json.dumps(receipt, indent=2) + "\n", encoding="utf-8")
    return receipt


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("action", choices=("prepare", "run"))
    parser.add_argument("--config", required=True, type=Path)
    parser.add_argument("--run-dir", required=True, type=Path)
    parser.add_argument("--seconds", type=int, default=15)
    parser.add_argument("--settle-seconds", type=int, default=20)
    parser.add_argument("--startup-seconds", type=int, default=90)
    args = parser.parse_args(argv)
    try:
        if args.action == "run" and os.name != "nt":
            raise ValueError("Live WGC capture requires Windows")
        if not 3 <= args.seconds <= 60 or not 0 <= args.settle_seconds <= 30 or not 1 <= args.startup_seconds <= 90:
            raise ValueError("Capture bound must be 3..60 and settle 0..30 seconds")
        plan = prepare(args.config, args.run_dir)
        if args.action == "prepare":
            print("Prepared private isolated run; no application launched")
            return 0
        # Recheck identities immediately before process creation, after writes.
        if admit(args.config) != plan["inputs"]:
            raise ValueError("Configuration changed after preparation")
        result = supervise(plan, args.seconds, startup=args.startup_seconds, settle=args.settle_seconds)
        print(result["state"] + "; inspect private result.json and client PNGs")
        return 0 if result["state"] == "captured-awaiting-inspection" else 1
    except (OSError, ValueError, KeyError) as error:
        parser.exit(1, "Capture harness: " + str(error) + "\n")


if __name__ == "__main__":
    raise SystemExit(main())
