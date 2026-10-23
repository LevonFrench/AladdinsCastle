"""Synthetic CPU-only source-transform and CSV checks; no engine or build."""
import contextlib
import csv
import hashlib
import io
import json
from pathlib import Path
import sys
import tempfile
import unittest
from unittest import mock

import analyze_draws as analyzer
import prepare_draw_copy as preparer


SOURCE = '''#include "Core/ConfigManager.h"
  const auto used_textures = UsedTextures();
        const auto cache_entry = g_texture_cache->Load(i);
        const auto cache_entry = g_texture_cache->Load(i);
        // Shader Hunter: register shader hashes and check for skip.
void VertexManagerBase::OnEndFrame()
{
  // Track the total emulated state draws
'''


class DiscoveryTests(unittest.TestCase):
    def setUp(self):
        root = Path(__file__).resolve().parents[3] / ".local" / "draw-discovery-tests"
        root.mkdir(parents=True, exist_ok=True)
        self.temp = tempfile.TemporaryDirectory(dir=root)
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name)

    def test_transform_hooks_both_loads_once_and_fails_closed_on_drift(self):
        transformed = preparer.transform(SOURCE)
        self.assertEqual(transformed.count("ACDrawDiscovery::CaptureTexture(i, cache_entry);"), 2)
        self.assertEqual(transformed.count("ACDrawDiscovery::BeginDraw("), 1)
        self.assertEqual(transformed.count("ACDrawDiscovery::Record(ac_draw);"), 1)
        self.assertEqual(transformed.count("ACDrawDiscovery::EndFrame();"), 1)
        with self.assertRaisesRegex(ValueError, "already instrumented"):
            preparer.transform(transformed)
        for anchor in ('#include "Core/ConfigManager.h"\n',
                       "  const auto used_textures = UsedTextures();\n",
                       "        const auto cache_entry = g_texture_cache->Load(i);\n",
                       "        // Shader Hunter: register shader hashes and check for skip.\n",
                       "void VertexManagerBase::OnEndFrame()\n{\n",
                       "  // Track the total emulated state draws\n"):
            with self.subTest(anchor=anchor), self.assertRaisesRegex(ValueError, "anchor count"):
                preparer.transform(SOURCE.replace(anchor, "", 1))
        with self.assertRaisesRegex(ValueError, "anchor count"):
            preparer.transform(SOURCE + '#include "Core/ConfigManager.h"\n')

    def test_prepare_checks_hash_before_writes_and_preserves_outputs(self):
        source = self.root / "source.cpp"
        original = SOURCE.replace("\n", "\r\n").encode()
        source.write_bytes(original)
        output = self.root / "copy"
        with self.assertRaisesRegex(ValueError, "SHA-256 differs"):
            preparer.prepare(source, output, "0" * 64)
        self.assertFalse(output.exists())
        receipt = preparer.prepare(source, output, hashlib.sha256(original).hexdigest())
        self.assertFalse(receipt["executed_or_built"])
        self.assertEqual(source.read_bytes(), original)
        self.assertEqual(receipt["output_sha256"], hashlib.sha256((output / "VertexManagerBase.cpp").read_bytes()).hexdigest())
        self.assertEqual(receipt["helper_sha256"], hashlib.sha256((output / "ACDrawDiscovery.h").read_bytes()).hexdigest())
        saved = (output / "source-copy.json").read_bytes()
        with self.assertRaises(FileExistsError):
            preparer.prepare(source, output, receipt["source_sha256"])
        self.assertEqual((output / "source-copy.json").read_bytes(), saved)

    def test_nearest_checkout_private_output(self):
        nested = self.root / "nested-checkout"
        nested.mkdir()
        (nested / ".git").write_text("gitdir: synthetic\n")
        self.assertFalse(preparer.private_output(nested / "generated"))
        self.assertTrue(preparer.private_output(nested / ".local" / "generated"))

    def fixture(self, missing_texture=False):
        inputs = self.root / "input"
        inputs.mkdir()
        draws = []
        textures = []
        for rid in range(2):
            row = {key: 0 for key in analyzer.DRAW_COLS}
            row.update(record_id=rid, xfb_boundary=rid, last_present_ticks=1000 + rid,
                       ticks_per_second=1000, last_present_emulated_s=1 + rid / 1000,
                       host_elapsed_ns=rid * 1000000, draw_order=1, used_texture_mask=1,
                       role="unknown")
            draws.append(row)
            tex = {key: 0 for key in analyzer.TEX_COLS}
            tex.update(record_id=rid, stage=0, snapshot_valid=1, native_width=64,
                       native_height=64, width=64, height=64, layers=2,
                       metadata_phase="afterLoad_beforeOverrides")
            textures.append(tex)
        for name, columns, values in (("draws.csv", analyzer.DRAW_COLS, draws),
                                      ("textures.csv", analyzer.TEX_COLS, textures[:1] if missing_texture else textures)):
            with (inputs / name).open("w", newline="", encoding="utf-8") as stream:
                writer = csv.DictWriter(stream, fieldnames=columns)
                writer.writeheader()
                writer.writerows(values)
        return inputs

    def analyze(self, inputs, output):
        argv = ["analyze_draws.py", "--input", str(inputs), "--output", str(output),
                "--start", "1", "--duration", ".1", "--marker", str(self.root / "HEADSET-ON")]
        with mock.patch.object(sys, "argv", argv), contextlib.redirect_stdout(io.StringIO()):
            with self.assertRaises(SystemExit) as stopped:
                analyzer.main()
        return stopped.exception.code

    def test_csv_counts_identity_and_preservation_without_roles(self):
        inputs = self.fixture()
        output = self.root / "inventory"
        self.assertEqual(self.analyze(inputs, output), 0)
        result = json.loads((output / "inventory.json").read_text())
        self.assertTrue(result["valid"])
        self.assertEqual((result["submitted_draw_rows"], result["texture_rows"], result["boundaries"]), (2, 2, 2))
        self.assertEqual(result["identity_group_count"], 1)
        self.assertEqual(result["candidates"][0]["count"], 2)
        self.assertFalse(result["roles_inferred"])
        self.assertFalse(result["rules_generated"])
        saved = (output / "inventory.json").read_bytes()
        with self.assertRaises(FileExistsError):
            self.analyze(inputs, output)
        self.assertEqual((output / "inventory.json").read_bytes(), saved)

    def test_missing_join_is_invalid_evidence(self):
        output = self.root / "invalid"
        self.assertEqual(self.analyze(self.fixture(missing_texture=True), output), 1)
        result = json.loads((output / "inventory.json").read_text())
        self.assertFalse(result["valid"])
        self.assertIn("missing texture stages 1", result["errors"])

    def test_receipt_hashes_the_bytes_parsed_when_input_changes(self):
        inputs = self.fixture()
        path = inputs / "draws.csv"
        original_hash = hashlib.sha256(path.read_bytes()).hexdigest()
        original_rows = analyzer.rows

        def changed_after_snapshot(source, columns, cap, **kwargs):
            if source == path:
                source.write_text("changed after byte snapshot\n", encoding="utf-8")
            return original_rows(source, columns, cap, **kwargs)

        output = self.root / "snapshot-inventory"
        with mock.patch.object(analyzer, "rows", side_effect=changed_after_snapshot):
            self.assertEqual(self.analyze(inputs, output), 0)
        result = json.loads((output / "inventory.json").read_text())
        self.assertEqual(result["submitted_draw_rows"], 2)
        self.assertEqual(result["input_sha256"][str(path.resolve())], original_hash)
        self.assertNotEqual(result["input_sha256"][str(path.resolve())],
                            hashlib.sha256(path.read_bytes()).hexdigest())

    def test_marker_appearing_during_validation_prevents_output(self):
        inputs = self.fixture()
        original_rows = analyzer.rows

        def marker_during_validation(source, columns, cap, **kwargs):
            if source.name == "textures.csv":
                (self.root / "HEADSET-ON").touch()
            return original_rows(source, columns, cap, **kwargs)

        output = self.root / "late-marker"
        with mock.patch.object(analyzer, "rows", side_effect=marker_during_validation):
            self.assertEqual(self.analyze(inputs, output), "HEADSET-ON: stop before result write")
        self.assertFalse(output.exists())

    def test_malformed_cli_bounds_leave_no_output(self):
        inputs = self.fixture()
        for option, value in (("--start", "nan"), ("--start", "inf"), ("--start", "-1"),
                              ("--duration", "nan"), ("--duration", "0"), ("--duration", "6"),
                              ("--max-candidates", "0"), ("--max-candidates", "1025")):
            output = self.root / "bad-bounds"
            argv = ["analyze_draws.py", "--input", str(inputs), "--output", str(output),
                    "--start", "1", "--duration", ".1", "--marker", str(self.root / "HEADSET-ON"),
                    option, value]
            with self.subTest(option=option, value=value), mock.patch.object(sys, "argv", argv):
                with self.assertRaisesRegex(ValueError, "invalid bounded window"):
                    analyzer.main()
            self.assertFalse(output.exists())

    def test_schema_finite_values_and_count_cap(self):
        inputs = self.fixture()
        path = inputs / "draws.csv"
        with self.assertRaisesRegex(ValueError, "row cap/shape"):
            list(analyzer.rows(path, analyzer.DRAW_COLS, 1))
        with self.assertRaisesRegex(ValueError, "unexpected schema"):
            list(analyzer.rows(path, analyzer.DRAW_COLS[:-1], 2))
        text = path.read_text()
        columns = text.splitlines()[1].split(",")
        columns[analyzer.DRAW_COLS.index("p0")] = "nan"
        path.write_text(text.splitlines()[0] + "\n" + ",".join(columns) + "\n")
        with self.assertRaisesRegex(ValueError, "nonfinite p0"):
            list(analyzer.rows(path, analyzer.DRAW_COLS, 2))

    def test_headset_marker_stops_before_output(self):
        inputs = self.fixture()
        (self.root / "HEADSET-ON").touch()
        output = self.root / "blocked"
        self.assertEqual(self.analyze(inputs, output), "HEADSET-ON: no CSV workload")
        self.assertFalse(output.exists())


if __name__ == "__main__":
    unittest.main()
