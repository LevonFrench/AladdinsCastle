"""CPU-only safety checks for the bounded offline runner; never launch Dolphin."""
import hashlib
import json
import os
from pathlib import Path
import tempfile
import unittest
from unittest import mock

import run_offline as runner


WORKTREE = Path(__file__).resolve().parents[3]
TEMP_ROOT = WORKTREE / ".local" / "runner-tests"


class RunnerTests(unittest.TestCase):
    def setUp(self):
        # Keep all fixtures inside the ignored private worktree tree.
        TEMP_ROOT.mkdir(parents=True, exist_ok=True)
        self.temp = tempfile.TemporaryDirectory(dir=TEMP_ROOT)
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name)
        self.exe = self.root / "probe.exe"
        self.exe.write_bytes(b"synthetic probe fixture")
        self.live = self.root / "live-user"
        (self.live / "Config").mkdir(parents=True)
        (self.live / "Config" / "GFX.ini").write_text("live graphics\n", encoding="utf-8")
        (self.seed / "Wii" / "title" / "00010000" / "12345678" / "data").mkdir(parents=True)
        self.manifest_path = self.root / "manifest.json"
        self.plan_path = self.root / "plan.json"
        self.disc = self.root / "disc-store" / "game.iso"
        self.disc.parent.mkdir()
        self.disc.write_bytes(b"synthetic disc placeholder")
        self.script = self.root / "input.txt"
        self.script.write_text("# no input events\n", encoding="utf-8")
        self.manifest_path.write_text(json.dumps({"demo": {"views": {"3d": {
            "user": str(self.live), "disc": str(self.disc), "config": []
        }}}}), encoding="utf-8")
        self.plan_path.write_text(json.dumps({"games": [{
            "game_key": "demo", "script": str(self.script), "seed_user": str(self.seed),
            "gameplay_start_s": 10, "max_emulated_s": 120
        }]}), encoding="utf-8")
        self.out = self.root / ".local" / "out"
        self.out.mkdir(parents=True)
        self.marker = self.root / "HEADSET-ON"
        self.args = runner.argparse.Namespace(
            marker=str(self.marker), exe=str(self.exe),
            exe_sha256=hashlib.sha256(self.exe.read_bytes()).hexdigest(),
            out=str(self.out), manifest=str(self.manifest_path), plan=str(self.plan_path),
            game="demo", view="3d", rate="native", tag="test", start=None,
            input_until=None, seconds=10, yaw=0, config=[])

    @property
    def seed(self):
        return self.root / "seed-user"

    @unittest.skipUnless(os.name == "nt", "runner launches only on Windows")
    def test_marker_present_rejects_before_process_creation(self):
        self.marker.write_text("present", encoding="utf-8")
        with mock.patch.object(runner.subprocess, "Popen") as popen:
            with self.assertRaisesRegex(RuntimeError, "Headset marker present"):
                runner.run(self.args)
        popen.assert_not_called()

    @unittest.skipUnless(os.name == "nt", "runner launches only on Windows")
    def test_incorrect_executable_hash_rejects_before_process_creation(self):
        self.args.exe_sha256 = "0" * 64
        with mock.patch.object(runner.subprocess, "Popen") as popen:
            with self.assertRaisesRegex(ValueError, "hash does not match"):
                runner.run(self.args)
        popen.assert_not_called()

    @unittest.skipUnless(os.name == "nt", "runner launches only on Windows")
    def test_unsafe_tags_and_guard_overrides_reject_without_launch(self):
        for tag in ("../escape", "..\\escape"):
            with self.subTest(tag=tag), mock.patch.object(runner.subprocess, "Popen") as popen:
                self.args.tag = tag
                with self.assertRaisesRegex(ValueError, "Run tag"):
                    runner.run(self.args)
                popen.assert_not_called()
        self.args.tag = "guard-test"
        self.args.config = ["Dolphin.Interface.ConfirmStop=True"]
        with mock.patch.object(runner.subprocess, "Popen") as popen:
            with self.assertRaisesRegex(ValueError, "weaken the offline guards"):
                runner.run(self.args)
        popen.assert_not_called()

    @unittest.skipUnless(os.name == "nt", "runner launches only on Windows")
    def test_existing_run_directory_is_not_reused(self):
        existing = self.out / "runs" / "demo-native-test"
        existing.mkdir(parents=True)
        (existing / "keep.txt").write_text("prior output", encoding="utf-8")
        with mock.patch.object(runner.subprocess, "Popen") as popen:
            with self.assertRaisesRegex(ValueError, "Refusing to reuse"):
                runner.run(self.args)
        popen.assert_not_called()
        self.assertEqual((existing / "keep.txt").read_text(encoding="utf-8"), "prior output")

    def test_seed_copy_keeps_settings_and_save_data_but_excludes_nand_and_account_ini(self):
        for name in ("WiimoteNew.ini", "GCPadNew.ini"):
            (self.live / "Config" / name).write_text(f"live {name}\n", encoding="utf-8")
        for rel in (Path("GameSettings"), Path("GameSettingsVR"), Path("Config/Profiles")):
            (self.live / rel).mkdir(parents=True, exist_ok=True)
            (self.live / rel / "fixture.ini").write_text("live profile\n", encoding="utf-8")
        title = self.seed / "Wii" / "title" / "00010000" / "12345678"
        (title / "data" / "save.dat").write_bytes(b"save data")
        (title / "content").mkdir(parents=True)
        (title / "content" / "nand-game.bin").write_bytes(b"NAND title content")
        (self.seed / "Config").mkdir()
        (self.seed / "Config" / "Dolphin.ini").write_text(
            "account_secret=must-not-copy\n", encoding="utf-8")
        destination = self.root / "copied-user"

        runner.copy_settings(self.live, self.seed, destination)

        self.assertEqual((destination / "Config/GFX.ini").read_text(encoding="utf-8"), "live graphics\n")
        for name in ("WiimoteNew.ini", "GCPadNew.ini"):
            self.assertTrue((destination / "Config" / name).is_file())
        for rel in (Path("GameSettings"), Path("GameSettingsVR"), Path("Config/Profiles")):
            self.assertTrue((destination / rel / "fixture.ini").is_file())
        self.assertEqual((destination / "Wii/title/00010000/12345678/data/save.dat").read_bytes(), b"save data")
        self.assertFalse((destination / "Wii/title/00010000/12345678/content").exists())
        generated_ini = (destination / "Config/Dolphin.ini").read_text(encoding="utf-8")
        self.assertIn("ConfirmStop = False", generated_ini)
        self.assertNotIn("account_secret", generated_ini)


if __name__ == "__main__":
    unittest.main()
