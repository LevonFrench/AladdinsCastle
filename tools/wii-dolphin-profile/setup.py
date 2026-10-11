# SPDX-License-Identifier: GPL-3.0-only
"""Create fresh, isolated ordinary-Dolphin profiles. Never starts an application."""
import argparse
import configparser
import hashlib
import io
import json
import os
from pathlib import Path
import shutil
import tempfile

ROOT = Path(__file__).resolve().parents[2]
GAME_IDS = ("RBUP08", "SBDP08")
PIN = "85c4266b29eb308d173fefa0f21f5cb77df3516a"
MANIFEST = "aladdinscastle-profile.json"


def ini(sections):
    config = configparser.ConfigParser(interpolation=None)
    config.optionxform = str
    config.read_dict(sections)
    stream = io.StringIO()
    config.write(stream)
    return stream.getvalue().encode("utf-8")


def binding(slot, control, default):
    qualifier = "" if slot == default else f"DSUClient/{slot}/AladdinsCastle:"
    return f"`{qualifier}{control}`"


def profile(remote, nunchuk):
    b = lambda control: binding(remote, control, remote)
    n = lambda control: binding(nunchuk, control, remote)
    result = {"Device": f"DSUClient/{remote}/AladdinsCastle",
              "Extension": "Nunchuk", "Extension/Attach MotionPlus": "False",
              "IR/Relative Input": "False", "IR/Auto-Hide": "False",
              "IMUIR/Enabled": "False"}
    for wii, source in {"A": "Cross", "B": "R2", "+": "Options"}.items():
        result[f"Buttons/{wii}"] = b(source)
    for direction, source in {"Up": "Pad N", "Down": "Pad S", "Left": "Pad W", "Right": "Pad E"}.items():
        result[f"D-Pad/{direction}"] = b(source)
    # DSU wire Y is inverted relative to SDL axes: positive byte displacement is UP.
    for direction, suffix in {"Up": "Y+", "Down": "Y-", "Left": "X-", "Right": "X+"}.items():
        result[f"IR/{direction}"] = b(f"Right {suffix}")
        result[f"Nunchuk/Stick/{direction}"] = n(f"Left {suffix}")
    result["Nunchuk/Buttons/Z"] = n("L2")
    result["Nunchuk/Buttons/C"] = n("Circle")
    for direction in ("Up", "Down", "Left", "Right", "Forward", "Backward"):
        result[f"IMUAccelerometer/{direction}"] = b(f"Accel {direction}")
        result[f"Nunchuk/IMUAccelerometer/{direction}"] = n(f"Accel {direction}")
    for direction in ("Pitch Up", "Pitch Down", "Roll Left", "Roll Right", "Yaw Left", "Yaw Right"):
        result[f"IMUGyroscope/{direction}"] = b(f"Gyro {direction}")
    # Simulated motion is intentionally not added to physical IMU motion.
    for prefix in ("", "Nunchuk/"):
        for axis in ("X", "Y", "Z"):
            result[f"{prefix}Shake/{axis}"] = ""
        for direction in ("Up", "Down", "Left", "Right", "Forward", "Backward"):
            result[f"{prefix}Swing/{direction}"] = ""
    return result


def render(mode):
    if mode not in ("one-gun", "two-remotes"):
        raise ValueError("Unknown profile mode")
    count = 1 if mode == "one-gun" else 2
    remotes = [profile(i, 1 if mode == "one-gun" else i) for i in range(count)]
    names = [f"AladdinsCastle-{mode}-{i+1}" for i in range(count)]
    wiimotes = {f"Wiimote{i+1}": ({"Source": "1", **remotes[i]} if i < count else {"Source": "0"})
                for i in range(4)}
    wiimotes["BalanceBoard"] = {"Source": "0"}
    files = {
        "Config/DSUClient.ini": ini({"Server": {"Enabled": "True", "Entries": "AladdinsCastle:127.0.0.1:26760"}}),
        "Config/WiimoteNew.ini": ini(wiimotes),
        "Config/GFX.ini": ini({"Stereoscopy": {"StereoMode": "1", "StereoDepth": "20", "StereoConvergence": "20", "StereoSwapEyes": "False"}}),
        "Config/Dolphin.ini": ini({"Core": {"EnableCheats": "False"},
                                  "Analytics": {"Enabled": "False", "PermissionAsked": "True"},
                                  "AutoUpdate": {"UpdateTrack": ""}}),
    }
    for name, data in zip(names, remotes):
        files[f"Config/Profiles/Wiimote/{name}.ini"] = ini({"Profile": data})
    for game in GAME_IDS:
        files[f"GameSettings/{game}.ini"] = ini({"Controls": {f"WiimoteProfile{i+1}": name for i, name in enumerate(names)}})
    return files


