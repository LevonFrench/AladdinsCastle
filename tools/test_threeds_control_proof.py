"""The 3DS control proof's script writer and log reader; no game, no emulator."""
import unittest
from unittest.mock import patch
from types import SimpleNamespace
from pathlib import Path
import contextlib
import io
import json
import subprocess
import sys
import tempfile
import zlib
import struct

import threeds_control_proof as proof


class ControlProofTests(unittest.TestCase):
    def test_every_trial_snaps_before_and_after_in_frame_order(self):
        for name, lines in proof.TRIALS.items():
            frames = [int(l.split()[0]) for l in proof.script_text(lines).splitlines()]
            self.assertEqual(frames, sorted(frames), name)
            self.assertLessEqual(frames[-1], proof.FRAMES, name)
            text = proof.script_text(lines)
            for snap, _ in proof.SNAPS:
                self.assertIn("snap " + snap, text, name)

    def test_the_two_idle_runs_have_no_input(self):
        self.assertEqual(proof.TRIALS["none1"], [])
        self.assertEqual(proof.TRIALS["none2"], [])

    def test_inputs_are_released_before_the_end_snapshot(self):
        for name, lines in proof.TRIALS.items():
            held = {}
            for l in lines:
                frame, cmd, *args = l.split()
                if cmd in ("rtrig", "ltrig", "rgrip", "lgrip"):
                    held[cmd] = args[0] != "0"
                elif cmd in ("lstick", "rstick"):
                    held[cmd] = any(float(a) != 0 for a in args)
                if int(frame) >= dict(proof.SNAPS)["end"]:
                    self.fail(name + " changes input after the end snapshot")
            self.assertFalse(any(held.values()), name + " leaves an input held")

    def test_pad_line_is_decoded_to_3ds_button_names(self):
        log = "  frame 30 pad: buttons 0x0300 left 0.000 0.000 right 0.000 0.000\n  frame 60 pad: buttons 0x0008 left -1.000 0.000 right 0.000 0.000 marker 1 2\n"
        rows = proof.parse_pad_log(log)
        self.assertEqual(proof.decode_pad(rows[30][0]), ["A", "X"])
        self.assertEqual(proof.decode_pad(rows[60][0]), ["Start"])
        self.assertEqual(rows[60][1], -1.0)


