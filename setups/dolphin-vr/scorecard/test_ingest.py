"""Synthetic CPU-only immutable-evidence checks; no process or runtime launch."""
import contextlib
import csv
import hashlib
import io
import json
from pathlib import Path
import tempfile
import unittest
from unittest import mock

import ingest_run
import scorecard


class IngestTests(unittest.TestCase):
    def setUp(self):
        private = Path(__file__).resolve().parents[3] / '.local' / 'ingest-tests'
        private.mkdir(parents=True, exist_ok=True)
        self.temp = tempfile.TemporaryDirectory(dir=private)
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name)
        self.run = self.root / 'run'
        self.run.mkdir()
        self.marker = self.root / 'HEADSET-ON'
        self.record = dict(marker=str(self.marker), synthetic_guard_logged=True,
                           measurement_window_complete=True, window_start_emulated_s=0,
                           rate='native', game_key='synthetic', view='3d', phase='baseline',
                           exe_sha256='a' * 64, fifo_error_mentions=0)
        self.write_record()
        (self.run / 'input.txt').write_text('shot 10 synthetic-checkpoint\n', encoding='utf-8')
        # Deliberately preserve older mutable-name artifacts without reading or replacing them.
        (self.run / 'analysis.json').write_bytes(b'historical analysis bytes')
        (self.run / 'frames.receipt.json').write_bytes(b'historical receipt bytes')
        self.rows = [dict(wall_ns=i * 1000000000, emulated_ticks=i * 1000000,
                          ticks_per_second=1000000, duplicate=0, reason='vi', texture_id=i,
                          layers=2, width=1920, height=1440, vi_width=640, vi_height=480,
                          native_vi_hz=60.0, forced_vbi_frequency=0,
                          effective_vbi_target_hz=0.0, vi_overclock_enabled=0, vi_overclock=1.0)
                     for i in range(62)]
        self.write_trace()
        manifest = self.root / 'manifest.json'
        manifest.write_text(json.dumps({'synthetic': {'title': 'Synthetic', 'views': {'3d': {}}}}), encoding='utf-8')
        self.card = scorecard.initialize(manifest)
        self.imported = []

    def write_record(self):
        (self.run / 'run.json').write_text(json.dumps(self.record), encoding='utf-8')

    def write_trace(self):
        with (self.run / 'frames.csv').open('w', newline='', encoding='utf-8') as handle:
            writer = csv.DictWriter(handle, fieldnames=list(self.rows[0]))
            writer.writeheader()
            writer.writerows(self.rows)

    def ingest(self, stage=None):
        def import_without_process(command, **kwargs):
            self.assertTrue(kwargs['check'])
            receipt = Path(command[command.index('--receipt') + 1])
            self.imported.append(receipt)
            scorecard.import_receipt(self.card, receipt)
        args = [str(self.run), '--out', str(self.root / '.local' / 'scorecard')]
        if stage:
            args += ['--stage', stage]
        with mock.patch.object(ingest_run.subprocess, 'run', side_effect=import_without_process), \
                contextlib.redirect_stdout(io.StringIO()):
            ingest_run.main(args)

    def snapshots(self):
        return {path.name: path.read_bytes() for path in self.run.iterdir() if path.is_file()}

    def test_unchanged_ingest_reuses_files_and_scorecard_deduplicates(self):
        self.ingest()
        first = self.snapshots()
        self.ingest()
        self.assertEqual(self.snapshots(), first)
        self.assertEqual(self.imported[0], self.imported[1])
        runs = self.card['games']['synthetic']['views']['3d']['runs']
        self.assertEqual(len(runs), 1)
        self.assertEqual(first['analysis.json'], b'historical analysis bytes')
        self.assertEqual(first['frames.receipt.json'], b'historical receipt bytes')
        self.assertEqual(runs[0]['receipt_sha256'], hashlib.sha256(self.imported[0].read_bytes()).hexdigest())
        for path, sha256 in runs[0]['evidence_sha256'].items():
            self.assertEqual(hashlib.sha256(Path(path).read_bytes()).hexdigest(), sha256)
            self.assertIn(sha256, Path(path).name)

    def test_changed_stage_creates_new_receipt_and_preserves_analysis(self):
        self.ingest('menu')
        first_receipt = self.imported[-1]
        first_bytes = first_receipt.read_bytes()
        self.ingest('gameplay')
        self.assertNotEqual(first_receipt, self.imported[-1])
        self.assertEqual(first_receipt.read_bytes(), first_bytes)
        self.assertEqual(len(list(self.run.glob('analysis-*.json'))), 1)
        runs = self.card['games']['synthetic']['views']['3d']['runs']
        self.assertEqual([run['stage'] for run in runs], ['menu', 'gameplay'])

    def test_changed_trace_and_record_cannot_change_previous_evidence(self):
        self.ingest()
        receipt = scorecard.read_json(self.imported[-1])
        previous = {path: Path(path).read_bytes() for path in receipt['evidence_paths']}
        self.rows[30]['layers'] = 1
        self.write_trace()
        self.record['fifo_error_mentions'] = 1
        self.write_record()
        self.ingest()
        self.assertNotEqual(self.imported[0], self.imported[1])
        self.assertEqual(len(list(self.run.glob('analysis-*.json'))), 2)
        for path, data in previous.items():
            self.assertEqual(Path(path).read_bytes(), data)
        latest = scorecard.read_json(self.imported[-1])
        self.assertEqual(latest['metrics']['frames']['native']['fresh_mono_or_blank_xfb_frames'], 1)

    def test_tampered_existing_content_address_is_rejected_without_overwrite(self):
        path = ingest_run.immutable_bytes(self.run, 'fixture', b'original', '.txt')
        path.write_bytes(b'tampered')
        with self.assertRaisesRegex(ValueError, 'different bytes'):
            ingest_run.immutable_bytes(self.run, 'fixture', b'original', '.txt')
        self.assertEqual(path.read_bytes(), b'tampered')

    def test_observation_and_headset_gates_write_no_snapshots(self):
        self.record['measurement_window_complete'] = False
        self.write_record()
        before = self.snapshots()
        with self.assertRaisesRegex(SystemExit, 'complete measurement-window'):
            self.ingest()
        self.assertEqual(self.snapshots(), before)
        self.record['measurement_window_complete'] = True
        self.write_record()
        before = self.snapshots()
        self.marker.write_text('present', encoding='utf-8')
        with self.assertRaisesRegex(SystemExit, 'Headset marker'):
            self.ingest()
        self.assertEqual(self.snapshots(), before)


if __name__ == '__main__':
    unittest.main()
