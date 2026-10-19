# Builds ACVR_GUN / ACVR_SNAP strings for a nine-point aim test.
# python -I aimplan.py <start s> <spacing s> <fire 0/1> [reload_after N] [buttons] [margin]
# prints two lines: the gun script and the snap times. Shots hold the trigger 0.1 s.
import sys
start = float(sys.argv[1]); step = float(sys.argv[2]); fire = sys.argv[3] == '1'
reload_after = int(sys.argv[4]) if len(sys.argv) > 4 else 0
buttons = int(sys.argv[5]) if len(sys.argv) > 5 else 257
m = float(sys.argv[6]) if len(sys.argv) > 6 else 0.15
lo, hi = int(m * 10000), int((1 - m) * 10000)
pts = [(5000, 5000), (lo, lo), (hi, lo), (lo, hi), (hi, hi), (5000, lo), (5000, hi), (lo, 5000), (hi, 5000)]
gun, snaps = [], []
t = start
gun.append('0:5000:5000:0')
for i, (x, y) in enumerate(pts):
    if reload_after and i == reload_after:
        gun.append('%.2f:-10000:-10000:%d' % (t - step / 2, 2 if fire else 0))
        gun.append('%.2f:-10000:-10000:0' % (t - step / 2 + 0.15))
    gun.append('%.2f:%d:%d:0' % (t - 0.3, x, y))
    gun.append('%.2f:%d:%d:%d' % (t, x, y, buttons if fire else 0))
    gun.append('%.2f:%d:%d:0' % (t + 0.1, x, y))
    snaps += ['%.2f' % (t + 0.07), '%.2f' % (t + 0.2)]
    t += step
print(';'.join(gun))
print(';'.join(snaps))
