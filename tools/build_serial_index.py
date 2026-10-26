#!/usr/bin/env python3
"""Refresh the PS2 rows of data/serial-index.json from pinned public PCSX2 metadata.

PS2 only. The PS1 rows (MAME psx.xml), the hand-curated rows and the other disc-index files
(data/wii-disc-index.json, data/dreamcast-disc-index.json) have no generator and stay hand
edited; see docs/SCANNER-ART.md and the data sources in docs/legal.md.

No network calls. Supply a local copy of the official GameIndex.yaml and the full commit it
was taken from (--commit); each generated row cites that exact commit. Only rows whose
source is a PCSX2 GameIndex URL are added or refreshed. PS1 rows and rows from any other
source are never touched, and extra fields such as "profile" are kept. Output keeps the
live schema (data/disc-index.schema.json): system, region, gameId, title, source.
No YAML code execution or emulator settings are copied.
"""
import argparse
import json
import os
import pathlib
import re
import sys
import tempfile
import tomllib
import unicodedata

# The oldest pin still cited by the catalog (games/ps2-time-crisis-2). The pin used for a refresh is --commit.
SOURCE = "https://github.com/PCSX2/pcsx2/blob/aa7ab4306e269075784c7ac3eb4b45e6e6c53445/bin/resources/GameIndex.yaml"
SOURCE_PREFIX = "https://github.com/PCSX2/pcsx2/blob/"
SOURCE_SUFFIX = "/bin/resources/GameIndex.yaml"


def source_url(commit):
    if not re.fullmatch(r"[0-9a-f]{40}", commit):
        raise ValueError("--commit must be the full 40-character commit the GameIndex.yaml was taken from")
    return SOURCE_PREFIX + commit + SOURCE_SUFFIX


def generated(row):
    # A row this script owns: a PCSX2 GameIndex row on the live schema. Hand-curated PS2 rows cite other pages.
    source = row.get("source", "")
    return row.get("system") == "ps2" and source.startswith(SOURCE_PREFIX) and source.endswith(SOURCE_SUFFIX)


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


def extract(source_text, titles, source=SOURCE):
    records = {}
    regions = {}
    serial = None
    for line in source_text.splitlines():
        m = re.fullmatch(r"([A-Z0-9]+-[A-Z0-9]+):", line)
        if m:
            serial = m[1]
            records.setdefault(serial, [])
        m = re.fullmatch(r'  region: "([^"]*)"', line)
        if m and serial:
            regions[serial] = m[1]
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
        # A title with nothing to compare (all punctuation) has an empty key and never matches.
        matches = [(title, titles.get(key(title), set()) if key(title) else set()) for title in names]
        ids = set().union(*(ids for _, ids in matches))
        if len(ids) == 1:
            title = next(title for title, matched in reversed(matches) if matched)
            result[serial] = {"gameId": next(iter(ids)), "title": title, "system": "ps2",
                              "region": regions.get(serial, "unknown"), "source": source}
    return result


def merge(existing, extracted):
    """The existing index with the extracted PS2 rows added or refreshed, sorted by serial."""
    merged = dict(existing)
    for serial, row in extracted.items():
        old = merged.get(serial)
        if old is None:
            merged[serial] = row
        elif generated(old):
            # Refresh the facts, keep fields the generator does not know (for example "profile").
            merged[serial] = {**old, **row}
    return dict(sorted(merged.items()))


def catalog_titles(root):
    """Map key(title) to the PS2 catalog game ids that carry it, plus the game files that could not be read.

    A game file that is broken or has no usable title is skipped and reported, not fatal: one bad
    entry must not stop a refresh, and it can only lose a mapping, never add a wrong one.
    """
    titles, skipped = {}, []
    for path in sorted((root / "games").glob("*/game.toml")):
        try:
            game = tomllib.loads(path.read_text(encoding="utf-8"))
            if game.get("hardware") != "sony-ps2":
                continue
            names = [game["title"], *game.get("alt_titles", [])]
            if not all(isinstance(name, str) for name in names) or not isinstance(game["id"], str):
                raise TypeError("title, alt_titles and id must be text")
        except (OSError, ValueError, KeyError, TypeError):
            skipped.append(path.parent.name)
            continue
        for name in names:
            if key(name):
                titles.setdefault(key(name), set()).add(game["id"])
    return titles, skipped


