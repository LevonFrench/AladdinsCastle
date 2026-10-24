"""CPU-only safety checks for the bounded offline runner; never launch Dolphin."""
import hashlib
import configparser
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
            input_until=None, seconds=10, yaw=0, config=[],
            phase="baseline", capture_policy="sampled", observe_until=None)

    @property
    def seed(self):
        return self.root / "seed-user"

    def run_with_fake_exited_child(self):
        """Exercise preparation/receipts while replacing process creation entirely."""
        child = mock.Mock()
        child.pid = 12345
        child.returncode = 0
        child.poll.return_value = 0
        with mock.patch.object(runner.subprocess, "Popen", return_value=child) as popen:
            with mock.patch("builtins.print"):
                runner.run(self.args)
        popen.assert_called_once()
        child.terminate.assert_not_called()
        run_dir = self.out / "runs" / f"demo-native-{self.args.tag}"
        receipt = json.loads((run_dir / "run.json").read_text(encoding="utf-8"))
        script = (run_dir / "input.txt").read_text(encoding="utf-8")
        return receipt, script

    @unittest.skipUnless(os.name == "nt", "runner prepares Windows process flags")
    def test_checkpoint_shots_require_live_observation(self):
        self.args.checkpoint_shots = True
        for live, observe in ((False, 30), (True, None)):
            self.args.live_input, self.args.observe_until = live, observe
            with mock.patch.object(runner.subprocess, 'Popen') as popen:
                with self.assertRaises(ValueError):
                    runner.run(self.args)
                popen.assert_not_called()

    @unittest.skipUnless(os.name == "nt", "runner prepares Windows process flags")
    def test_checkpoint_shots_stay_in_fresh_private_user_and_are_not_measurement_acceptance(self):
        self.args.checkpoint_shots = True
        self.args.live_input = True
        self.args.observe_until = 30
        child = mock.Mock(pid=12345, returncode=0)
        child.poll.return_value = 0
        with mock.patch.object(runner.subprocess, 'Popen', return_value=child) as popen, mock.patch('builtins.print'):
            runner.run(self.args)
        run = self.out / 'runs/demo-native-test'
        receipt = json.loads((run / 'run.json').read_text())
        self.assertEqual(popen.call_args.kwargs['env']['DOLPHINXR_INPUT_CHECKPOINT_DIR'], str(run / 'User/StateSaves'))
        self.assertEqual(receipt['checkpoint_files'], {})
        self.assertFalse(receipt['accepted_as_measurement'])
        self.assertFalse(receipt['measurement_window_complete'])

    @unittest.skipUnless(os.name == "nt", "runner prepares Windows process flags")
    def test_end_capture_rejects_navigation_and_observation_modes_before_launch(self):
        self.args.end_capture = True
        for live, observe in ((True, None), (False, 30)):
            self.args.live_input, self.args.observe_until = live, observe
            with mock.patch.object(runner.subprocess, 'Popen') as popen:
                with self.assertRaises(ValueError):
                    runner.run(self.args)
                popen.assert_not_called()

    @unittest.skipUnless(os.name == "nt", "runner prepares Windows process flags")
    def test_end_capture_is_requested_only_after_completed_trace_window(self):
        self.args.end_capture = True
        self.args.start = 100.0
        run = self.out / 'runs/demo-native-test'
        child = mock.Mock(pid=12345, returncode=0)
        child.poll.return_value = None
        header = 'wall_ns,emulated_ticks,ticks_per_second\n'

        def spawn(*unused, **ignored):
            (run / 'frames.csv').write_text(header + '0,10000,100\n')
            (run / 'User/Logs/dolphin.log').write_text('AC stereo probe: OpenXR not initialized; synthetic eyes.\n')
            return child

        def advance(seconds):
            inbox = run / 'end-capture.txt'
            if any(line.startswith('shot ') for line in inbox.read_text().splitlines()):
                target = run / 'User/ScreenShots/demo/ac-window-end.png'
                target.parent.mkdir(parents=True, exist_ok=True)
                target.write_bytes(b'synthetic file-presence fixture, not PNG acceptance')
            else:
                self.assertFalse(list((run / 'User/ScreenShots').rglob('*.png')))
                (run / 'frames.csv').write_text(header + '0,10000,100\n65000000000,17000,100\n')

        with mock.patch.object(runner.subprocess, 'Popen', side_effect=spawn) as popen, \
                mock.patch.object(runner.time, 'monotonic', return_value=0), \
                mock.patch.object(runner.time, 'sleep', side_effect=advance), mock.patch('builtins.print'):
            runner.run(self.args)
        receipt = json.loads((run / 'run.json').read_text())
        self.assertEqual(receipt['end_capture_request'], {'after_window_wall_seconds': 65.0, 'at_emulated_s': 175.0})
        self.assertIn('shot 175.000 ac-window-end', (run / 'end-capture.txt').read_text())
        self.assertTrue(receipt['measurement_window_complete'])
        self.assertFalse(receipt['accepted_as_measurement'])
        self.assertIsNone(receipt['live_input_path'])
        self.assertEqual(len(receipt['end_capture_paths']), 1)
        self.assertEqual(popen.call_args.kwargs['env']['DOLPHINXR_INPUT_APPEND'], str(run / 'end-capture.txt'))
        child.terminate.assert_called_once()

    @unittest.skipUnless(os.name == "nt", "runner prepares Windows process flags")
    def test_end_capture_marker_aborts_owned_child_without_extra_flush(self):
        self.args.end_capture = True
        self.args.start = 100.0
        run = self.out / 'runs/demo-native-test'
        child = mock.Mock(pid=12345, returncode=0)
        child.poll.return_value = None
        header = 'wall_ns,emulated_ticks,ticks_per_second\n'
        def spawn(*unused, **ignored):
            (run / 'frames.csv').write_text(header + '0,10000,100\n')
            return child
        def advance(seconds):
            if seconds == 0.5:
                (run / 'frames.csv').write_text(header + '0,10000,100\n65000000000,17000,100\n')
            elif seconds == 0.2:
                self.marker.touch()
            else:
                self.fail('Headset marker must skip the extra screenshot flush')
        with mock.patch.object(runner.subprocess, 'Popen', side_effect=spawn), \
                mock.patch.object(runner.time, 'monotonic', return_value=0), \
                mock.patch.object(runner.time, 'sleep', side_effect=advance), mock.patch('builtins.print'):
            runner.run(self.args)
        receipt = json.loads((run / 'run.json').read_text())
        self.assertEqual(receipt['stop_reason'], 'headset_marker')
        self.assertFalse(receipt['measurement_window_complete'])
        self.assertEqual(receipt['end_capture_paths'], [])
        child.terminate.assert_called_once()

    @unittest.skipUnless(os.name == "nt", "runner prepares Windows process flags")
    def test_single_core_option_replaces_default_without_weakening_offline_guards(self):
        fragment = configparser.ConfigParser()
        fragment.read(WORKTREE / 'setups/dolphin-vr/metroid-prime-trilogy/single-core.ini')
        self.assertFalse(fragment.getboolean('Core', 'CPUThread'))
        manifest = json.loads(self.manifest_path.read_text())
        manifest['demo']['views']['3d']['config'] = ['Dolphin.Core.CPUThread=True']
        self.manifest_path.write_text(json.dumps(manifest))
        self.args.config = ['Dolphin.Core.CPUThread=' + fragment['Core']['CPUThread']]
        receipt, _ = self.run_with_fake_exited_child()
        self.assertEqual([x for x in receipt['argv'] if x.startswith('Dolphin.Core.CPUThread=')],
                         ['Dolphin.Core.CPUThread=False'])
        self.assertEqual(receipt['guard_config']['Dolphin.DSP.Muted'], 'True')
        self.assertEqual(receipt['guard_config']['Dolphin.Interface.UsePanicHandlers'], 'False')

    @unittest.skipUnless(os.name == "nt", "runner prepares Windows process flags")
    def test_inherited_checkpoint_request_is_removed_by_default(self):
        child = mock.Mock(pid=12345, returncode=0)
        child.poll.return_value = 0
        with mock.patch.dict(os.environ, {'DOLPHINXR_INPUT_SAVE_PATH': 'must-not-write',
                                         'DOLPHINXR_INPUT_SAVE_AT': '0',
                                         'DOLPHINXR_INPUT_CHECKPOINT_DIR': 'must-not-write',
                                         'AC_W2_PRESERVE_STEREO_XFB': '1'}), \
                mock.patch.object(runner.subprocess, 'Popen', return_value=child) as popen, \
                mock.patch('builtins.print'):
            runner.run(self.args)
        env = popen.call_args.kwargs['env']
        self.assertNotIn('DOLPHINXR_INPUT_SAVE_PATH', env)
        self.assertNotIn('DOLPHINXR_INPUT_SAVE_AT', env)
        self.assertNotIn('DOLPHINXR_INPUT_CHECKPOINT_DIR', env)
        self.assertNotIn('AC_W2_PRESERVE_STEREO_XFB', env)
        self.assertNotIn('-s', popen.call_args.args[0])

    @unittest.skipUnless(os.name == "nt", "runner prepares Windows process flags")
    def test_checkpoint_requires_observation_and_completion_gap(self):
        for at, until in ((10, None), (float('nan'), 30), (-1, 30), (26, 30)):
            with self.subTest(at=at, until=until):
                self.args.save_state_at = at
                self.args.observe_until = until
                with mock.patch.object(runner.subprocess, 'Popen') as popen:
                    with self.assertRaises(ValueError):
                        runner.run(self.args)
                    popen.assert_not_called()

    @unittest.skipUnless(os.name == "nt", "runner prepares Windows process flags")
    def test_checkpoint_output_is_fresh_run_local_and_never_timing_evidence(self):
        self.args.save_state_at = 15.0
        self.args.observe_until = 30.0
        child = mock.Mock(pid=12345, returncode=0)
        child.poll.return_value = 0
        with mock.patch.dict(os.environ, {'DOLPHINXR_INPUT_SAVE_PATH': 'inherited-invalid-path'}), \
                mock.patch.object(runner.subprocess, 'Popen', return_value=child) as popen, \
                mock.patch('builtins.print'):
            runner.run(self.args)
        env = popen.call_args.kwargs['env']
        state = self.out / 'runs/demo-native-test/User/StateSaves/route.sav'
        self.assertEqual(env['DOLPHINXR_INPUT_SAVE_PATH'], str(state))
        self.assertEqual(env['DOLPHINXR_INPUT_SAVE_AT'], '15.0')
        receipt = json.loads((state.parents[2] / 'run.json').read_text())
        self.assertFalse(receipt['save_state_written'])
        self.assertFalse(receipt['measurement_window_complete'])
        self.assertFalse(receipt['accepted_as_measurement'])

    @unittest.skipUnless(os.name == "nt", "runner prepares Windows process flags")
    def test_load_state_is_explicit_and_hashed_without_copying_or_modifying_it(self):
        state = self.out / 'chosen.sav'
        state.write_bytes(b'synthetic checkpoint')
        self.args.load_state = str(state)
        child = mock.Mock(pid=12345, returncode=0)
        child.poll.return_value = 0
        with mock.patch.object(runner.subprocess, 'Popen', return_value=child) as popen, \
                mock.patch('builtins.print'):
            runner.run(self.args)
        argv = popen.call_args.args[0]
        self.assertEqual(argv[argv.index('-s') + 1], str(state))
        receipt = json.loads((self.out / 'runs/demo-native-test/run.json').read_text())
        self.assertEqual(receipt['load_state_sha256'], runner.digest(state))
        self.assertEqual(state.read_bytes(), b'synthetic checkpoint')

    @unittest.skipUnless(os.name == "nt", "runner prepares Windows process flags")
    def test_load_state_rejects_nonprivate_path_before_launch(self):
        self.args.load_state = str(self.script)
        # Fixture root itself is .local, so use an existing standard-library file.
        self.args.load_state = str(Path(runner.os.__file__).resolve())
        with mock.patch.object(runner.subprocess, 'Popen') as popen:
            with self.assertRaises(ValueError):
                runner.run(self.args)
            popen.assert_not_called()

    @unittest.skipUnless(os.name == "nt", "runner prepares Windows process flags")
    def test_before_window_excludes_original_and_generated_timing_captures(self):
        self.args.start = 40.0
        self.args.capture_policy = "before-window"
        self.script.write_text(
            "0 100 R primary 1\nshot 5 original-before\nshot 35 too-close\n"
            "shot 50 during-window\nshot 70 during-window-later\n", encoding="utf-8")

        receipt, script = self.run_with_fake_exited_child()

        scheduled = receipt["scheduled_capture_emulated_seconds"]
        emitted = [float(line.split()[1]) for line in script.splitlines()
                   if line.startswith("shot ")]
        self.assertEqual(scheduled, [5.0, 8.0, 20.0, 30.0])
        self.assertEqual(emitted, scheduled)
        self.assertTrue(all(0 <= value <= self.args.start - 10 for value in scheduled))
        self.assertFalse(any(self.args.start <= value < self.args.start + 60 for value in emitted))
        self.assertIn("0 100 R primary 1", script)
        self.assertEqual(receipt["capture_policy"], "before-window")
        self.assertIn("asynchronous", receipt["capture_overhead_note"])

    @unittest.skipUnless(os.name == "nt", "runner prepares Windows process flags")
    def test_before_window_zero_anchor_schedules_no_capture_at_measurement_start(self):
        self.args.start = 0.0
        self.args.capture_policy = "before-window"

        receipt, script = self.run_with_fake_exited_child()

        self.assertEqual(receipt["scheduled_capture_emulated_seconds"], [])
        self.assertFalse(any(line.startswith("shot ") for line in script.splitlines()))

    @unittest.skipUnless(os.name == "nt", "runner prepares Windows process flags")
    def test_after_phase_and_sampled_capture_caveat_are_recorded(self):
        self.args.phase = "after"

        receipt, _ = self.run_with_fake_exited_child()

        self.assertEqual(receipt["phase"], "after")
        self.assertEqual(receipt["capture_policy"], "sampled")
        self.assertIn("may interrupt the measured interval", receipt["capture_overhead_note"])
        self.assertIn(10, receipt["scheduled_capture_emulated_seconds"])
        self.assertIn(40, receipt["scheduled_capture_emulated_seconds"])
        self.assertFalse(receipt["accepted_as_measurement"])
        self.assertIsNone(receipt["observe_until_emulated_s"])

    @unittest.skipUnless(os.name == "nt", "runner prepares Windows process flags")
    def test_observation_filters_captures_and_keeps_five_second_settling_gap(self):
        self.args.start = 40.0
        self.args.observe_until = 35.0
        self.script.write_text("shot 5 early\nshot 35 stop-time\nshot 50 late\n", encoding="utf-8")

        receipt, script = self.run_with_fake_exited_child()

        self.assertEqual(receipt["scheduled_capture_emulated_seconds"], [5.0, 8.0, 20.0, 30.0])
        self.assertEqual(receipt["observe_until_emulated_s"], 35.0)
        self.assertFalse(receipt["measurement_window_complete"])
        self.assertFalse(receipt["accepted_as_measurement"])
        self.assertIn("Observation only", receipt["capture_overhead_note"])
        self.assertNotIn("shot 35.000", script)

    @unittest.skipUnless(os.name == "nt", "runner prepares Windows process flags")
    def test_observation_stops_owned_mock_child_without_claiming_measurement(self):
        self.args.observe_until = 100.0
        run_dir = self.out / "runs" / "demo-native-test"
        child = mock.Mock(pid=12345, returncode=0)
        child.poll.side_effect = [None, None, 0]

        def spawn(*unused, **ignored):
            (run_dir / "frames.csv").write_text(
                "wall_ns,emulated_ticks,ticks_per_second\n90000000000,12000,100\n", encoding="utf-8")
            (run_dir / "User/Logs/dolphin.log").write_text(
                "AC stereo probe: OpenXR not initialized; synthetic eyes.\n", encoding="utf-8")
            return child

        with mock.patch.object(runner.subprocess, "Popen", side_effect=spawn) as popen, \
                mock.patch.object(runner.time, "monotonic", return_value=0), \
                mock.patch.object(runner.time, "sleep") as sleep, mock.patch("builtins.print") as output:
            runner.run(self.args)

        popen.assert_called_once()
        sleep.assert_any_call(3)
        child.terminate.assert_called_once()
        child.wait.assert_called_once_with(timeout=20)
        receipt = json.loads((run_dir / "run.json").read_text(encoding="utf-8"))
        self.assertEqual(receipt["stop_reason"], "observation_end")
        self.assertEqual(receipt["observe_until_emulated_s"], 100.0)
        self.assertFalse(receipt["measurement_window_complete"])
        self.assertFalse(receipt["accepted_as_measurement"])
        self.assertIn('"stop_reason": "observation_end"', output.call_args.args[0])

    @unittest.skipUnless(os.name == "nt", "runner prepares Windows process flags")
    def test_invalid_observation_time_rejects_before_preparation(self):
        for value in (4.99, -1, float("inf"), float("nan")):
            with self.subTest(value=value), mock.patch.object(runner.subprocess, "Popen") as popen:
                self.args.observe_until = value
                with self.assertRaisesRegex(ValueError, "finite and at least 5"):
                    runner.run(self.args)
                popen.assert_not_called()
                self.assertFalse((self.out / "runs").exists())

    def test_observation_cli_value_rejects_nonfinite_or_short_values(self):
        self.assertEqual(runner.observation_time("5"), 5.0)
        for value in ("4", "nan", "inf", "-inf"):
            with self.subTest(value=value), self.assertRaises(runner.argparse.ArgumentTypeError):
                runner.observation_time(value)

    @unittest.skipUnless(os.name == "nt", "runner prepares Windows process flags")
    def test_live_input_missing_ready_ack_stops_owned_child(self):
        self.args.live_input = True
        self.args.observe_until = 100
        self.args.seconds = 120
        run_dir = self.out / "runs" / "demo-native-test"
        child = mock.Mock(pid=12345, returncode=0)
        child.poll.return_value = None

        def spawn(*unused, **kwargs):
            self.assertEqual(kwargs['env']['DOLPHINXR_INPUT_APPEND'], str(run_dir / 'live-input.txt'))
            self.assertTrue((run_dir / 'live-input.txt').is_file())
            (run_dir / 'User/Logs/dolphin.log').write_text(
                'AC stereo probe: OpenXR not initialized; synthetic eyes.\n', encoding='utf-8')
            return child

        clock = iter([0, 31, 31, 31])
        with mock.patch.object(runner.subprocess, 'Popen', side_effect=spawn), \
                mock.patch.object(runner.time, 'monotonic', side_effect=lambda: next(clock)), \
                mock.patch.object(runner.time, 'sleep', side_effect=AssertionError('must stop at missing ACK')), \
                mock.patch('builtins.print'):
            runner.run(self.args)

        child.terminate.assert_called_once()
        child.wait.assert_called_once_with(timeout=20)
        record = json.loads((run_dir / 'run.json').read_text(encoding='utf-8'))
        self.assertEqual(record['stop_reason'], 'missing_live_input_guard')
        self.assertFalse(record['measurement_window_complete'])
        self.assertFalse(record['accepted_as_measurement'])

    @unittest.skipUnless(os.name == "nt", "runner prepares Windows process flags")
    def test_live_input_ready_ack_then_run_local_stop(self):
        self.args.live_input = True
        self.args.observe_until = 100
        self.args.seconds = 120
        run_dir = self.out / 'runs' / 'demo-native-test'
        child = mock.Mock(pid=12345, returncode=0)
        child.poll.return_value = None

        def spawn(*unused, **ignored):
            (run_dir / 'User/Logs/dolphin.log').write_text(
                'AC stereo probe: OpenXR not initialized; synthetic eyes.\n', encoding='utf-8')
            (run_dir / 'probe.log').write_text('t=0.0 AC_INPUT_APPEND ready\n', encoding='utf-8')
            return child

        def request_stop(unused):
            self.assertFalse((run_dir / 'STOP').exists(), 'runner failed to honor STOP')
            (run_dir / 'STOP').write_text('test-owned stop request', encoding='utf-8')

        clock = iter([0, 31, 31, 31])
        with mock.patch.object(runner.subprocess, 'Popen', side_effect=spawn), \
                mock.patch.object(runner.time, 'monotonic', side_effect=lambda: next(clock)), \
                mock.patch.object(runner.time, 'sleep', side_effect=request_stop), mock.patch('builtins.print'):
            runner.run(self.args)

        child.terminate.assert_called_once()
        child.wait.assert_called_once_with(timeout=20)
        record = json.loads((run_dir / 'run.json').read_text(encoding='utf-8'))
        self.assertEqual(record['stop_reason'], 'operator_stop')
        self.assertEqual(record['operator_stop_path'], str(run_dir / 'STOP'))
        self.assertEqual(record['live_input_sha256'], runner.digest(run_dir / 'live-input.txt'))
        self.assertFalse(record['measurement_window_complete'])
        self.assertFalse(record['accepted_as_measurement'])

    @unittest.skipUnless(os.name == "nt", "runner launches only on Windows")
    def test_live_input_requires_observation_mode_before_process_creation(self):
        self.args.live_input = True
        self.args.observe_until = None
        with mock.patch.object(runner.subprocess, "Popen") as popen:
            with self.assertRaisesRegex(ValueError, "requires --observe-until"):
                runner.run(self.args)
        popen.assert_not_called()

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
