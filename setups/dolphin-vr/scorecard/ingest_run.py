"""Turn a reviewed-run trace into an E2 source-availability receipt.

Stage is deliberately optional: completing a replay does not establish gameplay.
This records XFB metadata, not GPU completion, content FPS or compositor delivery.
"""
import argparse
import hashlib
import json
from pathlib import Path
import subprocess
import sys

from analyze_metrics import analyze, read_rows


def immutable_bytes(directory, stem, data, suffix):
    """Create content-addressed evidence once; never overwrite existing bytes."""
    sha256 = hashlib.sha256(data).hexdigest()
    path = directory / (stem + '-' + sha256 + suffix)
    try:
        with path.open('xb') as handle:
            handle.write(data)
    except FileExistsError:
        if path.read_bytes() != data:
            raise ValueError('Immutable evidence exists with different bytes: ' + str(path))
    return path


def immutable_json(directory, stem, value):
    data = (json.dumps(value, sort_keys=True, indent=2, allow_nan=False) + '\n').encode('utf-8')
    return immutable_bytes(directory, stem, data, '.json')


def main(argv=None):
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument('run', type=Path)
    ap.add_argument('--out', type=Path, required=True)
    ap.add_argument('--stage', choices=('title', 'menu', 'gameplay'))
    args = ap.parse_args(argv)
    run = args.run.resolve()
    record_bytes = (run / 'run.json').read_bytes()
    record = json.loads(record_bytes.decode('utf-8-sig'))
    if Path(record['marker']).exists():
        raise SystemExit('Headset marker present; no analysis run.')
    if not record.get('synthetic_guard_logged') or not record.get('measurement_window_complete'):
        raise SystemExit('Run failed the synthetic guard or complete measurement-window gate.')
    # Pin the exact inputs being analyzed. The original run files and older
    # analysis/receipt names remain untouched, even when a later ingest differs.
    record_path = immutable_bytes(run, 'run-source', record_bytes, '.json')
    trace_path = immutable_bytes(run, 'frames-source', (run / 'frames.csv').read_bytes(), '.csv')
    evidence = [str(record_path), str(trace_path)]
    rows = read_rows(trace_path)
    result = analyze(rows, start_emulated=record['window_start_emulated_s'])
    rate = record['rate']
    expected_forced = 0 if rate == 'native' else int(rate)
    if result['forced_vbi_frequency_observed'] != [expected_forced]:
        raise SystemExit('Observed forced VBI differs from declared run rate.')
    if result['effective_vbi_target_hz_observed'] != [float(expected_forced)]:
        raise SystemExit('Observed effective VBI differs from declared run rate.')
    if result['vi_overclock_enabled_observed'] != [0]:
        raise SystemExit('Unexpected VI overclock in measured window.')
    start = result['window']['start_wall_seconds'] * 1e9
    window = [r for r in rows if start <= r['wall_ns'] < start + 60e9]
    observed = result['observed']
    # Old receipts predate capture-policy metadata. Recover the actual schedule
    # from their preserved replay, never assume their timing was capture-free.
    scheduled_shots = record.get('scheduled_capture_emulated_seconds')
    if scheduled_shots is None:
        input_bytes = (run / 'input.txt').read_bytes()
        input_path = immutable_bytes(run, 'input-source', input_bytes, '.txt')
        evidence.append(str(input_path))
        scheduled_shots = [float(line.split()[1])
                           for line in input_bytes.decode('utf-8-sig').splitlines()
                           if line.strip().startswith('shot ')]
    first_emu = window[0]['emulated_ticks'] / window[0]['ticks_per_second']
    last_emu = window[-1]['emulated_ticks'] / window[-1]['ticks_per_second']
    shots_in_window = [t for t in scheduled_shots if first_emu <= t <= last_emu]
    target_hz = result['nominal_vi_hz_observed'][0] if rate == 'native' else int(rate)
    frame = {
        'counter_scope': 'offline XFB source events before headless Present; no GPU-completion or XR delivery counter',
        'target_hz': target_hz, 'target_source': result['native_cadence_source'] if rate == 'native' else 'explicit forced VI experiment',
        'wall_duration_s': 60,
        'emulated_duration_s': observed['emulated_seconds_between_bracketing_events'],
        'present_count': observed['events'],
        'eligible_stereo_frame_count': sum(r['layers'] >= 2 for r in window),
        'layers_min': min(r['layers'] for r in window),
        'layers_max': max(r['layers'] for r in window),
        'fresh_xfb_frames': observed['fresh_xfb'],
        'fresh_stereo_xfb_frames': observed['fresh_stereo_eligible'],
        'duplicate_xfb_frames': observed['duplicate_xfb'],
        'fresh_mono_or_blank_xfb_frames': observed['fresh_mono_or_blank'],
        'fresh_content_frames': None, 'repeated_content_frames': None,
        'elapsed_wall_ticks': 60000000000, 'tick_frequency_hz': 1000000000,
        'fresh_xfb_intervals': observed['fresh_intervals'],
        'fresh_stereo_xfb_intervals': observed['fresh_stereo_intervals'],
        'actual_delivered_frames': None,
        'capture_policy': record.get('capture_policy', 'sampled'),
        'scheduled_captures_in_window': shots_in_window,
        'capture_overhead_measured': False,
        'fifo_unknown_opcode_mentions_entire_run': record.get('fifo_error_mentions'),
        'nominal_vi_hz_observed': result['nominal_vi_hz_observed'],
        'effective_vbi_target_hz_observed': result['effective_vbi_target_hz_observed'],
        'modeled_deadlines': result['cadences'],
    }
    analysis = immutable_json(run, 'analysis', result)
    evidence.append(str(analysis))
    receipt = {'schema_version': 1, 'game_key': record['game_key'], 'view': record['view'],
               'phase': record['phase'], 'evidence_level': 'E2', 'evidence_kind': 'offline',
               'evidence_paths': evidence,
               'evidence_sha256': {path: hashlib.sha256(Path(path).read_bytes()).hexdigest()
                                   for path in evidence},
               'metrics': {'frames': {rate: frame}},
               'notes': ['Stage requires visual review of this run.',
                         'XFB generation changes do not prove unique game-content frames.',
                         'Modeled deadlines are source availability, not separate 72/90 experiments or actual headset delivery.',
                         'Scheduled captures in this window: ' + str(shots_in_window) +
                         '. PNG readback/compression overhead is not separately measured.',
                         'FIFO Unknown Opcode mentions across the entire run: ' +
                         str(record.get('fifo_error_mentions', 'unmeasured')) +
                         '. A trace with errors is failure evidence, not a clean rendering pass.'],
               'run_exe_sha256': record['exe_sha256']}
    if args.stage:
        receipt['stage'] = args.stage
        receipt['notes'][0] = 'Stage assigned by caller after visual review.'
    path = immutable_json(run, 'frames.receipt', receipt)
    subprocess.run([sys.executable, '-B', str(Path(__file__).with_name('scorecard.py')),
                    'import', '--out', str(args.out.resolve()), '--receipt', str(path)], check=True)
    print(json.dumps({'game': record['game_key'], 'rate': rate, 'events': observed['events'],
                      'fresh_stereo': observed['fresh_stereo_eligible'],
                      'fresh_mono_or_blank': observed['fresh_mono_or_blank'],
                      'emulated_s': frame['emulated_duration_s'], 'wall_s': 60,
                      'analysis_path': str(analysis), 'receipt_path': str(path)}), flush=True)


if __name__ == '__main__':
    main()
