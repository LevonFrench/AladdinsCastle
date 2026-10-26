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
"""
import argparse
import os
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


def run(args):
    os.makedirs(args.out, exist_ok=True)
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
        p = subprocess.Popen(cmd, cwd=os.path.dirname(os.path.abspath(args.host)), creationflags=flags)
        try:
            code = p.wait(timeout=args.timeout)
        except subprocess.TimeoutExpired:
            p.kill()
            code = "killed"
        print(f"{name}: exit {code}", flush=True)


def report(args):
    from PIL import Image, ImageChops, ImageStat

    def eye(name, snap):
        # The left eye of a side by side 1200x720 frame, with the black bands cut off.
        return Image.open(os.path.join(args.out, name, "snaps", snap + ".png")).convert("RGB").crop((0, 180, 600, 540))

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
        key = SNAPS[1][1] if SNAPS[1][1] in pads else None   # the pad line logged with the "mid" snapshot
        held = ""
        if key is not None:
            b, lx, ly, rx, ry = pads[key]
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


def main():
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
    args = ap.parse_args()
    args.fn(args)


if __name__ == "__main__":
    sys.exit(main())
