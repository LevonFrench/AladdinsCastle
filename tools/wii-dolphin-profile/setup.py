# SPDX-License-Identifier: GPL-3.0-only
"""Create fresh, isolated ordinary-Dolphin profiles. Never starts an application."""
import argparse
import configparser
import hashlib
import io
import json
import math
import os
from pathlib import Path
import shutil
import tempfile
from product import load as load_product
from product import snapshot as product_snapshot
from disc_header import valid_id

ROOT = Path(__file__).resolve().parents[2]
GAME_IDS = ("RBUP08", "SBDP08")
PIN = "85c4266b29eb308d173fefa0f21f5cb77df3516a"
MANIFEST = "aladdinscastle-profile.json"
POINTER_YAW = math.degrees(2*math.atan(3.2/(2*2.0)))
POINTER_PITCH = math.degrees(2*math.atan(1.8/(2*2.0)))


def ini(sections):
    config = configparser.ConfigParser(interpolation=None)
    config.optionxform = str
    config.read_dict(sections)
    stream = io.StringIO()
    config.write(stream)
    return stream.getvalue().encode("utf-8")


def binding(slot, control, default, server_name=None):
    server_name = server_name or load_product(ROOT / "data/product.json")["dsu_server_name"]
    qualifier = "" if slot == default else f"DSUClient/{slot}/{server_name}:"
    return f"`{qualifier}{control}`"


