"""Copy the project's own 3D models into website/models/ and describe them for the site.

Run from anywhere:  python website/tools/sync_models.py

Only original CC0 models are copied (assets/guns, assets/controls, assets/hands). The
CC BY arc-light-gun is left out so the site needs no extra credit line. models.json carries
each model's name, size, default tints and moving parts, read from data/guns/*.toml, plus the
catalog size from games/*/game.toml, so the site follows the project as it grows.
"""
from __future__ import annotations

import json
import shutil
import sys
import tomllib
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
OUT = ROOT / "website" / "models"

GUNS = [
    "arc-pistol-slide", "arc-pistol-twin", "generic-pistol", "funcon", "con-pistol-dpad",
    "con-pistol-slim", "con-revolver-chunky", "con-remote-shell", "con-stick-grip",
    "con-pointer-remote", "mnt-mg-heavy",
]
CONTROLS = {
    "wheel-three-spoke": "Three-spoke steering wheel",
    "shifter-sequential": "Sequential shifter",
    "shifter-four-gate": "Four-gate shifter",
    "pedals-dual": "Accelerator and brake pedals",
    "bars-motorcycle": "Motorcycle handlebars",
    "stick-arcade": "Arcade joystick",
}
HANDS = {"grip-hand-right": "Right grip hand", "grip-hand-left": "Left grip hand"}


def gun_entry(gun_id: str) -> dict:
    data = tomllib.loads((ROOT / "data" / "guns" / f"{gun_id}.toml").read_text(encoding="utf-8"))
    tints = data.get("tints", {})
    default = tints.get(tints.get("default", ""), {}) if isinstance(tints.get("default"), str) else {}
    motion = {}
    for name, m in data.get("motion", {}).items():
        if isinstance(m, dict) and "node" in m:
            motion[name] = {k: m[k] for k in ("node", "kind", "axis", "range", "drive", "lod_nodes") if k in m}
    assembly = json.loads((ROOT / "assets" / "hands" / "assembly.json").read_text(encoding="utf-8"))["models"].get(gun_id, {})
    hands = {side: {k: h[k] for k in ("node", "translation", "rotation_xyzw") if k in h}
             for side, h in assembly.get("hands", {}).items()}
    return {
        "id": gun_id,
        "kind": "controller" if data.get("asset_kind") == "controller" else "gun",
        "name": data.get("name", gun_id),
        "family": data.get("family", ""),
        "hold": data.get("hold", ""),
        "shape": data.get("shape", ""),
        "length_mm": data.get("length_mm"),
        "tint": {"body": default.get("body"), "accent": default.get("accent")},
        "motion": motion,
        "hands": hands,  # where each grip hand attaches (assets/hands/assembly.json)
        "licence": "CC0-1.0",
        "file": f"models/{gun_id}.glb",
    }


def main() -> int:
    OUT.mkdir(parents=True, exist_ok=True)
    entries = []
    for gun in GUNS:
        shutil.copy2(ROOT / "assets" / "guns" / f"{gun}.glb", OUT / f"{gun}.glb")
        entries.append(gun_entry(gun))
    for cid, name in CONTROLS.items():
        shutil.copy2(ROOT / "assets" / "controls" / f"{cid}.glb", OUT / f"{cid}.glb")
        entries.append({"id": cid, "kind": "control", "name": name, "licence": "CC0-1.0", "file": f"models/{cid}.glb"})
    for hid, name in HANDS.items():
        shutil.copy2(ROOT / "assets" / "hands" / f"{hid}.glb", OUT / f"{hid}.glb")
        hand = tomllib.loads((ROOT / "assets" / "hands" / f"{hid}.toml").read_text(encoding="utf-8"))
        tints = hand.get("tints", {})
        glove = tints.get(tints.get("default", ""), {}) if isinstance(tints.get("default"), str) else {}
        entries.append({"id": hid, "kind": "hand", "name": name, "licence": "CC0-1.0", "file": f"models/{hid}.glb",
                        "tint": {"body": glove.get("body"), "accent": glove.get("accent")}})
    catalog = sum(1 for _ in (ROOT / "games").glob("*/game.toml"))
    (OUT / "models.json").write_text(json.dumps({"catalog_games": catalog, "models": entries}, indent=1) + "\n", encoding="utf-8")
    size = sum(p.stat().st_size for p in OUT.glob("*.glb"))
    print(f"{len(entries)} models, {size / 1e6:.1f} MB, catalog {catalog} games -> {OUT}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
