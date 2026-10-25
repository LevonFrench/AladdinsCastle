"""Synthetic diagnostics. Run only after the sequential timing queue is idle."""
import json
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest
from unittest.mock import patch

import numpy as np
from PIL import Image

from measure_rois import match_patch, measure, summarize, validate_layout

TEMP_ROOT = Path(__file__).resolve().parents[3] / '.local' / 'roi-tests'


class DisparityROITests(unittest.TestCase):
    def setUp(self):
        TEMP_ROOT.mkdir(parents=True, exist_ok=True)

    def texture(self):
        return np.random.default_rng(1701).integers(0, 256, (96, 192)).astype(np.float32)

    def shifted(self, left, disparity):
        right = np.zeros_like(left)
        if disparity > 0:
            right[:, :-disparity] = left[:, disparity:]
        elif disparity < 0:
            right[:, -disparity:] = left[:, :disparity]
        else:
            right[:] = left
        return right

    def test_positive_and_negative_shift_preserved(self):
        left = self.texture()
        for disparity in (7, -7, 0):
            with self.subTest(disparity=disparity):
                result, reason = match_patch(left, self.shifted(left, disparity), 72, 24,
                                             size=16, max_disparity=24)
                self.assertIsNone(reason)
                self.assertEqual(result['d_px'], disparity)
                self.assertEqual(result['reverse_d_px'], -disparity)

    def test_periodic_texture_is_ambiguous(self):
        left = np.tile(np.array([0, 255, 0, 255], dtype=np.float32), (96, 48))
        result, reason = match_patch(left, left, 72, 24, size=16, max_disparity=24)
        self.assertIsNone(result)
        self.assertEqual(reason, 'ambiguous')

    def test_occluded_patch_rejected(self):
        left = self.texture()
        result, reason = match_patch(left, np.zeros_like(left), 72, 24,
                                     size=16, max_disparity=24)
        self.assertIsNone(result)
        self.assertIsNotNone(reason)

    def test_reverse_inconsistent_match_rejected(self):
        left = self.texture()
        with patch('measure_rois.best_shift', side_effect=[
                ({'d_px': 6, 'sad': 1.0, 'uniqueness': 0.8}, None),
                ({'d_px': 0, 'sad': 1.0, 'uniqueness': 0.8}, None)]):
            result, reason = match_patch(left, left, 72, 24, size=16, max_disparity=24)
        self.assertIsNone(result)
        self.assertEqual(reason, 'reverse_inconsistent_or_occluded')

    def test_search_limit_is_not_a_measurement(self):
        left = self.texture()
        result, reason = match_patch(left, self.shifted(left, 24), 72, 24,
                                     size=16, max_disparity=24)
        self.assertIsNone(result)
        self.assertEqual(reason, 'search_boundary')

    def test_no_matches_is_unknown(self):
        result = summarize([], 1824)
        self.assertEqual(result['accepted'], 0)
        for key in ('median_px', 'p25_px', 'p75_px', 'min_px', 'max_px', 'median_at_1920_px'):
            self.assertIsNone(result[key])

    def test_mono_and_odd_layout_rejected(self):
        with self.assertRaisesRegex(ValueError, 'exactly two'):
            validate_layout(3840, 1440, 'sbs', 1)
        with self.assertRaisesRegex(ValueError, 'even'):
            validate_layout(3839, 1440, 'sbs', 2)

    def test_blank_image_and_provenance_mismatch(self):
        with tempfile.TemporaryDirectory(dir=TEMP_ROOT) as temporary:
            path = Path(temporary) / 'blank.png'
            Image.new('RGB', (256, 128), 'black').save(path)
            rois = [{'name': 'caller label only', 'rect': [0.25, 0.25, 0.75, 0.75]}]
            result = measure(path, rois, 'sbs', 2, patch_size=16, max_disparity=24)
            self.assertEqual(result['rois'][0]['summary']['accepted'], 0)
            self.assertIsNone(result['rois'][0]['summary']['median_px'])
            with self.assertRaisesRegex(ValueError, 'provenance'):
                measure(path, rois, 'sbs', 2, provenance={'capture_sha256': 'wrong'})

    def test_original_cli_textureless_statistics_are_null(self):
        script = Path(__file__).resolve().parents[1] / 'stereo-probe' / 'disparity.py'
        with tempfile.TemporaryDirectory(dir=TEMP_ROOT) as temporary:
            path = Path(temporary) / 'blank.png'
            Image.new('RGB', (256, 128), 'black').save(path)
            process = subprocess.run([sys.executable, '-B', str(script), str(path)],
                                     capture_output=True, text=True, check=True, timeout=30)
            result = json.loads(process.stdout)
        self.assertEqual(result['confident'], 0)
        for key in ('d_median', 'd_min', 'd_max', 'd_p10', 'd_p90', 'negative_share', 'zero_share'):
            self.assertIsNone(result[key])


if __name__ == '__main__':
    unittest.main()
