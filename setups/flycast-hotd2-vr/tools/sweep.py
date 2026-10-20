# Gun sweep script for ACVR_GUN: shots over a grid of screen points (640x480 pixels), a reload every N shots.
# python sweep.py <start s> <spacing s> <reload_every> <xs comma> <ys comma> [buttons]
import sys
start, step, every = float(sys.argv[1]), float(sys.argv[2]), int(sys.argv[3])
xs = [float(v) for v in sys.argv[4].split(',')]
ys = [float(v) for v in sys.argv[5].split(',')]
buttons = int(sys.argv[6]) if len(sys.argv) > 6 else 257
g = ['0:5000:5000:0']
t = start
n = 0
for y in ys:
    for x in xs:
        if every and n and n % every == 0:
            g += ['%.2f:-10000:-10000:2' % t, '%.2f:-10000:-10000:0' % (t + 0.1)]
            t += step
        X, Y = int(round(x / 640 * 10000)), int(round(y / 480 * 10000))
        g += ['%.2f:%d:%d:0' % (t, X, Y), '%.2f:%d:%d:%d' % (t + step * 0.4, X, Y, buttons), '%.2f:%d:%d:0' % (t + step * 0.6, X, Y)]
        t += step
        n += 1
print(';'.join(g))
print('end %.2f' % t, file=sys.stderr)
