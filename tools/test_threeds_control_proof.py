"""The 3DS control proof's script writer and log reader; no game, no emulator."""
import unittest

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


if __name__ == "__main__":
    unittest.main()
