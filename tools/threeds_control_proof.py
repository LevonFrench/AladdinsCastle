#!/usr/bin/env python3
"""Before/after proof for the VR controls of a 3DS game run through acvr-ps1 (Azahar core).

Every control is tried in its own short hidden run that starts from the same saved
emulator state, so the only difference between two runs is the injected input. Two
runs with no input at all give the noise floor (zero when the emulator replays
exactly). The proof reads:

  * the pad line the host logs at the middle of the press (which 3DS button, which
    stick values the game was given), and
  * how far the picture moved against the idle run (mean absolute pixel difference
    of the left eye, frames taken at fixed counts after the state loads).

    python tools/threeds_control_proof.py run --host acvr-ps1.exe --core azahar_libretro.dll \
        --game <decrypted dump> --state <saved state> --profile <profile.toml> --out <folder>
    python tools/threeds_control_proof.py report --out <folder>

Run `run` through tools/heavy_lease.py (one heavy job at a time on the PC). It needs no
window, no sound and no headset runtime. The report needs Pillow. Nothing here names a
game file or a folder; they come from the command line.

Use a fresh, empty output folder for every run. Existing evidence is never reused or
deleted. A child failure, timeout, missing snapshot or missing midpoint pad sample
fails the command and records run-status.json. Report requires a complete successful
receipt and rechecks its inputs; older unreceipted evidence remains untouched.
"""
import argparse
import json
import os
from pathlib import Path
import re
import subprocess
import sys

# Script lines are "<frame> <command> <args>", counted from the frame the state loads at.
# Names follow the profile's pad map: rtrig, rgrip, lgrip, ltrig, vr ra / rb, rstick, lstick, and the
# stick clicks (the universal Start and Select).
TRIALS = {
    "none1": [],
    "none2": [],
    "laser_rtrig": ["10 rtrig 1", "50 rtrig 0"],
    "bomb_ra": ["10 vr ra 30"],
    "boost_rgrip": ["10 rgrip 1", "70 rgrip 0"],
    "brake_lgrip": ["10 lgrip 1", "70 lgrip 0"],
    "bank_left_ltrig": ["10 ltrig 1", "40 ltrig 0"],
    "bank_right_rb": ["10 vr rb 30"],
    "bank_left_rstick": ["10 rstick -1 0", "40 rstick 0 0"],
    "bank_right_rstick": ["10 rstick 1 0", "40 rstick 0 0"],
    "steer_left": ["10 lstick -1 0", "50 lstick 0 0"],
    "steer_right": ["10 lstick 1 0", "50 lstick 0 0"],
    "steer_up": ["10 lstick 0 1", "50 lstick 0 0"],
    "steer_down": ["10 lstick 0 -1", "50 lstick 0 0"],
    "pause_lclick": ["10 vr lclick 12"],
    "select_rclick": ["10 vr rclick 12"],
}
SNAPS = [("pre", 5), ("mid", 30), ("late", 60), ("end", 125)]
FRAMES = 130
# RetroPad button numbers the host logs, named as the 3DS buttons.
BITS = {0: "B", 1: "Y", 2: "Select", 3: "Start", 4: "Up", 5: "Down", 6: "Left", 7: "Right", 8: "A", 9: "X", 10: "L", 11: "R"}
PAD_LINE = re.compile(r"frame (\d+) pad: buttons 0x([0-9A-Fa-f]+) left (\S+) (\S+) right (\S+) (\S+)")


def script_text(lines):
    """The scripted run for one trial: a snapshot before, the input, then snapshots at fixed counts."""
    events = [(5, "snap pre")] + [(int(l.split()[0]), l.split(None, 1)[1]) for l in lines]
    events += [(f, "snap " + n) for n, f in SNAPS[1:]]
    return "\n".join(f"{f} {c}" for f, c in sorted(events, key=lambda e: e[0])) + "\n"


def decode_pad(buttons):
    """Names of the 3DS buttons held in a logged RetroPad bit mask."""
    return [name for bit, name in sorted(BITS.items()) if buttons >> bit & 1]


