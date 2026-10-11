#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""Bounded scripted controller states through our loopback DSU server, never OS input."""
import argparse
import bisect
import dataclasses
import hashlib
import json
import math
import os
from pathlib import Path
import time
from dsu import Pad, Server

# DSU protocol button_states1/2. PS/touch are separate bytes and not scriptable here.
BUTTONS = dict(zip(('Share','L3','R3','Options','Pad N','Pad E','Pad S','Pad W',
                   'L2','R2','L1','R1','Triangle','Circle','Cross','Square'), range(16)))


def number(value, low, high):
    if type(value) not in (float, int) or not math.isfinite(value) or not low <= value <= high:
        raise ValueError('Non-finite or out-of-range script number')
    return float(value)


def vector(value, length, limit):
    if not isinstance(value, list) or len(value) != length:
        raise ValueError('Wrong vector shape')
    return tuple(number(x, -limit, limit) for x in value)


class Timeline:
    def __init__(self, data):
        if not isinstance(data, dict) or set(data) != {'version','seconds','steps'} or data['version'] != 1:
            raise ValueError('Expected script version 1 with seconds and steps')
        self.seconds = number(data['seconds'], .1, 1800)
        if not isinstance(data['steps'], list) or not 1 <= len(data['steps']) <= 10000:
            raise ValueError('Expected 1..10000 steps')
        self.times, self.states = [], []
        for step in data['steps']:
            if not isinstance(step, dict) or set(step) != {'at','pads'}:
                raise ValueError('A step contains at and pads only')
            at = number(step['at'], 0, self.seconds)
            if self.times and at <= self.times[-1]:
                raise ValueError('Step times must strictly increase')
            pads = step['pads']
            if not isinstance(pads, list) or len(pads) != 2:
                raise ValueError('Each step must specify both DSU slots')
            state = []
            for values in pads:
                if not isinstance(values, dict) or set(values) - {'buttons','pointer','stick','accel','gyro'}:
                    raise ValueError('Unknown pad field')
                buttons = values.get('buttons', [])
                if not isinstance(buttons, list) or any(type(b) is not str or b not in BUTTONS for b in buttons):
                    raise ValueError('Unknown DSU button')
                if len(buttons) != len(set(buttons)):
                    raise ValueError('Duplicate button')
                state.append(Pad(buttons=sum(1 << BUTTONS[b] for b in buttons),
                    pointer=vector(values.get('pointer',[0,0]),2,1),
                    stick=vector(values.get('stick',[0,0]),2,1),
                    accel=vector(values.get('accel',[0,-1,0]),3,16),
                    gyro=vector(values.get('gyro',[0,0,0]),3,2000)))
            self.times.append(at)
            self.states.append(state)

    def sample(self, elapsed, timestamp):
        index = bisect.bisect_right(self.times, elapsed) - 1
        states = self.states[index] if 0 <= elapsed < self.seconds and index >= 0 else [Pad(), Pad()]
        return [dataclasses.replace(p, timestamp=timestamp) for p in states]


def fresh_receipt(path):
    path = Path(os.path.abspath(path))
    if '.local' not in path.parts:
        raise ValueError('Receipt must be inside project .local')
    for part in (path, *path.parents):
        if part.is_symlink() or (hasattr(part, 'is_junction') and part.is_junction()):
            raise ValueError('Linked receipt path refused')
    path.parent.mkdir(parents=True, exist_ok=True)
    return path.open('x', encoding='utf-8')


def run(timeline, port, wait_seconds, receipt, script_sha256=None):
    server = None
    summary = dict(version=1, outcome='error', timeline_started=False, sent=0, peak_subscriptions=0,
                   script_sha256=script_sha256, seconds=timeline.seconds, port=port,
                   acceptance='DSU transport only; game progress requires captured evidence')
    try:
        server = Server(port)
        print(json.dumps(dict(ready=True, port=server.socket.getsockname()[1])), flush=True)
        deadline = time.monotonic() + wait_seconds
        started = None
        while True:
            now = time.monotonic()
            stamp = time.monotonic_ns() // 1000
            pads = timeline.sample(now-started, stamp) if started is not None else [Pad(),Pad()]
            server.tick(pads, now)
            summary['peak_subscriptions'] = max(summary['peak_subscriptions'], len(server.subscriptions))
            if started is None and any(key[2] == 0 for key in server.subscriptions):
                started = now
                summary['timeline_started'] = True
                print(json.dumps(dict(timeline_started=True)), flush=True)
            if started is None and now >= deadline:
                summary['outcome'] = 'no-client'
                break
            if started is not None and now-started >= timeline.seconds:
                summary['outcome'] = 'completed'
                break
            time.sleep(1/120)
    except KeyboardInterrupt:
        summary['outcome'] = 'interrupted'
    finally:
        if server is not None:
            # Deliver release states before closing even after interruption or a script error.
            summary['neutral_release'] = False
            try:
                for _ in range(30):
                    server.tick([Pad(timestamp=time.monotonic_ns()//1000),Pad(timestamp=time.monotonic_ns()//1000)])
                    time.sleep(1/120)
                summary['neutral_release'] = True
            except Exception as error:
                summary['release_error'] = type(error).__name__
                summary['outcome'] = 'error'
            finally:
                summary['sent'] = server.sent
                try:
                    server.close()
                except Exception as error:
                    summary['close_error'] = type(error).__name__
                    summary['outcome'] = 'error'
        json.dump(summary, receipt, indent=2)
        receipt.write('\n')
        receipt.flush()
    return 0 if summary['outcome'] == 'completed' else 2


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--script', required=True, type=Path)
    parser.add_argument('--receipt', type=Path)
    parser.add_argument('--port', type=int, default=26760)
    parser.add_argument('--wait-for-client', type=float, default=30)
    parser.add_argument('--check', action='store_true')
    args = parser.parse_args()
    if not 1024 <= args.port <= 65535:
        parser.error('Port must be 1024..65535')
    number(args.wait_for_client, .1, 120)
    if args.script.stat().st_size > 1024*1024:
        parser.error('Script exceeds 1 MiB')
    raw = args.script.read_bytes()
    timeline = Timeline(json.loads(raw))
    if args.check:
        print(json.dumps(dict(valid=True, seconds=timeline.seconds, sha256=hashlib.sha256(raw).hexdigest())))
        return 0
    if args.receipt is None:
        parser.error('--receipt is required for a live DSU run')
    with fresh_receipt(args.receipt) as receipt:
        return run(timeline, args.port, args.wait_for_client, receipt, hashlib.sha256(raw).hexdigest())


if __name__ == '__main__':
    raise SystemExit(main())
