"""Validate games/*/game.toml against docs/game-schema.md and data/vocab/*.toml.

Usage: python tools/validate_catalog.py [--summary]
Exit code 1 if any error. Warnings don't fail.
"""
import argparse
import collections
import glob
import json
import os
import re
import sys
import tomllib

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
REQUIRED = ["id", "title", "genre", "year", "manufacturer", "hardware", "graphics"]
PREFIXES = {"sony-ps1": "ps1-", "sony-ps2": "ps2-", "sony-ps3": "ps3-", "sega-saturn": "sat-",
            "sega-dreamcast": "dc-", "nintendo-64": "n64-", "nintendo-gamecube": "gc-",
            "nintendo-wii": "wii-", "nintendo-wii-u": "wiiu-", "nintendo-switch": "switch-",
            "microsoft-xbox": "xbox-", "microsoft-xbox-360": "x360-", "pc-windows": "pc-", "nintendo-3ds": "3ds-", "nintendo-ds": "nds-",
            "sega-genesis": "md-", "sega-cd": "scd-", "super-nes": "snes-", "sega-master-system": "sms-", "sega-game-gear": "gg-"}
ROUTE_STATUS = {"working", "imperfect", "not-working", "profile", "playable", "unknown"}


def load(path):
    with open(path, "rb") as f:
        return tomllib.load(f)


def media_requirement_id(item, game_id, row):
    if isinstance(item, dict):
        for key in ("set", "serial", "id"):
            if isinstance(item.get(key), str) and item[key]:
                return item[key]
    return f"{game_id}-media-{row}"


def media_errors(game, recipe=None):
    errors = []
    media = game.get("media", [])
    if not isinstance(media, list):
        return ["media must be an array of tables"]
    ids = set()
    for row, item in enumerate(media):
        if not isinstance(item, dict):
            errors.append("media item must be a table")
            continue
        if "optional" in item and not isinstance(item["optional"], bool):
            errors.append("media.optional must be boolean")
        for key in ("set", "serial", "id"):
            if key in item and not isinstance(item[key], str):
                errors.append(f"media.{key} must be text")
        requirement = media_requirement_id(item, game.get("id", ""), row)
        if requirement in ids:
            errors.append(f"duplicate media requirement: {requirement}")
        ids.add(requirement)
    variants = (recipe or {}).get("variant", {})
    if not isinstance(variants, dict):
        return errors + ["variant must be a table"]
    for name, variant in variants.items():
        if not isinstance(variant, dict):
            errors.append(f"variant.{name} must be a table")
            continue
        needs_table = variant.get("needs", {})
        if not isinstance(needs_table, dict):
            errors.append(f"variant.{name}.needs must be a table")
            continue
        needs = needs_table.get("media", [])
        if not isinstance(needs, list) or any(not isinstance(item, str) for item in needs):
            errors.append("needs.media must be an array of text")
            continue
        for requirement in needs:
            if requirement not in ids:
                errors.append(f"unresolved needs.media: {requirement}")
    return errors


def controls_errors(game):
    return ["controls must be a table"] if "controls" in game and not isinstance(game["controls"], dict) else []


