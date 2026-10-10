#!/usr/bin/env python3
"""Audit public tracked text. Private operator receipts belong in ignored state."""
import argparse
import pathlib
import re
import subprocess
import unittest

DRIVE = re.compile(r"(?<![A-Za-z0-9])[A-Za-z]:[\\/](?![\\/])")
HARDWARE = re.compile(r"(?:Ryzen\s+\d\s+\d{4}|RTX\s+\d{4}|\d+(?:\.\d+)?\s+GiB RAM)", re.I)
HEADSET = re.compile(r"\b(?:Quest\s*(?:Pro|\d+S?)|Pico\s*\d+|(?:HTC\s+)?Vi" r"ve(?:\s+(?:Pro|XR|Cosmos))?|Valve\s+Index|Reverb\s+G\d+)\b", re.I)
OWNER = re.compile(r"\b(?:(?:owner|operator)(?:'s)?\s+(?:(?:test|reference|local)\s+)?(?:headset|setup|PC|machine|rig)|test machines\s*\(owner\))", re.I)
RECEIPT = re.compile(r"\b(?:owner(?:'s)?\s+(?:verification|receipts?|(?:local\s+)?library|scan(?:\s+results)?)|operator(?:'s)?\s+(?:library|scan|receipts?)|full[- ]library|scan re" r"ceipts?|private re" r"ceipts?)\b(?:(?!\n\s*\n)[\s\S]){0,250}?(?:(?<![\w.])\d[\d,.]*\s+(?:supported\s+)?(?:files|sets|discs|archives|bindings|BIOS|media)\b|matched[^\n]*(?:regional identity|disc|BIOS))", re.I)
COVER = re.compile(r"\b([A-Z]{4}[-_ ]?\d{3}[._ ]?\d{2}|[A-Z0-9]{2,10}[-_][A-Z0-9]{2,8}[-_]\d{2,6}|[A-Z0-9]{4}\d{2})\.(?:png|jpe?g|webp)\b", re.I)
ORDINARY_ART = re.compile(r"^(?:icon|logo|tile|image|thumb|avatar|texture|sprite|banner|background|splash|thumbnail)[-_ .]?\d+(?:x\d+)?$", re.I)
SYNTHETIC_COVER = re.compile(r"^TEST[-_]\d{5}$", re.I)


# A positive library/scan observation is private even when its count is omitted.
RECEIPT_PROSE = re.compile(r"\b(?:(?:owner|operator)(?:'s)?\s+(?:(?:local\s+)?library|scan(?:\s+results)?|verification|receipts?)|scan re" r"ceipts?|private re" r"ceipts?)\b[^\n]{0,160}?\b(?:matched|confirmed|found|verified|contains|includes|reported|present|available)\b", re.I)

def receipt_prose(text):
    for match in RECEIPT_PROSE.finditer(text):
        boundary=max(text.rfind("\n",0,match.start()),text.rfind(".",0,match.start()),text.rfind(";",0,match.start()))+1
        observation=text[boundary:match.end()]
        if not re.search(r"\b(?:no|not|never|without|deferred|pending|unverified|aren['’]t|isn['’]t|wasn['’]t|weren['’]t|don['’]t|doesn['’]t)\b",observation,re.I):
            return True
    return False

def private_headset_names(text):
    # Quest 3 is owner-approved public project/platform information.
    return any(re.sub(r"\s+","",match.group()).lower()!="quest3" for match in HEADSET.finditer(text))

def issues(name, text):
    out = []
    if DRIVE.search(text): out.append("absolute drive path")
    if RECEIPT.search(text) or receipt_prose(text): out.append("private owner receipt")
    if private_headset_names(text) and pathlib.PurePosixPath(name).suffix in {".cpp", ".h", ".qml", ".py"}:
        out.append("operator headset name")
    if name.startswith("docs/"):
        if HARDWARE.search(text): out.append("exact operator hardware")
        # Product/platform documentation can name supported headsets. Operator
        # passages and spike receipts may not identify the owner's headset.
        if private_headset_names(text) and (name.startswith("docs/spikes/") or any(
                private_headset_names(line) and OWNER.search(line) for line in text.splitlines())):
            out.append("operator headset name")
    if pathlib.PurePosixPath(name).suffix in {".cpp", ".h", ".qml", ".py"} and any(
            not SYNTHETIC_COVER.fullmatch(serial) and not ORDINARY_ART.fullmatch(serial) for serial in COVER.findall(text)):
        out.append("non-synthetic cover serial")
    return out


