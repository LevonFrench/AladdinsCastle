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
    text = re.sub(r"\[(?:with Guncon ?2?|Only Soft|PlayStation ?2 the Best|PlayStation 2 BigHit Series|本体同梱版)\]", "", text, flags=re.I)
    # The database uses both II and 2 for the same numbered release.
    text = re.sub(r"\bII\b", "2", text)
    return "".join(c for c in unicodedata.normalize("NFKC", text).casefold() if c.isalnum())


def nonretail(text):
    # Check every language label on a record before allowing its English alias.
    return any(label in text for label in ("体験版", "試遊", "リプレイ", "実験", "店頭")) or bool(
        re.search(r"\b(?:demo|trial|replay|prototype|beta|sample)\b|\btest (?:disc|version)|[\[(]test[\])]|\btest$", text, re.I))


def extract(source_text, titles):
    records = {}
    serial = None
    for line in source_text.splitlines():
        m = re.fullmatch(r"([A-Z0-9]+-[A-Z0-9]+):", line)
        if m:
            serial = m[1]
            records.setdefault(serial, [])
        m = re.fullmatch(r'  (?:name|name-en): "((?:[^"\\]|\\.)*)"', line)
        if m and serial:
            try:
                records[serial].append(json.loads('"' + m[1] + '"'))
            except ValueError:
                continue
    result = {}
    for serial, names in records.items():
        if any(nonretail(title) for title in names):
            continue
        matches = [(title, titles.get(key(title), set())) for title in names]
        ids = set().union(*(ids for _, ids in matches))
        if len(ids) == 1:
            title = next(title for title, matched in reversed(matches) if matched)
            result[serial] = {"gameId": next(iter(ids)), "title": title, "source": SOURCE}
    return result


def self_test():
    import unittest

    class SerialFacts(unittest.TestCase):
        def test_cjk_preserved(self):
            self.assertNotEqual(key("Synthetic 体験版"), key("Synthetic"))
            self.assertEqual(key("Synthetic II"), key("Synthetic 2"))
            self.assertEqual(key("遊戲"), "遊戲")

        def test_nonretail_all_names(self):
            titles = {key("Synthetic"): {"ps2-synthetic"}}
            for label in ("体験版", "試遊", "リプレイ", "実験", "Store Demo", "Trial", "Replay", "Test"):
                source = f'SYNTH-00001:\n  name: "Synthetic {label}"\n  name-en: "Synthetic"\n'
                self.assertEqual(extract(source, titles), {})

        def test_retail_and_ambiguity(self):
            source = 'SYNTH-00002:\n  name: "Synthetic [with Guncon2]"\n'
            self.assertEqual(extract(source, {key("Synthetic"): {"ps2-synthetic"}})["SYNTH-00002"]["gameId"], "ps2-synthetic")
            self.assertEqual(extract(source, {key("Synthetic"): {"a", "b"}}), {})
            source = 'SYNTH-00003:\n  name: "Test Drive"\n'
            self.assertIn("SYNTH-00003", extract(source, {key("Test Drive"): {"ps2-synthetic"}}))
            source = 'SYNTH-00004:\n  name: "Synthetic [本体同梱版]"\n'
            self.assertIn("SYNTH-00004", extract(source, {key("Synthetic"): {"ps2-synthetic"}}))

    return 0 if unittest.TextTestRunner().run(unittest.defaultTestLoader.loadTestsFromTestCase(SerialFacts)).wasSuccessful() else 1


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
    result = extract(args.source.read_text(encoding="utf-8"), titles)
    output = args.output or args.root / "hub/resources/serial-index.json"
    output.parent.mkdir(parents=True, exist_ok=True)
    output.write_text(json.dumps(result, indent=2, ensure_ascii=False) + "\n", encoding="utf-8")
    print(f"{len(result)} verified public serial/title mappings")


if __name__ == "__main__":
    import sys
    if sys.argv[1:] == ["--test"]:
        sys.exit(self_test())
    main()
