"""Analyze an offline metadata trace; never claim XR/compositor delivery.

Use --start-emulated to anchor gameplay, then --duration 60 wall-clock seconds.
The default native cadence is observed nominal VI, never an invented game FPS.
A row at/past the window end is required.
"""
import argparse
import bisect
import csv
import json
import math
import statistics
from pathlib import Path


def percentile(values, percent):
    if not values:
        return None
    values = sorted(values)
    pos = (len(values) - 1) * percent / 100
    low = int(pos)
    return values[low] + (values[min(low + 1, len(values) - 1)] - values[low]) * (pos - low)


def intervals(rows):
    values = [(b['wall_ns'] - a['wall_ns']) / 1e6 for a, b in zip(rows, rows[1:])]
    return {'count': len(values), 'mean_ms': statistics.mean(values) if values else None,
            'stdev_ms': statistics.pstdev(values) if values else None,
            'p50_ms': percentile(values, 50), 'p95_ms': percentile(values, 95),
            'p99_ms': percentile(values, 99), 'max_ms': max(values, default=None)}


def read_rows(path):
    with Path(path).open(newline='', encoding='utf-8') as handle:
        reader = csv.DictReader(handle)
        required = {'wall_ns', 'emulated_ticks', 'ticks_per_second', 'duplicate', 'reason',
                    'texture_id', 'layers', 'width', 'height', 'vi_width', 'vi_height',
                    'native_vi_hz', 'forced_vbi_frequency', 'effective_vbi_target_hz',
                    'vi_overclock_enabled', 'vi_overclock'}
        if set(reader.fieldnames or ()) != required:
            raise ValueError('unexpected trace header')
        rows = []
        for raw in reader:
            if None in raw or any(value is None for value in raw.values()):
                raise ValueError('truncated or malformed trace row')
            row = {key: value if key == 'reason' else float(value) if key in
                   ('native_vi_hz', 'effective_vbi_target_hz', 'vi_overclock')
                   else int(value) for key, value in raw.items()}
            if (row['reason'] not in ('vi', 'immediate') or row['duplicate'] not in (0, 1)
                    or row['vi_overclock_enabled'] not in (0, 1)):
                raise ValueError('invalid trace event')
            if any(value < 0 for key, value in row.items() if key not in ('reason', 'forced_vbi_frequency')):
                raise ValueError('negative metadata')
            if not math.isfinite(row['native_vi_hz']) or row['native_vi_hz'] <= 0:
                raise ValueError('invalid nominal VI cadence')
            if any(not math.isfinite(row[k]) for k in ('effective_vbi_target_hz', 'vi_overclock')):
                raise ValueError('invalid VI metadata')
            if row['ticks_per_second'] == 0:
                raise ValueError('zero emulated clock')
            if rows and row['wall_ns'] < rows[-1]['wall_ns']:
                raise ValueError('non-monotonic host clock')
            rows.append(row)
    if not rows:
        raise ValueError('empty trace')
    return rows


