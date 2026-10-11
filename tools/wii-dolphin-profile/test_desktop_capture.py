# SPDX-License-Identifier: GPL-3.0-only
"""Synthetic admission and lifecycle checks; no Dolphin, HWND, WGC or GPU."""
import configparser
import json
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest
from unittest.mock import patch

import desktop_capture as capture
import canonical_dsu


class Child:
    def __init__(self, pid, code=None):
        self.pid, self.returncode = pid, code
        self.terminated = False

    def poll(self):
        return self.returncode

    def terminate(self):
        self.terminated = True
        self.returncode = -1

    def wait(self, timeout):
        return self.returncode


class CaptureTest(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name)
        self.local = self.root / ".local"
        self.local.mkdir()
        inputs = {}
        for key, name in (("dolphin", "Dolphin.exe"), ("capture", "acvr-dolphin-theatre.exe"), ("disc", "synthetic-disc.bin")):
            path = self.local / name
            path.write_bytes(b"RBUP08" + bytes(18) + b"\x5d\x1c\x9e\xa3" + bytes(4) if key == "disc" else b"synthetic binary, not executable")
            inputs[key] = str(path)
            inputs[key + ("_header_sha256" if key == "disc" else "_sha256")] = capture.sha256(path, key == "disc")
        self.config = self.local / "inputs.json"
        self.config.write_text(json.dumps(inputs))
        self.run = self.local / "run"
        self.inputs = inputs

    def plan(self):
        return capture.prepare(self.config, self.run, self.root)

    def supervise(self, plan, **kwargs):
        kwargs.setdefault("settle", 0)
        return capture.supervise(plan, **kwargs)

    def test_prepare_no_process_preserves_input_and_refuses_existing_run(self):
        before = {p: p.read_bytes() for p in self.local.iterdir()}
        with patch.object(capture.subprocess, "Popen", side_effect=AssertionError("must not launch")):
            plan = self.plan()
        for path, data in before.items():
            self.assertEqual(path.read_bytes(), data)
        self.assertFalse(plan["live_proof"])
        ini = configparser.ConfigParser()
        ini.read(self.run / "User/Config/Dolphin.ini")
        self.assertEqual(ini["Display"]["RenderToMain"], "False")
        self.assertEqual(ini["Interface"]["OnScreenDisplayMessages"], "False")
        self.assertEqual(ini["Interface"]["ShowActiveTitle"], "True")
        gfx = configparser.ConfigParser()
        gfx.read(self.run / "User/Config/GFX.ini")
        self.assertEqual(gfx["Stereoscopy"]["StereoMode"], "1")
        with self.assertRaises(FileExistsError):
            self.plan()

    def test_identity_changes_reject_before_writes(self):
        for key in ("dolphin", "capture", "disc"):
            path = Path(self.inputs[key])
            original = path.read_bytes()
            path.write_bytes(b"changed")
            with self.assertRaises(ValueError):
                self.plan()
            self.assertFalse(self.run.exists())
            path.write_bytes(original)

    def test_unscoped_disc_and_outside_config_reject(self):
        path = Path(self.inputs["disc"])
        path.write_bytes(b"zzzzzz" + bytes(26))
        with self.assertRaises(ValueError):
            capture.sha256(path, True)
        with self.assertRaises(ValueError):
            capture.prepare(self.root / "outside.json", self.run, self.root)

    def test_timeout_and_ambiguous_windows_stop_only_owned_child(self):
        plan = self.plan()
        for windows in (0, 2):
            child = Child(123)
            ticks = iter((0, 0, 40))
            result = self.supervise(plan, popen=lambda *a, **k: child,
                window_count=lambda pid: windows, clock=lambda: next(ticks), sleep=lambda t: None)
            self.assertEqual(result["state"], "failed")
            self.assertTrue(child.terminated)
            self.assertEqual([r["pid"] for r in result["owned_cleanup"]], [123])

    def test_failed_capture_keeps_logs_and_reaps_both(self):
        plan = self.plan()
        children = [Child(123), Child(456, 1)]
        result = self.supervise(plan, popen=lambda *a, **k: children.pop(0), window_count=lambda pid: 1)
        self.assertEqual(result["capture_exit"], 1)
        self.assertFalse(result["rgb_gate"])
        self.assertEqual([r["pid"] for r in result["owned_cleanup"]], [456, 123])
        self.assertEqual([r["termination"] for r in result["owned_cleanup"]], ["already-exited", "terminate"])
        self.assertFalse(result["normal_exit"])
        self.assertTrue((self.run / "result.json").is_file())
        self.assertTrue((self.run / "capture.log").is_file())

    def test_hung_capture_is_bounded_and_reaped(self):
        plan = self.plan()
        dolphin, viewer = Child(123), Child(456)
        children = iter((dolphin, viewer))
        ticks = iter((0, 0, 200))
        result = self.supervise(plan, popen=lambda *a, **k: next(children), window_count=lambda pid: 1,
                                   clock=lambda: next(ticks), sleep=lambda t: None)
        self.assertIn("exceeded bound", result["error"])
        self.assertTrue(dolphin.terminated and viewer.terminated)

    def test_captured_receipt_still_requires_visual_acceptance(self):
        plan = self.plan()
        children = iter((Child(123), Child(456, 0)))
        def start(argv, **kwargs):
            if "--capture-only" in argv:
                self.assertEqual(argv[argv.index("--pid")+1], "123")
                proof = self.run / "proof"
                proof.mkdir()
                for name in ("frame-1.png", "frame-2.png"):
                    (proof / name).write_bytes(b"\x89PNG\r\n\x1a\n" + b"synthetic")
                (proof / "capture.json").write_text(json.dumps({"capture": "Windows.Graphics.Capture",
                    "different_rgb": True, "spatial_candidate": True, "game_window_selected": True, "interval_ms": 1100}))
            return next(children)
        ticks = iter((0, 0, 1, 21, 21))
        result = capture.supervise(plan, popen=start, window_count=lambda pid: 1,
                                   clock=lambda: next(ticks), sleep=lambda t: None)
        self.assertEqual(result["settle_seconds"], 20)
        self.assertTrue(result["rgb_gate"])
        self.assertLessEqual(result["dolphin_launch_requested_unix"], result["dolphin_created_unix"])
        self.assertLessEqual(result["capture_launch_requested_unix"], result["capture_created_unix"])
        self.assertLessEqual(result["capture_created_unix"], result["finished_unix"])
        self.assertFalse(result["live_proof"])
        self.assertEqual(result["sbs_visual_check"], "required")

    def test_actual_cpu_child_reaped_if_second_spawn_fails(self):
        plan = self.plan()
        started = []
        def start(argv, **kwargs):
            if started:
                raise OSError("synthetic second spawn failure")
            child = subprocess.Popen([sys.executable, "-c", "import time; time.sleep(30)"], **kwargs)
            started.append(child)
            return child
        result = self.supervise(plan, popen=start, window_count=lambda pid: 1)
        self.assertEqual(result["state"], "failed")
        self.assertIsNotNone(started[0].poll())
        self.assertEqual(len(result["owned_cleanup"]), 1)

    def test_malformed_receipt_fails_and_cleans_up(self):
        plan = self.plan()
        proof = self.run / "proof"
        proof.mkdir()
        for data in ([], {"capture": "Windows.Graphics.Capture", "different_rgb": True, "interval_ms": "1000"}):
            (proof / "capture.json").write_text(json.dumps(data))
            children = iter((Child(123), Child(456, 0)))
            result = self.supervise(plan, popen=lambda *a, **k: next(children), window_count=lambda pid: 1)
            self.assertIn("Incomplete capture receipt", result["error"])
            self.assertFalse(result["rgb_gate"])
            self.assertEqual(len(result["owned_cleanup"]), 2)

    def test_settle_delays_capture_and_stops_if_window_changes(self):
        plan = self.plan()
        for changed in (False, True):
            dolphin = Child(123)
            calls = []
            ticks = iter((0, 0, 1, 8, 8))
            counts = iter((1, 2 if changed else 1))
            def start(argv, **kwargs):
                calls.append(argv)
                return dolphin if len(calls) == 1 else Child(456, 1)
            result = self.supervise(plan, settle=7, popen=start, clock=lambda: next(ticks),
                window_count=lambda pid: next(counts), sleep=lambda t: None)
            self.assertEqual(len(calls), 1 if changed else 2)
            if changed:
                self.assertIn("window changed during settle", result["error"])
            else:
                self.assertEqual(result["settle_seconds"], 7)
            self.assertTrue(dolphin.terminated)
        with self.assertRaises(ValueError):
            self.supervise(plan, settle=31)

    def test_canonical_dsu_manifest_preserved_and_named_override_removed(self):
        generated = self.local / "generated"
        profile = generated / "flat-dolphin/split-right.ini"
        profile.parent.mkdir(parents=True)
        profile.write_bytes(canonical_dsu.render("AladdinsCastle", legacy=True))
        (generated / "generated-files.json").write_text(json.dumps({"version":"shell-stick/1",
            "files":{"flat-dolphin/split-right.ini":capture.sha256(profile)}}))
        self.inputs.update(dsu_profile=str(profile), dsu_profile_sha256=capture.sha256(profile))
        self.config.write_text(json.dumps(self.inputs))
        self.plan()
        self.assertEqual((self.run / "User/Config/WiimoteNew.ini").read_bytes(), profile.read_bytes())
        ini = configparser.ConfigParser()
        ini.read(self.run / "User/Config/Dolphin.ini")
        self.assertEqual(ini["Input"]["BackgroundInput"], "True")
        self.assertNotIn("WiimoteProfile", (self.run / "User/GameSettings/RBUP08.ini").read_text())
        profile.write_bytes(b"tampered")
        with self.assertRaises(ValueError):
            capture.admit(self.config,self.root)
        profile.write_bytes(canonical_dsu.render("OtherInput", legacy=True))
        (generated / "generated-files.json").write_text(json.dumps({"version":"shell-stick/1",
            "files":{"flat-dolphin/split-right.ini":capture.sha256(profile)}}))
        self.inputs["dsu_profile_sha256"] = capture.sha256(profile)
        self.config.write_text(json.dumps(self.inputs))
        with self.assertRaisesRegex(ValueError, "label differs from product metadata"):
            capture.admit(self.config,self.root)


if __name__ == "__main__":
    unittest.main()
