# Nine-point gun script that pumps (an off-screen shot) before every shot: python aimpump.py <start> <step> <fire 0/1>
import sys
start, step, fire = float(sys.argv[1]), float(sys.argv[2]), sys.argv[3] == '1'
lo, hi = 1500, 8500
pts = [(5000, 5000), (lo, lo), (hi, lo), (lo, hi), (hi, hi), (5000, lo), (5000, hi), (lo, 5000), (hi, 5000)]
g, snaps = ['0:5000:5000:0'], []
t = start
for x, y in pts:
    g.append('%.2f:-10000:-10000:%d' % (t - 0.6, 2 if fire else 0))
    g.append('%.2f:-10000:-10000:0' % (t - 0.45))
    g.append('%.2f:%d:%d:0' % (t - 0.3, x, y))
    g.append('%.2f:%d:%d:%d' % (t, x, y, 1 if fire else 0))
    g.append('%.2f:%d:%d:0' % (t + 0.1, x, y))
    snaps += ['%.2f' % (t + 0.07), '%.2f' % (t + 0.2)]
    t += step
print(';'.join(g)); print(';'.join(snaps))