class RunnerTests(unittest.TestCase):
    """Real short-lived fake children; never an emulator or user game/state."""
    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory()
        self.addCleanup(self.tmp.cleanup)
        self.out = Path(self.tmp.name) / "proof"
        self.args = SimpleNamespace(host=sys.executable, core="fake-core", game="fake-game",
                                    state="fake-state", profile="fake-profile", out=str(self.out), timeout=2)
        self.mode = "ok"
        self.children = []
        real_popen = subprocess.Popen
        def chunk(kind, data):
            return struct.pack('>I', len(data)) + kind + data + struct.pack('>I', zlib.crc32(kind + data))
        png = (b'\x89PNG\r\n\x1a\n' + chunk(b'IHDR', struct.pack('>2I5B', 640, 540, 8, 2, 0, 0, 0))
               + chunk(b'IDAT', zlib.compress((b'\0' + b'\0' * (640 * 3)) * 540)) + chunk(b'IEND', b''))
        def spawn(command, **kwargs):
            snaps = command[command.index('--snapshots') + 1]
            log = command[command.index('--log') + 1]
            code = ("import pathlib,sys,time\n"
                    f"mode={self.mode!r}\n"
                    "if mode=='crash':sys.exit(7)\n"
                    "if mode=='timeout':time.sleep(5)\n"
                    f"folder=pathlib.Path({snaps!r})\n"
                    f"png={png!r}\n"
                    "for name in ('pre','mid','late','end'):\n"
                    "    if mode!='missing-snapshot' or name!='mid':(folder/(name+'.png')).write_bytes(png)\n"
                    f"pathlib.Path({log!r}).write_text('no pad sample' if mode=='missing-pad' else "
                    "'frame 30 pad: buttons 0x0000 left 0 0 right 0 0\\n')\n")
            p = real_popen([sys.executable, '-B', '-c', code], **kwargs)
            self.children.append(p)
            return p
        self.addCleanup(patch.stopall)
        patch.object(proof, 'TRIALS', {'none1': [], 'none2': [], 'laser': ['10 rtrig 1', '50 rtrig 0']}).start()
        patch.object(proof.subprocess, 'Popen', side_effect=spawn).start()

    def status(self):
        return json.loads((self.out/'run-status.json').read_text())

    def test_success_requires_all_trials_snapshots_and_pad_samples(self):
        self.assertEqual(proof.run(self.args), 0)
        self.assertEqual(proof.validate_run(self.out)['state'], 'complete')
        self.assertEqual(len(self.children), 3)

    def test_child_crash_fails_fast_and_records_nonzero(self):
        self.mode = 'crash'
        with self.assertRaisesRegex(proof.ProofError, 'none1.*code 7'):
            proof.run(self.args)
        self.assertEqual(self.status()['state'], 'failed')
        self.assertEqual(self.status()['trials']['none1']['exit_code'], 7)
        self.assertEqual(len(self.children), 1)

    def test_timeout_is_killed_reaped_and_fails(self):
        self.mode = 'timeout'
        self.args.timeout = .1
        with self.assertRaisesRegex(proof.ProofError, 'none1.*timed out'):
            proof.run(self.args)
        self.assertIsNotNone(self.children[0].poll())
        self.assertEqual(self.status()['trials']['none1']['state'], 'timeout')

    def test_incomplete_successful_child_is_not_a_pass(self):
        for mode, expected in [('missing-snapshot', 'snapshot mid'), ('missing-pad', 'midpoint pad')]:
            with self.subTest(mode=mode):
                self.args.out = str(self.out.parent/mode)
                self.mode = mode
                with self.assertRaisesRegex(proof.ProofError, expected):
                    proof.run(self.args)
                state=json.loads((Path(self.args.out)/'run-status.json').read_text())
                self.assertEqual(state['state'], 'failed')

    def test_existing_evidence_is_preserved_and_not_reused(self):
        self.out.mkdir()
        old=self.out/'old-evidence.txt'
        old.write_text('preserve')
        with self.assertRaisesRegex(proof.ProofError, 'fresh folder'):
            proof.run(self.args)
        self.assertEqual(old.read_text(), 'preserve')
        self.assertFalse(self.children)

    def test_report_revalidates_evidence_and_refuses_failed_run(self):
        proof.run(self.args)
        (self.out/'laser'/'host.log').write_text('missing pad')
        with self.assertRaisesRegex(proof.ProofError, 'midpoint pad'):
            proof.report(SimpleNamespace(out=str(self.out)))
        state=self.status();state['state']='failed'
        proof.write_status(self.out,state)
        with self.assertRaisesRegex(proof.ProofError, 'not complete'):
            proof.report(SimpleNamespace(out=str(self.out)))

    def test_command_line_returns_failure_for_child_error(self):
        self.mode='crash'
        argv=['run','--host',sys.executable,'--core','fake','--game','fake','--state','fake',
              '--profile','fake','--out',str(self.out)]
        error=io.StringIO()
        with contextlib.redirect_stderr(error):
            self.assertEqual(proof.main(argv),1)
        self.assertIn('none1: child exited with code 7',error.getvalue())

    def test_successful_report_uses_current_complete_run(self):
        try:
            import PIL  # noqa: F401
        except ImportError:
            self.skipTest('Pillow is optional for report rendering')
        proof.run(self.args)
        proof.report(SimpleNamespace(out=str(self.out)))
        self.assertTrue((self.out/'controls-sheet.png').is_file())


if __name__ == "__main__":
    unittest.main()