def profile(remote, nunchuk, server_name=None):
    server_name = server_name or load_product(ROOT / "data/product.json")["dsu_server_name"]
    b = lambda control: binding(remote, control, remote, server_name)
    n = lambda control: binding(nunchuk, control, remote, server_name)
    result = {"Device": f"DSUClient/{remote}/{server_name}",
              "Extension": "Nunchuk", "Extension/Attach MotionPlus": "False",
              "IR/Relative Input": "False", "IR/Auto-Hide": "False",
              "IR/Total Yaw": f"{POINTER_YAW:.6f}", "IR/Total Pitch": f"{POINTER_PITCH:.6f}",
              "IR/Hide": b("Square"),
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


def profile_games(games=None):
    games = list(GAME_IDS) if games is None else games
    if not isinstance(games, (list, tuple)) or not (
            tuple(games) == GAME_IDS or len(games) == 1 and valid_id(games[0])):
        raise ValueError("Expected legacy game pair or one selected game ID")
    return list(games)


def render(mode, labels=None, _prefix=None, games=None):
    if mode not in ("one-gun", "two-remotes"):
        raise ValueError("Unknown profile mode")
    count = 1 if mode == "one-gun" else 2
    labels = labels or load_product(ROOT / "data/product.json")
    remotes = [profile(i, 1 if mode == "one-gun" else i, labels["dsu_server_name"]) for i in range(count)]
    prefix = _prefix or labels["short_id"]
    names = [f"{prefix}-{mode}-{i+1}" for i in range(count)]
    wiimotes = {f"Wiimote{i+1}": ({"Source": "1", **remotes[i]} if i < count else {"Source": "0"})
                for i in range(4)}
    wiimotes["BalanceBoard"] = {"Source": "0"}
    files = {
        # Pinned ConfigChanged rebuilds every entry with ';' and raw-compares.
        "Config/DSUClient.ini": ini({"Server": {"Enabled": "True", "Entries": f"{labels['dsu_server_name']}:127.0.0.1:26760;"}}),
        "Config/WiimoteNew.ini": ini(wiimotes),
        "Config/GFX.ini": ini({"Stereoscopy": {"StereoMode": "1", "StereoDepth": "20", "StereoConvergence": "20", "StereoSwapEyes": "False"}}),
        "Config/Dolphin.ini": ini({"Core": {"EnableCheats": "False"},
                                  "Display": {"RenderWindowWidth": "1280", "RenderWindowHeight": "720", "RenderWindowAutoSize": "False"},
                                  "Analytics": {"Enabled": "False", "PermissionAsked": "True"},
                                  "AutoUpdate": {"UpdateTrack": ""}}),
    }
    for name, data in zip(names, remotes):
        files[f"Config/Profiles/Wiimote/{name}.ini"] = ini({"Profile": data})
    for game in profile_games(games):
        files[f"GameSettings/{game}.ini"] = ini({"Controls": {f"WiimoteProfile{i+1}": name for i, name in enumerate(names)}})
    return files


def _pre_canonical_dsu_entries(files, server_name):
    """Exact older bytes for manifest admission only; never a fresh profile."""
    return {**files, "Config/DSUClient.ini": ini({"Server": {
        "Enabled": "True", "Entries": f"{server_name}:127.0.0.1:26760"}})}


def legacy_templates(mode):
    """Exact pre-D62 identities are historical data, not current label defaults."""
    legacy = render(mode, {"short_id": "aladdinscastle", "dsu_server_name": "AladdinsCastle"}, _prefix="AladdinsCastle")
    legacy = _pre_canonical_dsu_entries(legacy, "AladdinsCastle")
    previous = dict(legacy)
    for name, data in previous.items():
        if name == "Config/WiimoteNew.ini" or name.startswith("Config/Profiles/Wiimote/"):
            parsed = configparser.ConfigParser(interpolation=None)
            parsed.optionxform = str
            parsed.read_string(data.decode("utf-8"))
            for section in parsed.sections():
                for key in ("IR/Total Yaw", "IR/Total Pitch", "IR/Hide"):
                    parsed.remove_option(section, key)
            previous[name] = ini({section: dict(parsed[section]) for section in parsed.sections()})
    previous["Config/Dolphin.ini"] = ini({"Core": {"EnableCheats": "False"},
        "Analytics": {"Enabled": "False", "PermissionAsked": "True"}, "AutoUpdate": {"UpdateTrack": ""}})
    initial = dict(previous)
    initial["Config/Dolphin.ini"] = ini({"Core": {"EnableCheats": "False"}})
    return (legacy, previous, initial)


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


def envelope(mode, files, labels=None, games=None):
    result = {"schema": 1, "mode": mode, "source_pin": PIN, "games": profile_games(games),
            "files": {name: digest(data) for name, data in files.items()}}
    if labels is not None:
        result["product_labels"] = product_snapshot(labels)
    return result


def admit(user, root=ROOT):
    """Admit only known generated manifests and safe regular config paths.

    Runtime-edited config bytes are allowed here; nothing is written/read from
    those configs. The initial delivered template is explicitly recognized too.
    """
    user = safe_user(user, root)
    manifest_path = safe_user(user / MANIFEST, root)
    manifest = json.loads(manifest_path.read_text(encoding="utf-8"))
    if not isinstance(manifest, dict) or not isinstance(manifest.get("mode"), str):
        raise ValueError("Invalid profile manifest")
    mode = manifest["mode"]
    if "product_labels" in manifest:
        labels = product_snapshot(manifest["product_labels"])
        games = profile_games(manifest.get("games"))
        expected = render(mode, labels, games=games)
        if manifest != envelope(mode, expected, labels, games):
            # Existing immutable profiles retain their recorded bytes. Admit
            # only the exact former template; use fresh preparation for the fix.
            expected = _pre_canonical_dsu_entries(expected, labels["dsu_server_name"])
            if manifest != envelope(mode, expected, labels, games):
                raise ValueError("Profile manifest has unexpected contents")
    else:
        for expected in legacy_templates(mode):
            if manifest == envelope(mode, expected):
                break
        else:
            raise ValueError("Profile manifest has unexpected contents")
    for name in expected:
        path = safe_user(user / name, root)
        if not path.is_file():
            raise ValueError(f"Managed configuration missing: {name}")
    return manifest, expected


def check(user, root=ROOT):
    manifest, expected = admit(user, root)
    user = safe_user(user, root)
    for name, data in expected.items():
        path = safe_user(user / name, root)
        if path.read_bytes() != data:
            raise ValueError(f"Generated configuration changed: {name}")
    return manifest


def create(user, mode, root=ROOT, game_id=None):
    user = safe_user(user, root)
    if game_id is not None and not valid_id(game_id):
        raise ValueError("Expected six-character selected game ID")
    if user.exists():
        manifest = check(user, root)
        if manifest["mode"] != mode or game_id is not None and manifest["games"] != [game_id]:
            raise ValueError("Use a fresh --user directory to change mode or selected game")
        return user
    labels = load_product(ROOT / "data/product.json")
    games = [game_id] if game_id is not None else None
    files = render(mode, labels, games=games)
    user.parent.mkdir(parents=True, exist_ok=True)
    stage = Path(tempfile.mkdtemp(prefix=".profile-stage-", dir=user.parent))
    try:
        for name, data in files.items():
            target = stage / name
            target.parent.mkdir(parents=True, exist_ok=True)
            target.write_bytes(data)
        (stage / MANIFEST).write_text(json.dumps(envelope(mode, files, labels, games), indent=2)+"\n", encoding="utf-8")
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
    admit(user, root)
    args = [str(Path(dolphin).resolve()), "--user", str(safe_user(user, root))]
    if game_file:
        args += ["-b", "-e", str(Path(game_file).resolve())]
    return args


def main(argv=None, root=ROOT):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("action", choices=("create", "inspect", "command"))
    parser.add_argument("--user", required=True, type=Path)
    parser.add_argument("--mode", choices=("one-gun", "two-remotes"), default="one-gun")
    parser.add_argument("--dolphin", type=Path)
    parser.add_argument("--game-file", type=Path)
    args = parser.parse_args(argv)
    try:
        if args.action == "create":
            create(args.user, args.mode, root)
        if args.action == "command":
            if args.dolphin is None:
                parser.error("command requires --dolphin")
            argv = command(args.user, args.dolphin, args.game_file, root)
            print(json.dumps(argv))
            print("& " + " ".join("'"+s.replace("'", "''")+"'" for s in argv))
        else:
            manifest = check(args.user, root)
            print(json.dumps(manifest, indent=2))
            print("Right hand: fire trigger, interact primary; absolute aim pointer.")
            print("Remote + uses DSU Options; Touch application-menu input is unavailable, so its usable mapping is pending.")
            print("Nunchuk: stick, grip Z, secondary C; physical acceleration, no simulated Shake/Swing.")
            print("one-gun: right Remote + left Nunchuk; two-remotes: each hand owns both attachments.")
            print("Side-by-side 3D theatre requires an SBS viewer. True 3D and live acceptance remain separate.")
        return 0
    except (OSError, ValueError, KeyError) as error:
        parser.exit(1, f"Profile error: {error}\n")


if __name__ == "__main__":
    raise SystemExit(main())
