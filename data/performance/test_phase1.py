"""Synthetic checks only. No emulator launch, real disc, or timing claim."""
import hashlib
import json
from pathlib import Path
import tempfile
import unittest
from unittest.mock import patch

import phase1


class Phase1Tests(unittest.TestCase):
    def setUp(self):
        root = phase1.REPO / ".local" / "synthetic-framerate"
        root.mkdir(parents=True, exist_ok=True)
        self.tmp = tempfile.TemporaryDirectory(dir=root)
        self.addCleanup(self.tmp.cleanup)
        self.root = Path(self.tmp.name)
        self.exe = self.root / "synthetic.exe"
        self.exe.write_bytes(b"synthetic, never executable")
        self.disc = self.root / "synthetic-header.fixture"
        header = bytearray(32)
        header[:6] = b"RBUP08"
        header[7] = 3
        header[24:28] = bytes.fromhex("5d1c9ea3")
        self.disc.write_bytes(header)
        self.pin = dict(phase1.PIN, executable_sha256=hashlib.sha256(self.exe.read_bytes()).hexdigest())

    def prepare(self, experiment="baseline", output="run", game_id="RBUP08"):
        with patch.object(phase1, "PIN", self.pin):
            return phase1.prepare(self.exe, self.disc, game_id, experiment, self.root / output)

    def test_baseline_private_revision_no_cheat(self):
        result = self.prepare()
        self.assertEqual(result["revision"], 3)
        self.assertIsNone(result["measured_results"])
        self.assertEqual(result["status"], "prepared-unmeasured")
        self.assertEqual((self.root / "run/User/GameSettings/RBUP08.ini").read_text(), "")
        self.assertIn("GFX.VR.EnableOpenXR=False", result["arguments"])
        self.assertIn("SYSCONF.IPL.E60=True", result["arguments"])
        self.assertIn("Dolphin.Core.EnableCheats=False", result["arguments"])

    def test_code_then_override_only_changes_vbi(self):
        code = self.prepare("code", "code")
        higher = self.prepare("code-vbi90", "higher")
        self.assertIn("Dolphin.Core.VIOverclock=1.5", higher["arguments"])
        self.assertIn("Dolphin.Core.VIOverclock=1.0", code["arguments"])
        self.assertEqual((self.root / "code/User/GameSettings/RBUP08.ini").read_text(),
                         (self.root / "higher/User/GameSettings/RBUP08.ini").read_text())

    def test_existing_profile_untouched(self):
        self.prepare()
        before = (self.root / "run/run.json").read_bytes()
        with self.assertRaises(ValueError):
            self.prepare()
        self.assertEqual(before, (self.root / "run/run.json").read_bytes())

    def test_other_title_rejected_before_file_access(self):
        with self.assertRaises(ValueError):
            self.prepare(game_id="OTHER1")

    def test_exact_disc_id_mismatch(self):
        with self.assertRaisesRegex(ValueError, "Disc ID"):
            self.prepare(game_id="SBDP08")
        self.assertFalse((self.root / "run").exists())

    def test_wrong_executable_rejected(self):
        self.exe.write_bytes(b"changed")
        with self.assertRaisesRegex(ValueError, "Executable"):
            self.prepare()

    def test_public_output_rejected(self):
        with self.assertRaises(ValueError):
            self.prepare(output=str(phase1.HERE / "forbidden"))

    def test_parent_escape_rejected(self):
        designated = self.root / "framerate"
        with self.assertRaises(ValueError):
            phase1.private_path(designated / "../outside", designated)

    def test_invalid_header_rejected(self):
        self.disc.write_bytes(b"short")
        with self.assertRaises(ValueError):
            self.prepare()

    def test_interval_window_uses_complete_intervals(self):
        log = self.root / "intervals.txt"
        log.write_text("100\n200\n300\n400\n")
        result = phase1.summarize(log, .1, .9)
        self.assertEqual(result["samples"], 3)
        self.assertEqual(result["interval_mean_ms"], 300)
        self.assertNotIn("fps", result)
        self.assertIn("duplicate", result["limits"])

    def test_missing_window_rejected(self):
        log = self.root / "intervals.txt"
        log.write_text("100\n")
        with self.assertRaises(ValueError):
            phase1.summarize(log, 0, 2)

    def test_invalid_interval_rejected(self):
        log = self.root / "intervals.txt"
        for value in ("nan", "inf", "0", "-1"):
            log.write_text(value)
            with self.assertRaises(ValueError):
                phase1.summarize(log, 0, 1)


if __name__ == "__main__":
    unittest.main()
