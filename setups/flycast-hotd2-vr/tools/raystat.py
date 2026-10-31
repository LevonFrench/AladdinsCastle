# Numbers for an ACVR_RAY run of Confidential Mission.
# python raystat.py <proof folder> [-v]
# For each trigger pull: the pixel aimed at (the picture as drawn), the pixel the headset decision sent to the game
# (the "ACVR ray" log line), what the game says it read (ACVR watch 0x2AFCB8 / BC), the game's focal length at that
# time (from the watch of its projection and viewport), and the buttons sent (1 shot, 2 reload, 257 shot at the 2D plane).
import re, sys, os, math

d = sys.argv[1]
verbose = '-v' in sys.argv
run = open(os.path.join(d, 'run.txt'), encoding='utf-8', errors='replace').read().splitlines()
ray = next((l[len('ACVR_RAY '):].strip() for l in run if l.startswith('ACVR_RAY ')), '')
presses = []
prev = 0
for part in ray.split(';'):
    f = part.split(':')
    if len(f) == 8:
        t, trig = float(f[0]), int(f[7])
        if trig and not prev:
            presses.append((t, float(f[1]), float(f[2]), (float(f[4]), float(f[5]), float(f[6]))))
        prev = trig
log = open(os.path.join(d, 'flycast.log'), encoding='utf-8', errors='replace').read().splitlines()
def wall(l):
    m = re.match(r'(\d+):(\d+):(\d+)', l)
    return int(m.group(1)) * 60 + int(m.group(2)) + int(m.group(3)) / 1000.0 if m else None
rays = []      # (wall, target, screen, onscreen, buttons)
watch = []     # (wall, game t, rx, ry, projx, vpx)
for l in log:
    m = re.search(r'ACVR ray(?: t=([\d.]+))?: toward ([\-\d.]+),([\-\d.]+) from .* trigger 1 -> (on|off) screen, screen ([\-\d.]+),([\-\d.]+), hit (\w+) .*buttons (\d+)', l)
    if m:
        rays.append((wall(l), float(m.group(2)), float(m.group(3)), m.group(4) == 'on', float(m.group(5)), float(m.group(6)), int(m.group(8)),
                     float(m.group(1)) if m.group(1) else None, m.group(7)))
        continue
    m = re.search(r'ACVR watch t=([\d.]+) 2afcb8=(-?\d+) 2afcbc=(-?\d+) 2f0c20=([\d.\-]+) .* 2f0d58=([\d.\-]+)', l)
    if m:
        watch.append((wall(l), float(m.group(1)), int(m.group(2)), int(m.group(3)), float(m.group(4)), float(m.group(5))))
rows = []
for i, (w, tx, ty, on, sx, sy, b, rt, how) in enumerate(rays):
    gt = rt if rt is not None else (presses[i][0] if i < len(presses) else None)
    if rt is not None:
        after = [x for x in watch if rt + 0.03 <= x[1] <= rt + 0.35][-1:]
        cur = [x for x in watch if x[1] <= rt]
    else:
        after = [x for x in watch if x[0] is not None and x[0] >= w - 0.02 and x[0] <= w + 1.5]
        cur = [x for x in watch if x[0] is not None and x[0] <= w]
    focal = cur[-1][4] * cur[-1][5] if cur else float('nan')
    rd = after[0] if after else None
    rows.append(dict(how=how, t=gt, tx=tx, ty=ty, on=on, sx=sx, sy=sy, b=b, focal=focal, rx=rd[2] if rd else None, ry=rd[3] if rd else None))
def line(r):
    s = 't=%s aim %.0f,%.0f -> sent %s (%s, buttons %d) focal %.0f' % ('%.2f' % r['t'] if r['t'] else '?', r['tx'], r['ty'],
            '%.1f,%.1f' % (r['sx'], r['sy']) if r['on'] else 'off screen', 'on' if r['on'] else 'off', r['b'], r['focal'])
    if r['on']:
        s += '  sent-aim %+.1f,%+.1f' % (r['sx'] - r['tx'], r['sy'] - r['ty'])
        if r['rx'] is not None:
            s += '  game read %d,%d  read-aim %+.1f,%+.1f' % (r['rx'], r['ry'], r['rx'] - r['tx'], r['ry'] - r['ty'])
    return s
if verbose:
    for r in rows:
        print(line(r))
def summary(rs, label):
    flat = [r for r in rs if r['on'] and r['how'] != '3D']
    rs = [r for r in rs if r['how'] == '3D' or not r['on']]
    on = [r for r in rs if r['on']]
    off = [r for r in rs if not r['on']]
    if on:
        e = [math.hypot(r['sx'] - r['tx'], r['sy'] - r['ty']) for r in on]
        g = [math.hypot(r['rx'] - r['tx'], r['ry'] - r['ty']) for r in on if r['rx'] is not None]
        print('%s: %d pulls on screen; sent-vs-aim error px max %.1f rms %.2f%s; %d off screen (buttons %s)' % (
            label, len(on), max(e), math.sqrt(sum(v * v for v in e) / len(e)),
            ('; game-read-vs-aim max %.1f rms %.2f (%d read)' % (max(g), math.sqrt(sum(v * v for v in g) / len(g)), len(g))) if g else '',
            len(off), ','.join(str(r['b']) for r in off) or '-') + ('; %d more hit the HUD plane (aimed past it, not counted)' % len(flat) if flat else ''))
    else:
        print('%s: %d pulls, none on screen (off screen buttons %s)' % (label, len(rs), ','.join(str(r['b']) for r in rs)))
summary(rows, 'all')
focals = [r['focal'] for r in rows if r['focal'] == r['focal']]
if focals:
    print('game focal at the pulls: %.0f to %.0f px' % (min(focals), max(focals)))
for lo, hi in ((0, 76.8), (76.8, 79.0), (79.0, 999)):
    rs = [r for r in rows if r['t'] is not None and lo <= r['t'] < hi]
    if rs:
        summary(rs, 'game time %.1f-%.1f s (focal %.0f-%.0f)' % (lo, min(hi, 999), min(r['focal'] for r in rs), max(r['focal'] for r in rs)))
