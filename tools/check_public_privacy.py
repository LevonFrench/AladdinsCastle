#!/usr/bin/env python3
"""Audit public tracked text. Private operator receipts belong in ignored state."""
import argparse
import pathlib
import re
import subprocess
import unittest

DRIVE = re.compile(r"(?<![A-Za-z0-9])[A-Za-z]:[\\/](?![\\/])")
HARDWARE = re.compile(r"(?:Ryzen\s+\d\s+\d{4}|RTX\s+\d{4}|\d+(?:\.\d+)?\s+GiB RAM)", re.I)
RECEIPT = re.compile(r"(?:owner verification|full-library|owner receipts)[\s\S]{0,600}?(?:\d[\d,.]*\s+(?:supported files|files /|archive entries)|matched[^\n]*(?:regional identity|disc|BIOS))", re.I)
COVER = re.compile(r"/covers/([A-Z]{4}-\d{5})\.png")

def issues(name, text):
    out = []
    if DRIVE.search(text): out.append("absolute drive path")
    if name.startswith("docs/"):
        if HARDWARE.search(text): out.append("exact operator hardware")
        if RECEIPT.search(text): out.append("private owner receipt")
    if name.startswith("hub/tests/") and any(s != "TEST-00001" for s in COVER.findall(text)):
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
        self.assertTrue(issues("docs/a.md", "Owner receipts: 123 supported files"))
        self.assertFalse(issues("docs/a.md", "Owner verification passed; receipts are kept privately in `.local/`."))
    def test_cover_fixture(self):
        self.assertTrue(issues("hub/tests/A.cpp", "/covers/ABCD-11111.png"))
        self.assertFalse(issues("hub/tests/A.cpp", "/covers/TEST-00001.png"))

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