def analyze(rows, native_rate=None, start=None, duration=60, start_emulated=None):
    if native_rate is not None and (not math.isfinite(native_rate) or native_rate <= 0):
        raise ValueError('native rate must be positive and finite')
    if not math.isfinite(duration) or duration <= 0:
        raise ValueError('duration must be positive and finite')
    if start is not None and start_emulated is not None:
        raise ValueError('choose wall-clock or emulated anchor, not both')
    if start_emulated is not None:
        if not math.isfinite(start_emulated) or start_emulated < 0:
            raise ValueError('invalid emulated anchor')
        if len({row['ticks_per_second'] for row in rows}) != 1:
            raise ValueError('emulated clock changed; use explicit wall-clock anchor')
        anchor = next((row for row in rows if row['emulated_ticks'] / row['ticks_per_second'] >= start_emulated), None)
        if anchor is None:
            raise ValueError('emulated gameplay anchor not reached')
        start_ns = anchor['wall_ns']
    else:
        start_ns = rows[0]['wall_ns'] if start is None else round(start * 1e9)
    end_ns = start_ns + round(duration * 1e9)
    if rows[0]['wall_ns'] > start_ns or rows[-1]['wall_ns'] < end_ns:
        raise ValueError('trace does not cover the complete consecutive wall-clock window')
    window = [row for row in rows if start_ns <= row['wall_ns'] < end_ns]
    nominal_rates = sorted({row['native_vi_hz'] for row in window})
    if native_rate is None and len(nominal_rates) != 1:
        raise ValueError('nominal VI cadence changed or no window events; declare cadence explicitly')
    rate_source = 'declared --native-rate' if native_rate is not None else 'observed nominal VI (not game FPS)'
    native_rate = native_rate if native_rate is not None else nominal_rates[0]
    fresh = [row for row in window if not row['duplicate']]
    stereo = [row for row in fresh if row['layers'] >= 2 and row['width'] and row['height']]
    clocks = {row['ticks_per_second'] for row in rows}
    times = [row['wall_ns'] for row in rows]
    first = rows[bisect.bisect_right(times, start_ns) - 1]
    last = rows[bisect.bisect_left(times, end_ns)]
    emulated_seconds = ((last['emulated_ticks'] - first['emulated_ticks']) /
                        first['ticks_per_second']) if len(clocks) == 1 else None
    result = {
        'evidence': 'E2 offline XFB source metadata; cadence availability is a model',
        'actual_compositor_delivery': 'unmeasured', 'gpu_completion': 'unmeasured',
        'native_cadence_source': rate_source, 'actual_game_fps': 'unmeasured',
        'nominal_vi_hz_observed': nominal_rates,
        'forced_vbi_frequency_observed': sorted({row['forced_vbi_frequency'] for row in window}),
        'effective_vbi_target_hz_observed': sorted({row['effective_vbi_target_hz'] for row in window}),
        'vi_overclock_enabled_observed': sorted({row['vi_overclock_enabled'] for row in window}),
        'vi_overclock_observed': sorted({row['vi_overclock'] for row in window}),
        'window': {'start_wall_seconds': start_ns / 1e9, 'duration_seconds': duration,
                   'requested_start_emulated_seconds': start_emulated,
                   'complete': True, 'interval': '[start,end)'},
        'observed': {'events': len(window), 'fresh_xfb': len(fresh),
                     'duplicate_xfb': len(window) - len(fresh), 'fresh_stereo_eligible': len(stereo),
                     'fresh_mono_or_blank': len(fresh) - len(stereo),
                     'fresh_stereo_share': len(stereo) / len(fresh) if fresh else None,
                     'fresh_stereo_hz': len(stereo) / duration,
                     'emulated_seconds_between_bracketing_events': emulated_seconds,
                     'emulated_clock_changed': len(clocks) != 1,
                     'all_event_intervals': intervals(window),
                     'fresh_intervals': intervals(fresh), 'fresh_stereo_intervals': intervals(stereo)},
        'cadences': {}}
    # Replay deadlines against metadata only: retain the last fresh eligible XFB
    # across a mono event, as a hypothetical held-eye image. No rendering occurs.
    eligible = [(i, row) for i, row in enumerate(rows)
                if not row['duplicate'] and row['layers'] >= 2 and row['width'] and row['height']]
    eligible_times = [row['wall_ns'] for _, row in eligible]
    for label, rate in [('native', native_rate), ('72', 72), ('90', 90)]:
        expected = max(0, math.ceil(duration * rate - 1e-9))
        empty = reused = new = latest_mono = 0
        previous = None
        ages = []
        for n in range(expected):
            deadline = start_ns + n * 1e9 / rate
            latest_index = bisect.bisect_right(times, deadline) - 1
            if latest_index < 0 or rows[latest_index]['layers'] < 2:
                latest_mono += 1
            eligible_index = bisect.bisect_right(eligible_times, deadline) - 1
            if eligible_index < 0:
                empty += 1
                continue
            generation, row = eligible[eligible_index]
            ages.append((deadline - row['wall_ns']) / 1e6)
            if generation == previous:
                reused += 1
            else:
                new += 1
            previous = generation
        result['cadences'][label] = {
            'rate_hz': rate, 'expected_deadlines': expected,
            'fresh_stereo_count_vs_expected': len(stereo) / expected if expected else None,
            'modeled_new_source_deadlines': new, 'modeled_held_source_deadlines': reused,
            'modeled_no_source_deadlines': empty,
            'deadlines_latest_xfb_mono_or_blank': latest_mono,
            'modeled_source_age_p95_ms': percentile(ages, 95),
            'modeled_source_age_max_ms': max(ages, default=None),
            'actual_delivered_frames': None}
    return result


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('csv', type=Path)
    parser.add_argument('--native-rate', type=float, help='explicit documented cadence override; default nominal VI')
    parser.add_argument('--start', type=float)
    parser.add_argument('--start-emulated', type=float)
    parser.add_argument('--duration', type=float, default=60)
    parser.add_argument('--out', type=Path)
    args = parser.parse_args()
    try:
        output = json.dumps(analyze(read_rows(args.csv), args.native_rate, args.start, args.duration,
                                   args.start_emulated), indent=2, allow_nan=False)
    except (ValueError, OSError) as error:
        parser.error(str(error))
    if args.out:
        args.out.write_text(output + '\n', encoding='utf-8')
    else:
        print(output)


if __name__ == '__main__':
    main()
