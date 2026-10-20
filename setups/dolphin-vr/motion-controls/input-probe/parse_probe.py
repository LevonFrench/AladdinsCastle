#!/usr/bin/env python
"""Turn an input-probe log into a table: for every scripted input, what the emulated Wii Remote
reported to the game just before it and while it was held.

Usage: python -B parse_probe.py <script.txt> <probe.log>
"""
import re
import sys

BUTTONS = [(0x0800, 'A'), (0x0400, 'B'), (0x0200, '1'), (0x0100, '2'), (0x1000, '-'),
           (0x0010, '+'), (0x8000, 'Home'), (0x0008, 'Up'), (0x0004, 'Down'), (0x0001, 'Left'),
           (0x0002, 'Right')]


def load_probe(path):
    rows = []
    for line in open(path, encoding='utf-8'):
        m = re.match(r't=([\d.]+) (.*)', line.strip())
        if m and not m.group(2).startswith('shot'):
            body = m.group(2)
            remote, _, nunchuk = body.partition(' nunchuk ')
            fields = dict(re.findall(r'(\w+)=([^ ]+)', remote))
            fields.update({'n_' + k: v for k, v in re.findall(r'(\w+)=([^ ]+)', nunchuk)})
            rows.append((float(m.group(1)), fields))
    return rows


def state_at(rows, t):
    current = None
    for rt, f in rows:
        if rt > t:
            break
        current = f
    return current or {}


def describe(f):
    out = []
    b = int(f.get('buttons', '0'), 16)
    out.append('buttons[' + ','.join(n for bit, n in BUTTONS if b & bit) + ']')
    if 'n_stick' in f:
        out.append(f"nunchuk stick={f['n_stick']} C={f.get('n_c')} Z={f.get('n_z')}")
    return ' '.join(out)


def main():
    script, probe = sys.argv[1], sys.argv[2]
    rows = load_probe(probe)
    steps = {}
    for line in open(script, encoding='utf-8'):
        parts = line.split()
        if not parts or parts[0].startswith('#') or parts[0] == 'shot':
            continue
        key = (float(parts[0]), float(parts[1]))
        steps.setdefault(key, []).append(' '.join(parts[2:]))
    print(f'{len(rows)} probe lines')
    last = rows[-1][0] if rows else 0.0
    for (t0, t1), inputs in sorted(steps.items()):
        if t0 > last:
            print(f'{t0:6.2f}-{t1:6.2f} {" + ".join(inputs):34s} -> no probe line at or after this time (not reached or no change)')
            continue
        before = state_at(rows, t0 - 0.2)
        during = state_at(rows, t1 - 0.05)
        changed = {k: (before.get(k), v) for k, v in during.items() if before.get(k) != v}
        print(f'{t0:6.2f}-{t1:6.2f} {" + ".join(inputs):34s} -> {describe(during)}')
        for k, (a, b) in changed.items():
            if k not in ('buttons', 'n_c', 'n_z', 'n_stick'):
                print(f'{"":50s}{k}: {a} -> {b}')


if __name__ == '__main__':
    main()
