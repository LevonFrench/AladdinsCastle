"""Synthetic checks only. No emulator launch, real disc, or timing claim."""
import hashlib
import json
from pathlib import Path
import tempfile
import unittest
from unittest.mock import patch
import subprocess

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
        self.assertIn("Graphics.VR.EnableOpenXR=False", result["arguments"])
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

    def test_native_mode_mapping_and_no_code(self):
        for experiment, mode in phase1.NATIVE_PLAN["experiments"].items():
            with self.subTest(mode=mode):
                record = self.prepare(experiment, experiment)
                self.assertEqual(record["forced_vbi_value"], phase1.NATIVE_PLAN["modes"][mode])
                self.assertEqual(record["status"], "prepared-awaiting-scripted-input")
                self.assertIsNone(record["input_contract"])
                self.assertIsNone(record["candidate_code"])
                self.assertIsNone(record["measured_results"])
                self.assertIn("Graphics.VR.AutoVBIFromHMD=False", record["arguments"])
                self.assertIn("Graphics.VR.EnableOpenXR=True", record["arguments"])
                self.assertIn("Dolphin.Core.VIOverclockEnable=False", record["arguments"])
                self.assertIn("Dolphin.Core.EnableCheats=False", record["arguments"])
                self.assertEqual((self.root / experiment / "User/GameSettings/RBUP08.ini").read_text(), "")
                self.assertFalse(any(arg.startswith("GFX.") for arg in record["arguments"]))

    def test_wbfs_waits_on_hub_before_file_access(self):
        with self.assertRaisesRegex(ValueError, "Hub identity"):
            self.prepare("native-off", "wbfs", "R2VE01")
        self.assertFalse((self.root / "wbfs").exists())

    def test_baseline_command_namespace(self):
        self.assertFalse(any(arg.startswith("GFX.") for arg in self.prepare()["arguments"]))

    def input_fixture(self):
        profile = self.root / "split-right.ini"
        profile.write_text("[Wiimote1]\nSource = 1\nDevice = DSUClient/0/AladdinsCastle\nButtons/A = `Cross`\nButtons/B = `R2`\nButtons/+ = `Options`\n" +
                           "".join(f"[{s}]\nSource = 0\n" for s in ("Wiimote2", "Wiimote3", "Wiimote4", "BalanceBoard")))
        manifest = self.root / "manifest.json"
        manifest.write_text(json.dumps({"version": "shell-stick/1", "files": {
            "flat-dolphin/split-right.ini": phase1.sha(profile)}}))
        script = self.root / "timeline.json"
        script.write_text(json.dumps({"version": 1, "seconds": 180, "steps": [{"at": 0, "pads": [{}, {}]}]}))
        entrypoint = self.root / "scripted.py"
        entrypoint.write_text("synthetic; never executed")
        return profile, manifest, script, entrypoint

    def test_attach_checked_lead_input_never_runs_server(self):
        self.prepare("native-off")
        profile, manifest, script, entrypoint = self.input_fixture()
        checked = subprocess.CompletedProcess([], 0, json.dumps({"valid": True, "sha256": phase1.sha(script)}))
        with patch.object(phase1.subprocess, "run", return_value=checked) as checker:
            record = phase1.attach_input(self.root / "run", profile, manifest, script, entrypoint)
        self.assertEqual(checker.call_args.args[0][-1], "--check")
        self.assertEqual(record["status"], "prepared-unmeasured")
        self.assertIsNone(record["measured_results"])
        self.assertEqual((self.root / "run/User/Config/WiimoteNew.ini").read_bytes(), profile.read_bytes())
        self.assertEqual(record["input_contract"]["script_sha256"], phase1.sha(script))
        with self.assertRaises(ValueError):
            phase1.attach_input(self.root / "run", profile, manifest, script, entrypoint)

    def test_changed_profile_rejected_before_script_check(self):
        self.prepare("native-off")
        profile, manifest, script, entrypoint = self.input_fixture()
        profile.write_text(profile.read_text().replace("`Cross`", "`Triangle`"))
        with patch.object(phase1.subprocess, "run") as checker, self.assertRaisesRegex(ValueError, "manifest"):
            phase1.attach_input(self.root / "run", profile, manifest, script, entrypoint)
        checker.assert_not_called()

    def test_input_check_failure_preserves_waiting_profile(self):
        self.prepare("native-off")
        paths = self.input_fixture()
        with patch.object(phase1.subprocess, "run", side_effect=subprocess.CalledProcessError(2, [])), self.assertRaises(subprocess.CalledProcessError):
            phase1.attach_input(self.root / "run", *paths)
        record = json.loads((self.root / "run/run.json").read_text())
        self.assertEqual(record["status"], "prepared-awaiting-scripted-input")
        self.assertFalse((self.root / "run/User/Config/WiimoteNew.ini").exists())

    def test_deadspace_preview_has_disabled_code_and_exact_region(self):
        raw = bytearray(self.disc.read_bytes())
        raw[:6] = b"RZJE69"
        raw[7] = 0
        self.disc.write_bytes(raw)
        with patch.object(phase1, "PIN", self.pin):
            record = phase1.prepare_deadspace_candidate(self.exe, self.disc, self.root / "dse")
        game_ini = (self.root / "dse/User/GameSettings/RZJE69.ini").read_text()
        self.assertIn("[ActionReplay_Disabled]", game_ini)
        self.assertNotIn("[ActionReplay_Enabled]", game_ini)
        self.assertIn("04603B44 42700000", game_ini)
        self.assertIn("Dolphin.Core.EnableCheats=False", record["arguments"])
        self.assertIn("Graphics.VR.EnableOpenXR=False", record["arguments"])
        self.assertEqual(record["status"], "prepared-disabled-code-candidate")
        self.assertFalse(record["candidate_code"]["source_review_complete"])
        self.assertIsNone(record["measured_results"])

    def test_deadspace_wrong_id_or_revision_never_writes(self):
        for game_id, revision in ((b"RZJP69", 0), (b"RZJE69", 1), (b"RBUP08", 0)):
            raw = bytearray(self.disc.read_bytes())
            raw[:6], raw[7] = game_id, revision
            self.disc.write_bytes(raw)
            with self.assertRaisesRegex(ValueError, "exact RZJE69 revision 0"):
                phase1.prepare_deadspace_candidate(self.exe, self.disc, self.root / "dse")
            self.assertFalse((self.root / "dse").exists())


if __name__ == "__main__":
    unittest.main()
