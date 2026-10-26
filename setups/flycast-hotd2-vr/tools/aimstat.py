# Aim error of a Confidential Mission run: the gun's target pixels (from the gun script in run.txt) against what the
# game says it read (ACVR watch lines for 0x2AFCB8 / 0x2AFCBC, logged when they change).
# python aimstat.py <proof folder> [t0 t1 ...]   (windows t0..t1 of game seconds; default: the whole run, and 60 s bands)
import re, sys, os, math

d = sys.argv[1]
run = open(os.path.join(d, 'run.txt'), encoding='utf-8', errors='replace').read().splitlines()
gun = next((l[4:].strip() for l in run if l.startswith('gun ')), '') or next((l[len('ACVR_GUN '):].strip() for l in run if l.startswith('ACVR_GUN ')), '')
ev = []
for part in gun.split(';'):
    f = part.split(':')
    if len(f) == 4:
        ev.append((float(f[0]), int(f[1]), int(f[2]), int(f[3])))
# trigger presses on screen (x, y >= 0) and off-screen presses
shots = []
prev = 0
for t, x, y, b in ev:
    if (b & 1) and not (prev & 1):
        shots.append((t, x * 640 / 10000.0, y * 480 / 10000.0, x >= 0 and y >= 0))
    prev = b
rd = []
for l in open(os.path.join(d, 'flycast.log'), encoding='utf-8', errors='replace'):
    m = re.search(r'ACVR watch t=([\d.]+) 2afcb8=(-?\d+) 2afcbc=(-?\d+)', l)
    if m:
        rd.append((float(m.group(1)), int(m.group(2)), int(m.group(3))))
rows = []
for t, ex, ey, on in shots:
    if not on:
        continue
    # the game latches the beam about 0.07 s after the pull; the value then holds until the next shot (0.5 s later)
    cand = [r for r in rd if t - 0.02 <= r[0] <= t + 0.35]
    if not cand or cand[-1][0] < t + 0.03:
        rows.append((t, ex, ey, None, None))
    else:
        r = cand[-1]
        rows.append((t, ex, ey, r[1], r[2]))
def stats(rs, label):
    ok = [r for r in rs if r[3] is not None]
    if not ok:
        print('%s: no readings (%d shots sent)' % (label, len(rs)))
        return
    dx = [r[3] - r[1] for r in ok]; dy = [r[4] - r[2] for r in ok]
    e = [math.hypot(a, b) for a, b in zip(dx, dy)]
    print('%s: %d shots sent, %d read; error px: max %.1f  rms %.2f  mean dx %+.2f dy %+.2f  (max |dx| %.1f, max |dy| %.1f)' % (
        label, len(rs), len(ok), max(e), math.sqrt(sum(v * v for v in e) / len(e)), sum(dx) / len(dx), sum(dy) / len(dy),
        max(abs(v) for v in dx), max(abs(v) for v in dy)))
if '-v' in sys.argv:
    for r in rows:
        print('  t=%.2f want %.1f,%.1f got %s,%s' % r)
stats(rows, 'all')
if rows:
    t_end = max(r[0] for r in rows)
    band = 60.0
    t = min(r[0] for r in rows)
    while t <= t_end:
        stats([r for r in rows if t <= r[0] < t + band], 'game time %.0f-%.0f s' % (t, t + band))
        t += band
