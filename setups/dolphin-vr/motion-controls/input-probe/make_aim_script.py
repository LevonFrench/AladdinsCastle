#!/usr/bin/env python
"""Write an aim-check script: A presses at given times, then nine aim points held 2 s each.

usage: python make_aim_script.py <out.txt> <first aim time s> <extent> [A press times ...]
Points: centre, corners and edge midpoints at +/-extent (u right, v up). Pictures 1.5 s into each.
"""
import sys

out, start, ext = sys.argv[1], float(sys.argv[2]), float(sys.argv[3])
presses = [float(t) for t in sys.argv[4:]]
e = ext
pts = [(0, 0), (-e, e), (0, e), (e, e), (-e, 0), (e, 0), (-e, -e), (0, -e), (e, -e)]
lines = [f'# aim check: A at {presses}, nine points at +/-{ext} from {start} s']
lines += [f'{t:.1f} {t + 0.3:.1f} R primary 1' for t in presses]
t = start
for i, (u, v) in enumerate(pts):
    lines += [f'{t:.1f} {t + 2:.1f} R u {u}', f'{t:.1f} {t + 2:.1f} R v {v}', f'shot {t + 1.5:.1f} aim-{i}_{u}_{v}']
    t += 2
lines.append(f'{t:.1f} {t + 1:.1f} R u 5')
open(out, 'w', newline='\n').write('\n'.join(lines) + '\n')
print('until', t)
