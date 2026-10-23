import unittest
from analyze_metrics import analyze
from apply_metrics import patch


def stream(rate=60, mono=False, duplicates=False):
    return [dict(wall_ns=round(i * 1e9 / rate), emulated_ticks=round(i * 1e6 / rate),
                 ticks_per_second=1000000, duplicate=int(duplicates and i % 2 == 1),
                 reason='vi', texture_id=i // 2 if duplicates else i, layers=1 if mono else 2,
                 width=1920, height=1440, vi_width=640, vi_height=480,
                 native_vi_hz=60.0, forced_vbi_frequency=0, effective_vbi_target_hz=0.0,
                 vi_overclock_enabled=0, vi_overclock=1.0)
            for i in range(rate * 61 + 1)]


class MetricsTests(unittest.TestCase):
    def test_full_minute_and_repeated_deadlines(self):
        report = analyze(stream(), 60, start=0)
        self.assertEqual(report['observed']['fresh_stereo_eligible'], 3600)
        for rate in (72, 90):
            cadence = report['cadences'][str(rate)]
            self.assertEqual(cadence['expected_deadlines'], rate * 60)
            self.assertGreater(cadence['modeled_held_source_deadlines'], 0)
            self.assertEqual(cadence['actual_delivered_frames'], None)
            self.assertEqual(sum(cadence[k] for k in ('modeled_new_source_deadlines', 'modeled_held_source_deadlines', 'modeled_no_source_deadlines')), rate * 60)

    def test_duplicates_do_not_become_fresh_stereo(self):
        report = analyze(stream(duplicates=True), 30, start=0)
        self.assertEqual(report['observed']['fresh_stereo_eligible'], 1800)
        self.assertEqual(report['observed']['duplicate_xfb'], 1800)

    def test_mono_never_delivers_a_stereo_source(self):
        report = analyze(stream(mono=True), 60, start=0)
        self.assertEqual(report['observed']['fresh_stereo_eligible'], 0)
        self.assertEqual(report['cadences']['90']['modeled_no_source_deadlines'], 5400)

    def test_partial_window_rejected(self):
        with self.assertRaisesRegex(ValueError, 'complete consecutive'):
            analyze(stream()[:3600], 60, start=0)

    def test_warmup_excluded(self):
        report = analyze(stream(), 60, start=1)
        self.assertEqual(report['observed']['fresh_stereo_eligible'], 3600)

    def test_nominal_inference_and_emulated_gameplay_anchor(self):
        report = analyze(stream(), start_emulated=1)
        self.assertEqual(report['window']['start_wall_seconds'], 1)
        self.assertEqual(report['observed']['fresh_stereo_eligible'], 3600)
        self.assertEqual(report['cadences']['native']['rate_hz'], 60)
        self.assertEqual(report['actual_game_fps'], 'unmeasured')

    def test_unreached_gameplay_anchor_rejected(self):
        with self.assertRaisesRegex(ValueError, 'not reached'):
            analyze(stream(), start_emulated=200)

    def test_patch_ambiguity_fails_before_any_write(self):
        with self.assertRaisesRegex(ValueError, 'anchor expected once'):
            patch('not the expected source')


if __name__ == '__main__':
    unittest.main()
