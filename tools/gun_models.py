#!/usr/bin/env python3
"""Resolve the gun model and two-gun support of every gun game.

Usage:
  python tools/gun_models.py            check the data, print a summary
  python tools/gun_models.py --table    also print one line per game
  python tools/gun_models.py --markdown print the per-model table used in docs/guns.md

Rules: docs/guns.md. Data: data/guns/*.toml, data/guns/defaults.toml, games/*/game.toml.
Exit code 1 on any error.
"""
import glob
import os
import re
import sys
import tomllib

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
HOLDS = {"one-hand", "two-hand", "mounted"}
FAMILIES = {"generic", "console", "arcade", "mounted", "special"}


def load(path):
    with open(path, "rb") as f:
        return tomllib.load(f)


def as_list(value):
    return value if isinstance(value, list) else [value]


def rule_matches(match, game, hardware_kind):
    controls = game.get("controls", {})
    for key, want in match.items():
        if key == "id":
            if game["id"] not in as_list(want):
                return False
        elif key in ("hardware", "manufacturer"):
            if game.get(key) not in as_list(want):
                return False
        elif key == "kind":
            if hardware_kind not in as_list(want):
                return False
        elif key == "gun":
            if not re.search(want, controls.get("gun", ""), re.IGNORECASE):
                return False
        else:
            raise ValueError(f"unknown match key '{key}'")
    return True


def resolve_model(game, games, models, defaults, hardware, depth=0):
    """Shared rule resolver; returns (model id, provenance, shape-review flag)."""
    explicit = game.get('controls', {}).get('gun_model')
    if explicit:
        if explicit not in models:
            raise ValueError(f"games/{game['id']}: unknown gun_model '{explicit}'")
        return explicit, 'game.toml', False
    kind = hardware.get(game.get('hardware'), {}).get('kind')
    for index, rule in enumerate(defaults.get('rule', [])):
        if rule_matches(rule.get('match', {}), game, kind):
            if rule.get('model') not in models:
                raise ValueError(f'defaults.toml rule {index+1}: unknown model')
            return rule['model'], f'rule {index+1}', bool(rule.get('review'))
    if game.get('hardware') in defaults.get('inherit_original', []) and depth == 0:
        original = games.get(game.get('original', ''))
        if original:
            model, _, review = resolve_model(original, games, models, defaults, hardware, 1)
            return model, f"original {original['id']}", review
    if defaults.get('fallback') not in models:
        raise ValueError('defaults.toml: fallback is not a model')
    return defaults['fallback'], 'fallback', True


def main():
    errors = []
    models = {}
    for path in sorted(glob.glob(os.path.join(ROOT, "data", "guns", "*.toml"))):
        stem = os.path.splitext(os.path.basename(path))[0]
        if stem == "defaults":
            continue
        m = load(path)
        if m.get("id") != stem:
            errors.append(f"{path}: id must equal the file name")
        if m.get("hold") not in HOLDS:
            errors.append(f"{path}: hold must be one of {sorted(HOLDS)}")
        if m.get("family") not in FAMILIES:
            errors.append(f"{path}: family must be one of {sorted(FAMILIES)}")
        tints = m.get("tints", {})
        for key in ("default", "p1", "p2"):
            if tints.get(key) not in tints:
                errors.append(f"{path}: tints.{key} must name a tint in the same table")
        seen = set()
        for b in m.get("button", []):
            if b.get("id") in seen:
                errors.append(f"{path}: duplicate button id {b.get('id')}")
            seen.add(b.get("id"))
        models[stem] = m

    defaults = load(os.path.join(ROOT, "data", "guns", "defaults.toml"))
    hardware = load(os.path.join(ROOT, "data", "vocab", "hardware.toml"))
    rules = defaults.get("rule", [])
    for i, r in enumerate(rules):
        if r.get("model") not in models:
            errors.append(f"defaults.toml rule {i + 1}: unknown model '{r.get('model')}'")
    if defaults.get("fallback") not in models:
        errors.append("defaults.toml: fallback is not a model")

    games = {}
    for path in sorted(glob.glob(os.path.join(ROOT, "games", "*", "game.toml"))):
        g = load(path)
        games[g["id"]] = g

    rule_ids = set()
    for r in rules:
        rule_ids.update(as_list(r.get("match", {}).get("id", [])))
    separate = set(defaults.get("two_guns", {}).get("separate_views", []))
    for gid in sorted((rule_ids | separate) - set(games)):
        errors.append(f"defaults.toml: game id '{gid}' is not in the catalog")

    def resolve(game, depth=0):
        try:
            return resolve_model(game, games, models, defaults, hardware, depth)
        except ValueError as exc:
            errors.append(str(exc))
            return defaults.get('fallback'), 'invalid rules', True

    rows = []
    for gid, g in games.items():
        if g.get("genre") != "gun":
            continue
        model, why, review = resolve(g)
        controls = g.get("controls", {})
        guns = controls.get("guns") or 1
        if guns < 2 or (g.get("players") or 1) < 2:
            two = "no"
        elif gid in separate:
            two = "separate-views"
        else:
            two = "yes"
        rows.append((gid, model, why, review, two, guns))

    if "--table" in sys.argv:
        for gid, model, why, review, two, guns in rows:
            flag = " REVIEW" if review else ""
            print(f"{gid:48} {model:22} two_guns={two:15} ({why}){flag}")
        print()

    by_model = {}
    for gid, model, why, review, two, guns in rows:
        by_model.setdefault(model, []).append(gid)
    if "--markdown" in sys.argv:
        print("| Model | Tier | Hold | Games | Resembles |")
        print("|---|---|---|---|---|")
        for mid, m in sorted(models.items(), key=lambda kv: (kv[1].get("tier", 9), kv[0])):
            print(f"| `{mid}` | {m.get('tier')} | {m.get('hold')} | {len(by_model.get(mid, []))} | {m.get('resembles')} |")
        print()

    two_yes = sum(1 for r in rows if r[4] == "yes")
    two_sep = sum(1 for r in rows if r[4] == "separate-views")
    reviews = sum(1 for r in rows if r[3])
    unused = sorted(set(models) - set(by_model))
    print(f"{len(models)} gun models, {len(rules)} rules, {len(rows)} gun games")
    print(f"eligible for a gun in each hand: {two_yes} games; separate views: {two_sep}; one gun only: {len(rows) - two_yes - two_sep}")
    print("(eligibility comes from the catalog; whether an installed route can take two guns is a separate check)")
    print(f"assignments to review: {reviews}")
    if unused:
        print("models no game uses yet: " + ", ".join(unused))
    for e in errors:
        print("error " + e)
    print(f"{len(errors)} errors")
    return 1 if errors else 0


if __name__ == "__main__":
    sys.exit(main())
