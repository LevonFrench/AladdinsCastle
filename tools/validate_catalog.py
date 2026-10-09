"""Validate games/*/game.toml against docs/game-schema.md and data/vocab/*.toml.

Usage: python tools/validate_catalog.py [--summary]
Exit code 1 if any error. Warnings don't fail.
"""
import collections
import glob
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


def main():
    genres = load(os.path.join(ROOT, "data/vocab/genres.toml"))
    makers = load(os.path.join(ROOT, "data/vocab/manufacturers.toml"))
    hardware = load(os.path.join(ROOT, "data/vocab/hardware.toml"))
    graphics = load(os.path.join(ROOT, "data/vocab/graphics.toml"))
    errors, warnings, games = [], [], []

    for path in sorted(glob.glob(os.path.join(ROOT, "games/*/game.toml"))):
        folder = os.path.basename(os.path.dirname(path))
        rel = os.path.relpath(path, ROOT).replace("\\", "/")
        try:
            g = load(path)
        except Exception as e:  # noqa: BLE001
            errors.append(f"{rel}: TOML error: {e}")
            continue
        games.append(g)
        for k in REQUIRED:
            if k not in g:
                errors.append(f"{rel}: missing required field '{k}'")
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
        if not isinstance(y, int) or not 1970 <= y <= 2026:
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
        orig = g.get("original")
        if orig and not os.path.isdir(os.path.join(ROOT, "games", orig)):
            warnings.append(f"{rel}: original '{orig}' has no games/{orig} folder (yet)")

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
    sys.exit(main())
