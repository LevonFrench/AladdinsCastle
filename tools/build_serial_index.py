#!/usr/bin/env python3
"""Extract serial/title facts for exact catalog titles from pinned public PCSX2 metadata.

No network calls. Supply the official GameIndex.yaml from the source URL below.
No YAML code execution or emulator settings are copied.
"""
import argparse
import json
import pathlib
import re
import tomllib
import unicodedata

SOURCE = "https://github.com/PCSX2/pcsx2/blob/aa7ab4306e269075784c7ac3eb4b45e6e6c53445/bin/resources/GameIndex.yaml"


def key(text):
    # Recognized retail bundle/reprint annotations do not change the game.
    # Keep demos/trials/disc-number annotations, which identify different media.
    text = re.sub(r"\[(?:with Guncon ?2?|Only Soft|PlayStation ?2 the Best|PlayStation 2 BigHit Series)\]", "", text, flags=re.I)
    # The database uses both II and 2 for the same numbered release.
    text = re.sub(r"\bII\b", "2", text)
    return re.sub(r"[^a-z0-9]", "", unicodedata.normalize("NFKD", text).lower())


def main():
    p = argparse.ArgumentParser()
    p.add_argument("source", type=pathlib.Path)
    p.add_argument("--root", type=pathlib.Path, default=pathlib.Path(__file__).resolve().parents[1])
    p.add_argument("--output", type=pathlib.Path)
    args = p.parse_args()
    titles = {}
    for path in sorted((args.root / "games").glob("*/game.toml")):
        game = tomllib.loads(path.read_text(encoding="utf-8"))
        if game.get("hardware") != "sony-ps2":
            continue
        for title in [game["title"], *game.get("alt_titles", [])]:
            titles.setdefault(key(title), set()).add(game["id"])
    result = {}
    serial = None
    for line in args.source.read_text(encoding="utf-8").splitlines():
        m = re.fullmatch(r"([A-Z0-9]+-[A-Z0-9]+):", line)
        if m:
            serial = m[1]
        m = re.fullmatch(r'  (?:name|name-en): "((?:[^"\\]|\\.)*)"', line)
        if m and serial:
            try:
                title = json.loads('"' + m[1] + '"')
            except ValueError:
                continue
            ids = titles.get(key(title), set())
            if len(ids) == 1:
                result[serial] = {"gameId": next(iter(ids)), "title": title, "source": SOURCE}
    output = args.output or args.root / "hub/resources/serial-index.json"
    output.parent.mkdir(parents=True, exist_ok=True)
    output.write_text(json.dumps(result, indent=2, ensure_ascii=False) + "\n", encoding="utf-8")
    print(f"{len(result)} verified public serial/title mappings")


if __name__ == "__main__":
    main()