def safe_user(user, root=ROOT):
    root = Path(root).resolve()
    local = root / ".local"
    user = Path(os.path.abspath(user))
    # Check lexical containment AND every existing ancestor; reject junctions/symlinks.
    if not user.is_relative_to(local) or user == local:
        raise ValueError("--user must name a child directory under this checkout's .local")
    current = user
    while current != root:
        if current.is_symlink() or (hasattr(current, "is_junction") and current.is_junction()):
            raise ValueError("Linked user-directory ancestors are not allowed")
        current = current.parent
    if not user.resolve().is_relative_to(local.resolve()):
        raise ValueError("User path escapes .local")
    return user


def digest(data):
    return hashlib.sha256(data).hexdigest()


def check(user, root=ROOT):
    user = safe_user(user, root)
    manifest = json.loads((user / MANIFEST).read_text(encoding="utf-8"))
    expected = render(manifest["mode"])
    hashes = {name: digest(data) for name, data in expected.items()}
    if manifest != {"schema": 1, "mode": manifest["mode"], "source_pin": PIN, "games": list(GAME_IDS), "files": hashes}:
        raise ValueError("Profile manifest has unexpected contents")
    for name, data in expected.items():
        path = user / name
        safe_user(path, root)
        if not path.is_file() or path.read_bytes() != data:
            raise ValueError(f"Generated configuration changed: {name}")
    return manifest


def create(user, mode, root=ROOT):
    user = safe_user(user, root)
    files = render(mode)
    if user.exists():
        if check(user, root)["mode"] != mode:
            raise ValueError("Use a fresh --user directory to change modes")
        return user
    user.parent.mkdir(parents=True, exist_ok=True)
    stage = Path(tempfile.mkdtemp(prefix=".profile-stage-", dir=user.parent))
    try:
        for name, data in files.items():
            target = stage / name
            target.parent.mkdir(parents=True, exist_ok=True)
            target.write_bytes(data)
        (stage / MANIFEST).write_text(json.dumps({"schema": 1, "mode": mode, "source_pin": PIN,
            "games": list(GAME_IDS), "files": {name: digest(data) for name, data in files.items()}}, indent=2)+"\n", encoding="utf-8")
        # rename (rather than replace) cannot overwrite an existing directory on Windows.
        # Recheck on all platforms and never touch an existing user's contents.
        if user.exists():
            raise ValueError("User directory appeared during creation")
        stage.rename(user)
    finally:
        if stage.exists():
            shutil.rmtree(stage)
    return user


def command(user, dolphin, game_file=None, root=ROOT):
    check(user, root)
    args = [str(Path(dolphin).resolve()), "--user", str(safe_user(user, root))]
    if game_file:
        args += ["-b", "-e", str(Path(game_file).resolve())]
    return args


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("action", choices=("create", "inspect", "command"))
    parser.add_argument("--user", required=True, type=Path)
    parser.add_argument("--mode", choices=("one-gun", "two-remotes"), default="one-gun")
    parser.add_argument("--dolphin", type=Path)
    parser.add_argument("--game-file", type=Path)
    args = parser.parse_args()
    try:
        if args.action == "create":
            create(args.user, args.mode)
        manifest = check(args.user)
        if args.action == "command":
            if args.dolphin is None:
                parser.error("command requires --dolphin")
            argv = command(args.user, args.dolphin, args.game_file)
            print(json.dumps(argv))
            print("& " + " ".join("'"+s.replace("'", "''")+"'" for s in argv))
        else:
            print(json.dumps(manifest, indent=2))
            print("Right hand: fire trigger, interact primary, Start menu; absolute aim pointer.")
            print("Nunchuk: stick, grip Z, secondary C; physical acceleration, no simulated Shake/Swing.")
            print("one-gun: right Remote + left Nunchuk; two-remotes: each hand owns both attachments.")
            print("Side-by-side 3D theatre requires an SBS viewer. True 3D and live acceptance remain separate.")
        return 0
    except (OSError, ValueError, KeyError) as error:
        parser.exit(1, f"Profile error: {error}\n")


if __name__ == "__main__":
    raise SystemExit(main())
