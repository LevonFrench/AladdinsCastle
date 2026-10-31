# Finds words in six RAM dumps (gun held at known points) that are a linear function of the gun's x (or y) alone.
# python findgun.py <dump folder>   points: (80,80) (560,80) (80,400) (560,400) (320,240) (200,300)
import sys, os
import numpy as np
d = sys.argv[1]
X = np.array([80, 560, 80, 560, 320, 200], float); Y = np.array([80, 80, 400, 400, 240, 300], float)
rams = [np.fromfile(os.path.join(d, 'ram%04d.bin' % i), dtype=np.uint8) for i in range(6)]
n = len(rams[0]) // 4 * 4
res = []
for kind, dt, step in (('f32', '<f4', 4), ('s16', '<i2', 2), ('u32', '<u4', 4)):
    v = np.stack([np.frombuffer(r[:n], dtype=dt).astype(np.float64) for r in rams])
    with np.errstate(all='ignore'):
        for axis, T, O in (('x', X, Y), ('y', Y, X)):
            A = np.vstack([T, np.ones_like(T)]).T
            coef, *_ = np.linalg.lstsq(A, v, rcond=None)
            pred = A @ coef
            err = np.abs(pred - v).max(axis=0)
            span = np.abs(v).max(axis=0) - np.abs(v).min(axis=0)
            ok = np.isfinite(err) & np.isfinite(coef[0]) & (np.abs(coef[0]) > 1e-4) & (err < 0.01 * np.abs(coef[0]) * (T.max() - T.min()) + 1e-9) & (np.abs(coef[0]) * (T.max() - T.min()) > 0)
            # not explained by the other axis
            c2, *_ = np.linalg.lstsq(np.vstack([O, np.ones_like(O)]).T, v, rcond=None)
            ok &= np.abs(c2[0]) * (O.max() - O.min()) < 0.2 * np.abs(coef[0]) * (T.max() - T.min())
            for i in np.nonzero(ok)[0][:12]:
                res.append('%s %s at %06x: %.5g * %s + %.4g (max misfit %.3g) values %s' % (kind, axis, i * (4 if step == 4 else 2), coef[0][i], axis, coef[1][i], err[i], ' '.join('%.4g' % q for q in v[:, i])))
print('\n'.join(res[:40]))
