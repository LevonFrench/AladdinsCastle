# SPDX-License-Identifier: GPL-3.0-or-later
"""Checks that need no compiler: the host's settings registry (host/HostConfig.h) against the code that reads
the settings, and every profile in profiles/ against the registry and the catalog.

    python setups/retro-ps1-vr/test_host_profiles.py

The reader itself is checked by host/HostConfigTest.cpp (it needs g++).
"""
import os
import re
import sys
import tomllib
import unittest

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(os.path.dirname(HERE))
HOST = os.path.join(HERE, "host")
PROFILES = os.path.join(HERE, "profiles")

ROW = re.compile(r'\{"(ACVR_[A-Z0-9_]+)",\s*Type::(\w+),\s*(-?\d+),\s*"((?:[^"\\]|\\.)*)",\s*([^,]+?),\s*(true|false),\s*(true|false),')
TOP = {"schema", "name", "system", "notes", "controls", "gun", "port", "default_view", "core_options", "settings", "pad", "views"}
VIEW = {"label", "evidence", "notes", "settings", "core_options", "pad"}
PAD = {"map": "ACVR_PAD_MAP", "wheel_deg": "ACVR_PAD_WHEEL_DEG", "aim_hold": "ACVR_PAD_AIM_HOLD", "aim_button": "ACVR_PAD_AIM_BUTTON",
       "aim": "ACVR_PAD_AIM", "aim_curve": "ACVR_PAD_AIM_CURVE", "aim_dot": "ACVR_PAD_AIM_DOT", "aim_on": "ACVR_PAD_AIM_ON",
       "follow_u": "ACVR_PAD_FOLLOW_U", "follow_v": "ACVR_PAD_FOLLOW_V", "follow_gain": "ACVR_PAD_FOLLOW_GAIN"}
SYSTEMS = {"ps1": 1, "saturn": 2, "n64": 4, "3ds": 8}
SYSTEM_BITS = {"kPs1": 1, "kSaturn": 2, "kN64": 4, "k3ds": 8, "kAll": 15}


def read(path):
    with open(path, encoding="utf-8") as f:
        return f.read()


def registry():
    rows = {}
    for m in ROW.finditer(read(os.path.join(HOST, "HostConfig.h"))):
        name, kind, count, default, systems, auto, core = m.groups()
        mask = 0
        for part in systems.split("|"):
            mask |= SYSTEM_BITS[part.strip()]
        rows[name] = dict(type=kind, count=int(count), default=default, systems=mask, auto=auto == "true", core=core == "true")
    return rows


def host_names():
    names = set()
    for f in os.listdir(HOST):
        if f.endswith((".cpp", ".h")) and f not in ("HostConfig.h", "HostConfigTest.cpp"):
            names |= set(re.findall(r"ACVR_[A-Z0-9]+(?:_[A-Z0-9]+)*", read(os.path.join(HOST, f))))
    return names


def patch_names():
    names = set()
    for f in os.listdir(HERE):
        if f.endswith(".patch"):
            names |= set(re.findall(r"ACVR_[A-Z0-9]+(?:_[A-Z0-9]+)*", read(os.path.join(HERE, f))))
    return names


# Profiles that are not Hub games yet (no catalog entry, no Hub row): started from the command line.
NOT_HUB_YET = {"3ds-kid-icarus-uprising.toml"}


def profile_files():
    return sorted(f for f in os.listdir(PROFILES) if f.endswith(".toml"))


class Registry(unittest.TestCase):
    def test_rows_parse_and_are_unique(self):
        text = read(os.path.join(HOST, "HostConfig.h"))
        listed = re.findall(r'^\s*\{"(ACVR_[A-Z0-9_]+)",', text, re.M)
        self.assertGreater(len(listed), 50)
        self.assertEqual(sorted(listed), sorted(registry()), "a registry row does not match the pattern this test reads")
        self.assertEqual(len(listed), len(set(listed)), "duplicate registry names")

    def test_every_name_the_host_mentions_is_registered(self):
        # Names other ACVR programs own never appear in the host's sources; a name here that is not in the registry is
        # a setting the host reads (or documents) that has no entry, so no typo check, no profile support.
        missing = host_names() - set(registry())
        self.assertEqual(sorted(missing), [], "ACVR_* names in host/ with no registry entry")

    def test_every_host_setting_is_still_used(self):
        used = host_names() | patch_names()
        stale = [n for n in registry() if n not in used]
        self.assertEqual(stale, [], "registry entries nothing reads or documents any more")

    def test_core_settings_are_in_the_patches(self):
        patches = patch_names()
        for name, row in registry().items():
            if row["core"]:
                self.assertIn(name, patches, name + " is marked as read by a patched core")