def write_atomic(path, text):
    """Write beside the target, then replace it: an interrupted run never leaves a half-written index."""
    path.parent.mkdir(parents=True, exist_ok=True)
    handle, temporary = tempfile.mkstemp(dir=path.parent, prefix=path.name + ".", suffix=".tmp")
    try:
        with os.fdopen(handle, "w", encoding="utf-8", newline="\n") as stream:
            stream.write(text)
        os.replace(temporary, path)
    except BaseException:
        pathlib.Path(temporary).unlink(missing_ok=True)
        raise


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

        def test_crlf_source_and_empty_keys(self):
            source = 'SYNTH-00007:\r\n  name: "Synthetic"\r\n  region: "PAL"\r\n'
            self.assertEqual(extract(source, {key("Synthetic"): {"ps2-synthetic"}})["SYNTH-00007"]["region"], "PAL")
            # Nothing to compare: never a match, even when the catalog has an equally empty title.
            self.assertEqual(key("?!"), "")
            self.assertEqual(extract('SYNTH-00008:\n  name: "!!!"\n', {"": {"ps2-synthetic"}}), {})
            # Neighbouring titles stay apart: a different number or an extra word is another game.
            titles = {key("Synthetic 2"): {"ps2-two"}}
            self.assertEqual(extract('SYNTH-00009:\n  name: "Synthetic"\n', titles), {})
            self.assertEqual(extract('SYNTH-00010:\n  name: "Synthetic 3"\n', titles), {})
            self.assertEqual(extract('SYNTH-00011:\n  name: "Synthetic II"\n', titles)["SYNTH-00011"]["gameId"], "ps2-two")

        def test_catalog_titles_skip_unreadable_games(self):
            import tempfile as temporary
            with temporary.TemporaryDirectory() as folder:
                root = pathlib.Path(folder)
                for name, text in {
                    "good": 'id = "ps2-good"\ntitle = "Good Game"\nalt_titles = ["Good Alias"]\nhardware = "sony-ps2"\n',
                    "other": 'id = "ps1-other"\ntitle = "Other"\nhardware = "sony-ps1"\n',
                    "broken": "title = [\n",
                    "notitle": 'id = "ps2-notitle"\nhardware = "sony-ps2"\n',
                    "badalt": 'id = "ps2-badalt"\ntitle = "Bad"\nalt_titles = [1]\nhardware = "sony-ps2"\n',
                    "empty": 'id = "ps2-empty"\ntitle = "?!"\nhardware = "sony-ps2"\n',
                }.items():
                    (root / "games" / name).mkdir(parents=True)
                    (root / "games" / name / "game.toml").write_text(text, encoding="utf-8")
                titles, skipped = catalog_titles(root)
                self.assertEqual(titles, {key("Good Game"): {"ps2-good"}, key("Good Alias"): {"ps2-good"}})
                self.assertEqual(skipped, ["badalt", "broken", "notitle"])

        def test_atomic_write_replaces_the_whole_file(self):
            import tempfile as temporary
            with temporary.TemporaryDirectory() as folder:
                target = pathlib.Path(folder) / "out" / "index.json"
                write_atomic(target, "first\n")
                write_atomic(target, "second\n")
                self.assertEqual(target.read_text(encoding="utf-8"), "second\n")
                self.assertEqual([item.name for item in target.parent.iterdir()], ["index.json"])

        def test_live_schema_and_merge(self):
            commit = "0123456789abcdef0123456789abcdef01234567"
            url = source_url(commit)
            with self.assertRaises(ValueError):
                source_url("master")
            source = 'SYNTH-00005:\n  name: "Synthetic"\n  region: "NTSC-U"\nSYNTH-00006:\n  name: "Synthetic"\n'
            row = extract(source, {key("Synthetic"): {"ps2-synthetic"}}, url)
            self.assertEqual(row["SYNTH-00005"], {"gameId": "ps2-synthetic", "title": "Synthetic", "system": "ps2", "region": "NTSC-U", "source": url})
            self.assertEqual(row["SYNTH-00006"]["region"], "unknown")
            ps1 = {"gameId": "ps1-x", "title": "X", "system": "ps1", "region": "USA", "source": "mame", "sourceRecord": "x"}
            manual = {"gameId": "ps2-y", "title": "Y", "system": "ps2", "region": "PAL", "source": "https://example.test/y"}
            old = {"SYNTH-00005": {**row["SYNTH-00005"], "source": SOURCE, "profile": True}, "SYNTH-00001": ps1, "SYNTH-00006": manual}
            merged = merge(old, row)
            self.assertEqual(merged["SYNTH-00001"], ps1)  # PS1 rows are never touched.
            self.assertEqual(merged["SYNTH-00006"], manual)  # Hand-curated rows are never overwritten.
            self.assertEqual(merged["SYNTH-00005"]["source"], url)  # A generated row is refreshed...
            self.assertTrue(merged["SYNTH-00005"]["profile"])  # ...and keeps the fields it does not own.
            self.assertEqual(list(merged), sorted(merged))

    return 0 if unittest.TextTestRunner().run(unittest.defaultTestLoader.loadTestsFromTestCase(SerialFacts)).wasSuccessful() else 1


def main():
    p = argparse.ArgumentParser()
    p.add_argument("source", type=pathlib.Path, help="local copy of GameIndex.yaml")
    p.add_argument("--commit", required=True, help="full commit of PCSX2 that the GameIndex.yaml was taken from")
    p.add_argument("--root", type=pathlib.Path, default=pathlib.Path(__file__).resolve().parents[1])
    p.add_argument("--output", type=pathlib.Path, help="default: data/serial-index.json under --root")
    args = p.parse_args()
    titles, skipped = catalog_titles(args.root)
    for name in skipped:
        print(f"skipped games/{name}/game.toml: unreadable or without a usable title", file=sys.stderr)
    try:
        source_text = args.source.read_text(encoding="utf-8")
    except (OSError, UnicodeDecodeError) as error:
        p.error(f"cannot read {args.source.name}: {error}")
    result = extract(source_text, titles, source_url(args.commit))
    output = args.output or args.root / "data/serial-index.json"
    try:
        existing = json.loads(output.read_text(encoding="utf-8")) if output.exists() else {}
        if not isinstance(existing, dict):
            raise ValueError("not an object")
    except (OSError, ValueError) as error:
        # Never replace an index that cannot be read: it may hold hand-curated rows.
        p.error(f"cannot read the existing {output.name} ({error}); fix or move it first")
    merged = merge(existing, result)
    write_atomic(output, json.dumps(merged, indent=2, ensure_ascii=False) + "\n")
    print(f"{len(result)} verified public PS2 serial/title mappings extracted; {len(merged)} rows in {output.name}")


if __name__ == "__main__":
    if sys.argv[1:] == ["--test"]:
        sys.exit(self_test())
    main()