def parse_pad_log(text):
    """frame -> (buttons mask, left x, left y, right x, right y) from the host's snapshot log lines."""
    rows = {}
    for line in text.splitlines():
        m = PAD_LINE.search(line)
        if m:
            rows[int(m.group(1))] = (int(m.group(2), 16),) + tuple(float(g) for g in m.groups()[2:])
    return rows


class ProofError(RuntimeError):
    """A run is incomplete or failed; it must never produce a success report."""


def validate_trial(folder):
    folder = Path(folder)
    for name, _ in SNAPS:
        path = folder / "snaps" / (name + ".png")
        if not path.is_file() or path.stat().st_size <= 8:
            raise ProofError(f"{folder.name}: missing snapshot {name}")
        with path.open("rb") as stream:
            if stream.read(8) != b"\x89PNG\r\n\x1a\n":
                raise ProofError(f"{folder.name}: invalid PNG snapshot {name}")
    log = folder / "host.log"
    if not log.is_file():
        raise ProofError(f"{folder.name}: missing host.log")
    pads = parse_pad_log(log.read_text(encoding="utf-8", errors="replace"))
    mid = dict(SNAPS)["mid"]
    if mid not in pads:
        raise ProofError(f"{folder.name}: missing midpoint pad sample at frame {mid}")
    return pads


def write_status(out, status):
    temporary = Path(out) / "run-status.tmp"
    temporary.write_text(json.dumps(status, indent=2) + "\n", encoding="utf-8")
    temporary.replace(Path(out) / "run-status.json")


def validate_run(out):
    status = json.loads((Path(out) / "run-status.json").read_text(encoding="utf-8"))
    if status.get("schema") != 1 or status.get("state") != "complete":
        raise ProofError("run is not complete; no success report can be made")
    if set(status.get("trials", {})) != set(TRIALS):
        raise ProofError("run trial set does not match this proof")
    for name in TRIALS:
        row = status["trials"][name]
        if row.get("state") != "complete" or row.get("exit_code") != 0:
            raise ProofError(f"{name}: trial did not complete successfully")
        validate_trial(Path(out) / name)
    return status


def run(args):
    if args.timeout <= 0:
        raise ProofError("timeout must be positive")
    out = Path(args.out)
    out.mkdir(parents=True, exist_ok=True)
    if any(out.iterdir()):
        raise ProofError("output folder is not empty; choose a fresh folder (old evidence is preserved)")
    status = {"schema": 1, "state": "running", "trials": {}}
    # Exclusive claim also rejects a second runner racing for the same empty folder.
    with (out / "run-status.json").open("x", encoding="utf-8") as stream:
        json.dump(status, stream)
    try:
        run_trials(args, status)
        status["state"] = "complete"
        write_status(out, status)
    except BaseException as error:
        status["state"] = "failed"
        status["error"] = str(error)
        write_status(out, status)
        raise
    return 0


def run_trials(args, status):
    for name, lines in TRIALS.items():
        d = os.path.join(args.out, name)
        for sub in ("system", "saves", "snaps"):
            os.makedirs(os.path.join(d, sub), exist_ok=True)
        script = os.path.join(d, "script.txt")
        with open(script, "w", newline="\n") as f:
            f.write(script_text(lines))
        cmd = [args.host, "--core", args.core, "--game", args.game, "--system", os.path.join(d, "system"), "--saves", os.path.join(d, "saves"),
               "--controls", "pad", "--no-xr", "--mute", "--headless", "--frames", str(FRAMES), "--script", script,
               "--snapshots", os.path.join(d, "snaps"), "--log", os.path.join(d, "host.log"), "--state", args.state, "--profile", args.profile]
        flags = 0x4000 | 0x08000000 if os.name == "nt" else 0   # below normal priority, no window
        row = status["trials"][name] = {"state": "running"}
        write_status(args.out, status)
        p = None
        try:
            p = subprocess.Popen(cmd, cwd=os.path.dirname(os.path.abspath(args.host)), creationflags=flags)
            try:
                code = p.wait(timeout=args.timeout)
            except subprocess.TimeoutExpired:
                p.kill()
                row.update(state="timeout", exit_code=p.wait(timeout=10))
                raise ProofError(f"{name}: timed out after {args.timeout} seconds")
            row["exit_code"] = code
            if code != 0:
                row["state"] = "failed"
                raise ProofError(f"{name}: child exited with code {code}")
            validate_trial(d)
            row["state"] = "complete"
            write_status(args.out, status)
        except BaseException as error:
            if row["state"] != "timeout":
                row["state"] = "failed"
            row["error"] = str(error)
            write_status(args.out, status)
            raise
        finally:
            if p is not None:
                if p.poll() is None:
                    p.kill()
                p.wait(timeout=10)
        print(f"{name}: exit {code}", flush=True)