class Profiles(unittest.TestCase):
    def test_there_are_profiles(self):
        self.assertIn("3ds-star-fox-64-3d.toml", profile_files())

    def test_each_profile_fits_the_registry(self):
        rows = registry()
        for f in profile_files():
            path = os.path.join(PROFILES, f)
            text = read(path)
            with self.subTest(profile=f):
                # The host's reader takes a subset of TOML: nothing it would refuse may be in a tracked profile.
                self.assertNotIn('"""', text)
                self.assertNotIn("'''", text)
                self.assertIsNone(re.search(r"^\s*\[\[", text, re.M), "arrays of tables")
                self.assertIsNone(re.search(r"^\s*[A-Za-z0-9_-]+\.[A-Za-z0-9_.-]*\s*=", text, re.M), "dotted keys")
                self.assertIsNone(re.search(r"[A-Za-z]:\\|/Users/|/home/", text), "a path on one machine")
                data = tomllib.loads(text)
                self.assertEqual(data.get("schema"), 1)
                self.assertFalse(set(data) - TOP, "unknown top-level keys")
                system = SYSTEMS[data["system"]] if "system" in data else 0
                layers = [data] + [v for v in data.get("views", {}).values()]
                for layer in layers:
                    if layer is not data:
                        self.assertFalse(set(layer) - VIEW, "unknown view keys")
                    settings = dict(layer.get("settings", {}))
                    for key, value in layer.get("pad", {}).items():
                        self.assertIn(key, PAD)
                        settings[PAD[key]] = value
                    for name, value in settings.items():
                        self.assertIn(name, rows, name + " is not in the registry")
                        row = rows[name]
                        self.assertNotEqual(row["type"], "Path", name + " is a machine path; keep it in the launcher")
                        if system:
                            self.assertTrue(row["systems"] & system, name + " does not apply to " + data["system"])
                        if row["auto"] and value == "auto":
                            continue
                        kind = row["type"]
                        if kind == "Flag":
                            self.assertTrue(isinstance(value, bool) or value in (0, 1), name)
                        elif kind == "Float":
                            self.assertTrue(isinstance(value, (int, float)) and not isinstance(value, bool), name)
                        elif kind == "Int":
                            self.assertIsInstance(value, int, name)
                        elif kind == "Floats":
                            self.assertIsInstance(value, list, name)
                            n = len(value)
                            self.assertTrue(n == row["count"] if row["count"] > 0 else 1 <= n <= -row["count"], name)
                        elif kind == "Text":
                            self.assertTrue(isinstance(value, str) or (name == "ACVR_PAD_MAP" and isinstance(value, dict)), name)
                if "default_view" in data:
                    self.assertIn(data["default_view"], data.get("views", {}))

    def test_each_profile_has_a_catalog_entry(self):
        for f in profile_files():
            if f in NOT_HUB_YET:
                continue
            game = f[:-5]
            self.assertTrue(os.path.isfile(os.path.join(ROOT, "games", game, "game.toml")), f + " names a catalog id that does not exist")

    def test_star_fox_matches_the_documented_environment_path(self):
        data = tomllib.loads(read(os.path.join(PROFILES, "3ds-star-fox-64-3d.toml")))
        self.assertEqual(data["core_options"], {"citra_layout_option": "single_screen", "citra_render_3d": "side-by-side-full",
                                                "citra_factor_3d": 40, "citra_resolution_factor": 3})
        self.assertEqual(data["settings"]["ACVR_PS1_SBS"], 1)
        self.assertEqual(data["settings"]["ACVR_PS1_LETTERBOX"], "auto")
        self.assertEqual(",".join("%s=%s" % kv for kv in data["pad"]["map"].items()),
                         "rtrig=a,ra=y,rgrip=x,lgrip=b,ltrig=l,rb=r,rleft=l,rright=r,lclick=start")
        # README.md quotes the same map.
        self.assertIn("ACVR_PAD_MAP=rtrig=a,ra=y,rgrip=x,lgrip=b,ltrig=l,rb=r,rleft=l,rright=r,lclick=start", read(os.path.join(HERE, "README.md")))

    def test_catalog_entry_routes_to_theatre(self):
        for game in ("3ds-star-fox-64-3d", "3ds-mario-kart-7"):
            data = tomllib.loads(read(os.path.join(ROOT, "games", game, "game.toml")))
            self.assertEqual(data["routes"]["vr"]["best"], "theatre", game)

    def test_mario_kart_7_steers_with_the_stick(self):
        data = tomllib.loads(read(os.path.join(PROFILES, "3ds-mario-kart-7.toml")))
        self.assertEqual(data["system"], "3ds")
        self.assertEqual(data["controls"], "pad", "pad mode: the left stick is the Circle Pad; wheel mode is the roll-steering one")
        self.assertEqual(data["core_options"]["citra_factor_3d"], 40)
        pad = data["pad"]["map"]
        # A accelerate (right trigger), B brake (left grip), R hop and drift (right grip), L item, X look behind, Start on the left stick click.
        self.assertEqual(pad, {"rtrig": "a", "lgrip": "b", "rgrip": "r", "ltrig": "l", "ra": "l", "rb": "x", "lclick": "start"})
        # Roll steering is opt-in only: no profile turns it on, and the 3DS never asks for it.
        for layer in [data] + list(data.get("views", {}).values()):
            for name in ("ACVR_N64_ROLL_STEER", "ACVR_PAD_ROLL_STEER", "ACVR_PAD_WHEEL_DEG"):
                self.assertNotIn(name, layer.get("settings", {}))
            self.assertNotIn("wheel_deg", layer.get("pad", {}))
        self.assertIn("ACVR_PAD_MAP=" + ",".join("%s=%s" % kv for kv in pad.items()), read(os.path.join(HERE, "README.md")))

    def test_every_3ds_profile_is_a_hub_game(self):
        # The Hub lists the games it starts through acvr-ps1 in hub/src/core/libretro/Libretro.cpp, by catalog id.
        listed = set(re.findall(r'^\s*\{"(3ds-[a-z0-9-]+)","', read(os.path.join(ROOT, "hub", "src", "core", "libretro", "Libretro.cpp")), re.M))
        profiles = {f[:-5] for f in profile_files() if f.startswith("3ds-") and f not in NOT_HUB_YET}
        self.assertEqual(listed, profiles)
        for f in NOT_HUB_YET:
            self.assertNotIn(f[:-5], listed, f + " is a Hub game now: take it out of NOT_HUB_YET and give it a catalog entry")

    def test_kid_icarus_uses_the_touch_panel(self):
        data = tomllib.loads(read(os.path.join(PROFILES, "3ds-kid-icarus-uprising.toml")))
        self.assertEqual(data["system"], "3ds")
        self.assertEqual(data["controls"], "pad")
        s = data["settings"]
        self.assertEqual(s["ACVR_PS1_3DS_TOUCH"], 1)
        self.assertEqual(s["ACVR_PS1_SBS"], 1)
        # Both screens in one frame: the layout that has the bottom screen, with the top screen as a side-by-side pair.
        self.assertNotEqual(data["core_options"]["citra_layout_option"], "single_screen")
        self.assertEqual(data["core_options"]["citra_render_3d"], "side-by-side-full")
        for key in ("ACVR_PS1_3DS_TOP", "ACVR_PS1_3DS_BOTTOM"):
            x, y, w, h = s[key]
            self.assertTrue(0 <= x and 0 <= y and w > 0 and h > 0 and x + w <= 1.0001 and y + h <= 1.0001, key)
        pad = data["pad"]["map"]
        # The right trigger is the stylus, so it is not also a button; start is on the left stick click; roll steering is off.
        self.assertEqual(s["ACVR_PS1_3DS_STYLUS"], "rtrig")
        self.assertNotIn("rtrig", pad)
        self.assertEqual(pad["lclick"], "start")
        for layer in [data] + list(data.get("views", {}).values()):
            for name in ("ACVR_N64_ROLL_STEER", "ACVR_PAD_ROLL_STEER", "ACVR_PAD_WHEEL_DEG"):
                self.assertNotIn(name, layer.get("settings", {}))

    def test_touch_settings_are_for_the_3ds_only(self):
        rows = registry()
        for name in ("ACVR_PS1_3DS_TOUCH", "ACVR_PS1_3DS_TOP", "ACVR_PS1_3DS_BOTTOM", "ACVR_PS1_3DS_STYLUS", "ACVR_PS1_3DS_PANEL"):
            self.assertEqual(rows[name]["systems"], SYSTEM_BITS["k3ds"], name)
        # The stylus controls the host accepts are the ones the pad map knows (PadControls.h), so a typo is caught here too.
        pad_h = read(os.path.join(HOST, "PadControls.h"))
        known = re.search(r"known\[\] = \{([^}]*)\}", pad_h).group(1)
        self.assertIn('"rtrig"', known)


if __name__ == "__main__":
    sys.exit(0 if unittest.main(exit=False).result.wasSuccessful() else 1)
