# SPDX-License-Identifier: GPL-3.0-only
import hashlib
import importlib.util
import pathlib
import tempfile
import unittest

SCRIPT = pathlib.Path(__file__).resolve().parents[1] / "tools/patch_engine.py"
spec = importlib.util.spec_from_file_location("n22_patch", SCRIPT)
patch = importlib.util.module_from_spec(spec)
spec.loader.exec_module(patch)

class GuardTests(unittest.TestCase):
    def setUp(self):
        self.parent = pathlib.Path.cwd().resolve()
        self.temp = tempfile.TemporaryDirectory(dir=self.parent, prefix="n22-guard-")
        self.root = pathlib.Path(self.temp.name)
        if not self.root.resolve().is_relative_to(self.parent) or self.root.resolve() == self.parent:
            raise RuntimeError("Fixture root outside intended test workspace")
        self.source = self.root / "source"
        self.manifest = {"files": {}, "headers": {}}
        for name in patch.HEADERS:
            path = self.source / name
            path.parent.mkdir(parents=True, exist_ok=True)
            text = "synthetic header\n"
            path.write_text(text, encoding="utf-8")
            self.manifest["headers"][name] = hashlib.sha256(text.encode()).hexdigest()
        for name in patch.FILES:
            path = self.source / name
            path.parent.mkdir(parents=True, exist_ok=True)
            text = "synthetic first anchor\nsynthetic second anchor\n"
            path.write_text(text, encoding="utf-8")
            self.manifest["files"][name] = {"sha256_lf": hashlib.sha256(text.encode()).hexdigest(), "edits": [
                {"id": "one", "old": "synthetic first anchor", "new": "first replacement"},
                {"id": "two", "old": "synthetic second anchor", "new": "second replacement"}]}
    def tearDown(self):
        if not self.root.resolve().is_relative_to(self.parent) or self.root.resolve() == self.parent:
            raise RuntimeError("Fixture cleanup target outside intended test workspace")
        self.temp.cleanup()
    def test_all_edits_and_idempotent_output(self):
        rendered = patch.render(self.manifest, self.source)
        output = self.root / ".local/overlay"
        patch.write_overlay(rendered, output, self.root)
        before = {n:(output/n).read_bytes() for n in patch.FILES}
        patch.write_overlay(rendered, output, self.root)
        self.assertEqual(before, {n:(output/n).read_bytes() for n in patch.FILES})
        for text in rendered.values():
            self.assertIn("ACVR-PATCH:one", text)
            self.assertIn("ACVR-PATCH:two", text)
    def test_source_drift_aborts_before_output(self):
        name = sorted(patch.FILES)[-1]
        (self.source/name).write_text("drift", encoding="utf-8")
        with self.assertRaises(ValueError):
            patch.render(self.manifest, self.source)
        self.assertFalse((self.root/".local").exists())
    def test_anchor_drift_rejected_even_with_matching_hash(self):
        name = sorted(patch.FILES)[0]
        self.manifest["files"][name]["edits"][1]["old"] = "missing"
        with self.assertRaises(ValueError):
            patch.render(self.manifest, self.source)
    def test_output_drift_and_escape_rejected(self):
        rendered = patch.render(self.manifest, self.source)
        output = self.root / ".local/overlay"
        patch.write_overlay(rendered, output, self.root)
        name = sorted(patch.FILES)[0]
        (output/name).write_text("drift", encoding="utf-8")
        with self.assertRaises(ValueError):
            patch.write_overlay(rendered, output, self.root)
        self.assertEqual("drift", (output/name).read_text())
        with self.assertRaises(ValueError):
            patch.write_overlay(rendered, self.root/"outside", self.root)
    def test_allowlist_cannot_target_game_paths(self):
        self.manifest["files"]["unapproved/path.c"] = {}
        with self.assertRaises(ValueError):
            patch.render(self.manifest, self.source)
    def test_header_drift_rejected(self):
        (self.source/sorted(patch.HEADERS)[0]).write_text("drift", encoding="utf-8")
        with self.assertRaises(ValueError):
            patch.render(self.manifest, self.source)

if __name__ == "__main__":
    unittest.main()