def self_test():
    import unittest

    class MediaContract(unittest.TestCase):
        def test_optional_boolean(self):
            for value in (True, False):
                self.assertEqual(media_errors({"media": [{"kind": "bios", "optional": value}]}), [])
            for value in ("true", 1, None):
                self.assertTrue(media_errors({"media": [{"optional": value}]}))

        def test_recipe_resolution(self):
            game = {"id": "synthetic", "media": [{"set": "set", "serial": "ignored", "id": "ignored"},
                    {"serial": "TEST-00003", "id": "ignored"}, {"id": "disc"}, {}]}
            ids = ["set", "TEST-00003", "disc", "synthetic-media-3"]
            recipe = {"variant": {"fixture": {"needs": {"media": ids}}}}
            self.assertEqual(media_errors(game, recipe), [])
            recipe["variant"]["fixture"]["needs"]["media"] = ["missing"]
            self.assertEqual(media_errors(game, recipe), ["unresolved needs.media: missing"])

        def test_malformed_contract(self):
            for game, recipe in [({"media": "bad"}, {}), ({"media": ["bad"]}, {}),
                    ({"media": [{"id": 1}]}, {}), ({"media": [{"id": "same"}, {"id": "same"}]}, {}),
                    ({}, {"variant": {"fixture": {"needs": {"media": "bad"}}}}),
                    ({}, {"variant": {"fixture": {"needs": {"media": [1]}}}})]:
                self.assertTrue(media_errors(game, recipe))

        def test_non_table_recipe_and_controls(self):
            for recipe, expected in [({"variant": "bad"}, "variant must be a table"),
                    ({"variant": {"synthetic": "bad"}}, "variant.synthetic must be a table"),
                    ({"variant": {"synthetic": {"needs": "bad"}}}, "variant.synthetic.needs must be a table")]:
                self.assertEqual(media_errors({}, recipe), [expected])
            for value in ("bad", ["bad"], 1):
                self.assertEqual(controls_errors({"controls": value}), ["controls must be a table"])
            self.assertEqual(controls_errors({"controls": {"type": "gun"}}), [])

    return 0 if unittest.TextTestRunner().run(unittest.defaultTestLoader.loadTestsFromTestCase(MediaContract)).wasSuccessful() else 1


