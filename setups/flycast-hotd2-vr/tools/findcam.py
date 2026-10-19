# Looks for a game's camera in a Flycast RAM dump (rip/ramNNNN.bin, 16 MB, offset 0 = 0x8C000000):
#  - screen matrices: 4x4 floats with x/y scales of 100..5000 px and the screen centre (320, 240) as offsets
#  - viewport-like pairs (320, +-240) and projection-like matrices (x/y scale 0.3..10, -1 in a w slot)
# python -I findcam.py <ram.bin> [more ram.bin ...]
import sys, struct
import numpy as np

def scan(path):
    raw = open(path, 'rb').read()
    n = len(raw) // 4
    f = np.frombuffer(raw[:n * 4], dtype='<f4')
    with np.errstate(all='ignore'):
        fin = np.isfinite(f)
    print('==', path)
    out = []
    # screen centre offsets 320 / 240 one or four floats apart (row or column major)
    for gap in (1, 4):
        idx = np.nonzero(fin[:-gap] & (np.abs(f[:-gap] - 320.0) < 0.01) & (np.abs(f[gap:] - 240.0) < 0.01))[0]
        for i in idx:
            # candidate matrix starts: offsets at m[8], m[9] (column major, translation in col 2) or m[12],m[13]
            for start in (i - 8, i - 12, i - 2, i - 3):
                if start < 0 or start + 16 > n:
                    continue
                m = f[start:start + 16]
                if not np.all(np.isfinite(m)):
                    continue
                sx, sy = abs(m[0]), abs(m[5])
                if 50 < sx < 5000 and 50 < sy < 5000 and 0.5 < sx / sy < 2.0:
                    out.append((start * 4, 'screen matrix', m))
    # viewport scale pairs 320, -240 (or 240) at m[0], m[5]
    idx = np.nonzero(fin[:-5] & (np.abs(np.abs(f[:-5]) - 320.0) < 0.01) & (np.abs(np.abs(f[5:]) - 240.0) < 0.01))[0]
    for i in idx:
        out.append((i * 4, 'viewport-like (m0=+-320, m5=+-240)', f[i:i + 16] if i + 16 <= n else f[i:i + 6]))
    seen = set()
    for off, what, m in out:
        if off in seen:
            continue
        seen.add(off)
        print('%06x %s: %s' % (off, what, ' '.join('%.4g' % v for v in m)))

for p in sys.argv[1:]:
    scan(p)