def report(args):
    validate_run(args.out)
    from PIL import Image, ImageChops, ImageStat

    def eye(name, snap):
        # The left eye of a side by side 1200x720 frame, with the black bands cut off.
        with Image.open(os.path.join(args.out, name, "snaps", snap + ".png")) as image:
            if image.width < 600 or image.height < 540:
                raise ProofError(f"{name}: snapshot {snap} is smaller than the eye crop")
            return image.convert("RGB").crop((0, 180, 600, 540))

    def diff(a, b):
        return sum(ImageStat.Stat(ImageChops.difference(a, b)).mean) / 3

    def changed(a, b):
        # Share of the picture, in percent, where any colour moved by more than 24 of 255.
        mask = ImageChops.difference(a, b).convert("L").point(lambda v: 255 if v > 24 else 0)
        return ImageStat.Stat(mask).mean[0] / 255 * 100

    names = [n for n in TRIALS if not n.startswith("none")]
    noise = {s: round(diff(eye("none1", s), eye("none2", s)), 3) for s, _ in SNAPS}
    print("idle run against a second idle run (noise floor):", noise)
    rows = []
    for n in names:
        with open(os.path.join(args.out, n, "host.log"), errors="replace") as f:
            pads = parse_pad_log(f.read())
        b, lx, ly, rx, ry = pads[dict(SNAPS)["mid"]]
        held = ",".join(decode_pad(b)) or "-"
        held += f" stick({lx:+.1f},{ly:+.1f}) cstick({rx:+.1f},{ry:+.1f})"
        d = [round(diff(eye(n, s), eye("none1", s)), 2) for s in ("mid", "late", "end")]
        c = [round(changed(eye(n, s), eye("none1", s)), 1) for s in ("mid", "late", "end")]
        rows.append((n, held, d))
        print(f"{n:18} pad at mid-press: {held:40} mean change vs idle  mid {d[0]:6}  late {d[1]:6}  end {d[2]:6};  area changed %  mid {c[0]:5}  late {c[1]:5}  end {c[2]:5}")
    w, h = 300, 180
    sheet = Image.new("RGB", (w * 4, h * (len(rows) + 1)), "black")
    for j, (s, _) in enumerate(SNAPS):
        sheet.paste(eye("none1", s).resize((w, h)), (j * w, 0))
    for i, (n, _, _) in enumerate(rows, 1):
        for j, (s, _) in enumerate(SNAPS):
            sheet.paste(eye(n, s).resize((w, h)), (j * w, i * h))
    sheet.save(os.path.join(args.out, "controls-sheet.png"))
    print("sheet:", os.path.join(args.out, "controls-sheet.png"), "(row 1 idle; then one row per control: before, mid, late, end)")


def main(argv=None):
    ap = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    sub = ap.add_subparsers(dest="cmd", required=True)
    r = sub.add_parser("run")
    for a in ("host", "core", "game", "state", "profile", "out"):
        r.add_argument("--" + a, required=True)
    r.add_argument("--timeout", type=int, default=120, help="seconds per trial")
    r.set_defaults(fn=run)
    p = sub.add_parser("report")
    p.add_argument("--out", required=True)
    p.set_defaults(fn=report)
    args = ap.parse_args(argv)
    try:
        return args.fn(args) or 0
    except (ProofError, OSError, ValueError, subprocess.SubprocessError) as error:
        print(f"3DS proof failed: {error}", file=sys.stderr)
        return 1
    except KeyboardInterrupt:
        print("3DS proof interrupted; incomplete output retained", file=sys.stderr)
        return 130


if __name__ == "__main__":
    sys.exit(main())