class Regression(unittest.TestCase):
    def test_paths_and_urls(self):
        self.assertTrue(issues("docs/a.md", "X" + ":/private/file"))
        self.assertFalse(issues("docs/a.md", "https://example.org/source <repo>/.local/"))
    def test_hardware(self):
        self.assertTrue(issues("docs/a.md", "RTX " + "9999"))
        self.assertFalse(issues("docs/a.md", "desktop, recent high-end GPU"))
    def test_receipt(self):
        for text in ("Owner" + " receipts: 123 supported files", "Scan" + " receipt: 37 discs verified",
                     "Operator" + " library: 19 archives and 4 bindings", "Full" + "-library scan: 42 sets"):
            for source in ("docs/a.md", "hub/tests/A.cpp", "tools/a.py"):
                self.assertIn("private owner receipt", issues(source, text))
        self.assertFalse(issues("docs/a.md", "Owner verification passed; receipts are kept privately in `.local/`."))
        self.assertFalse(issues("docs/a.md", "Catalog validation: 413 games; synthetic tests passed."))
    def test_receipt_without_counts(self):
        for text in ("Owner"+" library: expected regional disc matched", "Operator"+" scan confirmed the BIOS identity", "Scan"+" receipt: disc present", "Private"+" receipt includes a recognised set", "No downloads were made; owner"+" library contains the expected disc", "Owner"+" verification: expected regional disc matched"):
            self.assertIn("private owner receipt",issues("docs/a.md",text))
        for text in ("No owner"+" library scan was verified", "Owner"+" scan was not run", "Use the owner"+" library list; skip items that aren't present", "Synthetic catalog tests passed", "Owner"+" receipts remain private; runtime acceptance is pending"):
            self.assertNotIn("private owner receipt",issues("docs/a.md",text))
    def test_ordinary_image_names(self):
        for name in ("icon-256.png","logo96.png","thumbnail_1024.jpg","tiles-64.webp","banner2026.png","sprite-512.png"):
            self.assertFalse(issues("hub/tests/A.cpp",name),name)
    def test_public_headset_model(self):
        for source in ("docs/architecture.md","docs/hub-architecture.md","docs/tasks/m1/B-spike-overlay.md","docs/spikes/a.md","hub/src/A.cpp"):
            self.assertNotIn("operator headset name",issues(source,"Owner headset: Quest 3"))
    def test_cover_serial_forms_without_directory(self):
        for stem in ("FAKE" + "-11111", "FAKE" + "_111.11", "FAKE" + "11111", "ABCD" + "01", "FAKE" + "-AB12-34"):
            for extension in ("png", "jpg", "webp"):
                for source in ("hub/tests/A.cpp", "hub/src/A.cpp", "tools/a.py"):
                    self.assertIn("non-synthetic cover serial", issues(source, 'write("' + stem + '.' + extension + '")'))
        self.assertFalse(issues("hub/tests/A.cpp", "/covers/TEST-00002.png"))
        self.assertFalse(issues("hub/tests/A.cpp", "synthetic-cover.png overlay-thumbnail.png"))
    def test_pinned_catalog_provenance(self):
        import tomllib
        root = pathlib.Path(__file__).resolve().parents[1]
        game_text = (root / "games/ps2-time-crisis-2/game.toml").read_text(encoding="utf-8")
        game = tomllib.loads(game_text)
        from build_serial_index import SOURCE
        self.assertIn(SOURCE, game["meta"]["sources"])
        from datetime import date
        date.fromisoformat(game["meta"]["checked"])
        route_line = next(line for line in game_text.splitlines() if line.startswith("pcsx2 ="))
        self.assertIsNone(re.search(r"\b[A-Z]{4}[-_]\d{5}\b", route_line))
    def test_operator_headsets(self):
        for name in ("Qu" + "est 99", "Pi" + "co 99", "Re" + "verb G99"):
            for source in ("docs/spikes/a.md", "hub/tests/A.cpp", "hub/src/A.cpp", "tools/a.py"):
                self.assertIn("operator headset name", issues(source, name))
            self.assertIn("operator headset name", issues("docs/a.md", "Owner headset: " + name))
            self.assertFalse(issues("docs/platforms.md", "Supported platform: " + name))
        self.assertFalse(issues("docs/spikes/a.md", "PCVR headset over streamed transport"))


def audit(root):
    files = subprocess.check_output(["git", "-C", str(root), "ls-files", "-z"], text=True).split("\0")
    failures = []
    for name in files:
        if not name: continue
        try: text = (root / name).read_text(encoding="utf-8")
        except (UnicodeError, OSError): continue
        found = issues(name, text)
        if found: failures.append((name, found))
    for name, found in failures: print(name + ": " + ", ".join(found))
    return bool(failures)


if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument("--test", action="store_true")
    args = parser.parse_args()
    if args.test:
        result = unittest.TextTestRunner().run(unittest.defaultTestLoader.loadTestsFromTestCase(Regression))
        if not result.wasSuccessful(): raise SystemExit(1)
    raise SystemExit(audit(pathlib.Path(__file__).resolve().parents[1]))