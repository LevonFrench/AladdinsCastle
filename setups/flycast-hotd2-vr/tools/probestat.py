# Headset-decision probes of a long Confidential Mission run (ACVR_RAY mode 2 lines "ACVR ray probe t=...").
# For each probe: the pixel of the drawn picture the pointer was aimed through, the pixel the decision would send to the game,
# the game's focal length now and the stock focal length the decision used.
# python probestat.py <proof folder> [-v]
import re, sys, os, math

d = sys.argv[1]
rows = []
for l in open(os.path.join(d, 'flycast.log'), encoding='utf-8', errors='replace'):
    m = re.search(r'ACVR ray probe t=([\d.]+): toward ([\-\d.]+),([\-\d.]+) -> (on|off) screen, screen ([\-\d.]+),([\-\d.]+),(?: hit (\w+),)? focal ([\d.]+) vs stock ([\d.]+)', l)
    if m:
        t, px, py, on, sx, sy, how, f, st = float(m.group(1)), float(m.group(2)), float(m.group(3)), m.group(4) == 'on', float(m.group(5)), float(m.group(6)), m.group(7), float(m.group(8)), float(m.group(9))
        rows.append((t, px, py, on, sx, sy, f, st, how))
if '-v' in sys.argv:
    for r in rows:
        print('t=%.1f aim %.0f,%.0f -> %s %.1f,%.1f focal %.0f stock %.0f' % ((r[0], r[1], r[2], 'on' if r[3] else 'off', r[4], r[5], r[6], r[7])))
def stats(rs, label):
    flat = [r for r in rs if r[3] and r[8] not in ('3D', None)]
    rs = [r for r in rs if r[8] in ('3D', None) or not r[3]]
    on = [r for r in rs if r[3]]
    miss = len(rs) - len(on)
    if not on:
        print('%s: %d probes, none on screen' % (label, len(rs)))
        return
    e = [math.hypot(r[4] - r[1], r[5] - r[2]) for r in on]
    print('%s: %d probes (%d left the screen); error px max %.1f  rms %.2f  focal %.0f-%.0f  stock %.0f-%.0f' % (
        label, len(rs), miss, max(e), math.sqrt(sum(v * v for v in e) / len(e)), min(r[6] for r in on), max(r[6] for r in on),
        min(r[7] for r in on), max(r[7] for r in on)) + ('; %d more landed on the 2D plane (menus, HUD, flat scenes: not counted)' % len(flat) if flat else ''))
if rows:
    stats(rows, 'all')
    t0 = rows[0][0]
    band = 60.0
    t = 0.0
    while t <= rows[-1][0]:
        rs = [r for r in rows if t <= r[0] < t + band]
        if rs:
            stats(rs, 'game time %3.0f-%3.0f s' % (t, t + band))
        t += band
    for lo, hi in ((0, 600), (600, 800), (800, 1200), (1200, 99999)):
        rs = [r for r in rows if lo <= r[6] < hi]
        if rs:
            stats(rs, 'game focal %d-%d px' % (lo, hi))
