# Finds where a game keeps the light gun position it read: RAM dumps taken with the gun at known points.
# python gunram.py x1,y1 ram1.bin x2,y2 ram2.bin x3,y3 ram3.bin
# Reports u16/s16/float/u32 values that are an exact (or near) linear function of x or y across all dumps.
import sys
import numpy as np

pts, rams = [], []
args = sys.argv[1:]
for i in range(0, len(args), 2):
    x, y = map(float, args[i].split(','))
    pts.append((x, y))
    rams.append(open(args[i + 1], 'rb').read())
X = np.array([p[0] for p in pts]); Y = np.array([p[1] for p in pts])

def fit(vals, target):
    # vals: (k, n) values; fit vals = a*target + b using the first two, check the rest
    a = (vals[1] - vals[0]) / (target[1] - target[0])
    b = vals[0] - a * target[0]
    err = np.zeros(vals.shape[1])
    for k in range(2, len(target)):
        err = np.maximum(err, np.abs(a * target[k] + b - vals[k]))
    return a, b, err

for kind, dt, step in (('u16', '<u2', 2), ('s16', '<i2', 2), ('f32', '<f4', 4), ('u32', '<u4', 4)):
    vals = np.stack([np.frombuffer(r[:len(r) // step * step], dtype=dt).astype(np.float64) for r in rams])
    with np.errstate(all='ignore'):
        for axis, T in (('x', X), ('y', Y)):
            a, b, err = fit(vals, T)
            ok = np.isfinite(a) & np.isfinite(err) & (np.abs(a) > 0.05) & (np.abs(a) < 20) & (err <= np.maximum(1.0, np.abs(a) * 1.5))
            # the other axis must not explain it equally (values that move with both are noise)
            idx = np.nonzero(ok)[0]
            for i in idx[:40]:
                print('%s %s at %06x: value = %.4f * %s + %.2f (max misfit %.2f) values %s' % (
                    kind, axis, i * step, a[i], axis, b[i], err[i], ' '.join('%.2f' % v for v in vals[:, i])))