def main(root=ROOT, json_report=False):
    genres = load(os.path.join(root, "data/vocab/genres.toml"))
    makers = load(os.path.join(root, "data/vocab/manufacturers.toml"))
    hardware = load(os.path.join(root, "data/vocab/hardware.toml"))
    graphics = load(os.path.join(root, "data/vocab/graphics.toml"))
    errors, warnings, games = [], [], []

    for path in sorted(glob.glob(os.path.join(root, "games/*/game.toml"))):
        folder = os.path.basename(os.path.dirname(path))
        rel = os.path.relpath(path, root).replace("\\", "/")
        try:
            g = load(path)
        except Exception as e:  # noqa: BLE001
            errors.append(f"{rel}: TOML error: {e}")
            continue
        games.append(g)
        recipe_path = os.path.join(os.path.dirname(path), "install.toml")
        try:
            recipe = load(recipe_path) if os.path.isfile(recipe_path) else {}
        except Exception as e:
            errors.append(f"{rel}: install TOML error: {e}")
            recipe = {}
        errors.extend(f"{rel}: {message}" for message in media_errors(g, recipe))
        errors.extend(f"{rel}: {message}" for message in controls_errors(g))
        for k in REQUIRED:
            if k not in g:
                errors.append(f"{rel}: missing required field '{k}'")
        for k in ("id", "title", "genre", "manufacturer", "hardware", "graphics"):
            if k in g and not isinstance(g[k], str):
                errors.append(f"{rel}: {k} must be text")
        if g.get("id") != folder:
            errors.append(f"{rel}: id '{g.get('id')}' does not match folder '{folder}'")
        if not re.fullmatch(r"[a-z0-9][a-z0-9-]*", folder):
            errors.append(f"{rel}: folder/id must be lowercase kebab-case")
        if g.get("genre") not in genres.get("genre", {}):
            errors.append(f"{rel}: genre '{g.get('genre')}' not in vocab")
        for sg in g.get("subgenre", []):
            s = genres.get("subgenre", {}).get(sg)
            if not s:
                errors.append(f"{rel}: subgenre '{sg}' not in vocab")
            elif s.get("genre") != g.get("genre"):
                warnings.append(f"{rel}: subgenre '{sg}' belongs to genre '{s.get('genre')}'")
        if g.get("manufacturer") not in makers:
            errors.append(f"{rel}: manufacturer '{g.get('manufacturer')}' not in vocab")
        if g.get("graphics") not in graphics:
            errors.append(f"{rel}: graphics '{g.get('graphics')}' not in vocab")
        hw = hardware.get(g.get("hardware"))
        if not hw:
            errors.append(f"{rel}: hardware '{g.get('hardware')}' not in vocab")
        else:
            prefix = PREFIXES.get(g["hardware"])
            if prefix and not folder.startswith(prefix):
                warnings.append(f"{rel}: {g['hardware']} entries should use id prefix '{prefix}'")
            if hw.get("kind") == "arcade" and any(folder.startswith(p) for p in PREFIXES.values()):
                warnings.append(f"{rel}: arcade entry uses a console id prefix")
        y = g.get("year")
        if type(y) is not int or not 1970 <= y <= 2026:
            errors.append(f"{rel}: year '{y}' is not a plausible integer year")
        for key, val in g.get("routes", {}).items():
            if key == "vr":
                best = val.get("best") if isinstance(val, dict) else None
                if best not in {"true3d", "theatre", "none"}:
                    warnings.append(f"{rel}: routes.vr.best '{best}' should be true3d|theatre|none")
            elif isinstance(val, str) and val not in ROUTE_STATUS:
                warnings.append(f"{rel}: routes.{key} status '{val}' not in {sorted(ROUTE_STATUS)}")
        hub = g.get("hub", {})
        for k in ("pill", "colour", "accent", "blurb"):
            if k not in hub:
                warnings.append(f"{rel}: hub.{k} missing")
        if "pill" in hub and len(hub["pill"]) > 12:
            warnings.append(f"{rel}: hub.pill longer than 12 chars")
        for k in ("colour", "accent"):
            if k in hub and not re.fullmatch(r"#[0-9a-fA-F]{6}", hub[k]):
                warnings.append(f"{rel}: hub.{k} '{hub[k]}' is not #rrggbb")
        if not g.get("meta", {}).get("sources"):
            warnings.append(f"{rel}: meta.sources empty")
        if type(g.get("players")) is not int or g["players"] < 1:
            warnings.append(f"{rel}: players missing or invalid")
        controls = g.get("controls", {}).get("type", "") if isinstance(g.get("controls", {}), dict) else ""
        if controls and controls not in {"gun", "wheel", "handlebars", "bike", "ski", "joystick", "yoke", "boat", "other"}:
            warnings.append(f"{rel}: controls.type unknown: {controls}")
        orig = g.get("original")
        if orig and not os.path.isdir(os.path.join(root, "games", orig)):
            warnings.append(f"{rel}: original '{orig}' has no games/{orig} folder (yet)")

    if json_report:
        print(json.dumps({"records": len(games), "errors": errors, "warnings": warnings}))
        return 1 if errors else 0
    for e in errors:
        print("ERROR  ", e)
    for w in warnings:
        print("warning", w)
    print(f"\n{len(games)} games, {len(errors)} errors, {len(warnings)} warnings")
    if "--summary" in sys.argv:
        for field in ("genre", "graphics", "manufacturer", "hardware"):
            c = collections.Counter(g.get(field) for g in games)
            print(f"\nby {field}: " + ", ".join(f"{k} {v}" for k, v in c.most_common()))
        dec = collections.Counter((g.get("year", 0) // 10) * 10 for g in games if isinstance(g.get("year"), int))
        print("\nby decade: " + ", ".join(f"{k}s {v}" for k, v in sorted(dec.items())))
    return 1 if errors else 0


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--test", action="store_true")
    parser.add_argument("--summary", action="store_true")
    parser.add_argument("--root", default=ROOT, help="Base catalog root (packs/user overrides are runtime-only)")
    parser.add_argument("--json", action="store_true", help="Machine-readable base-catalog report")
    args = parser.parse_args()
    sys.exit(self_test() if args.test else main(args.root, args.json))
