import sys
import numpy as np
from PIL import Image
a = np.asarray(Image.open(sys.argv[1]).convert('L')).astype(int)
b = np.asarray(Image.open(sys.argv[2]).convert('L')).astype(int)
x0 = int(sys.argv[3]) if len(sys.argv) > 3 else 60
d = (b - a)  # darker in fire
m = d > 25
m[:, :x0] = False
# connected blobs
from collections import deque
seen = np.zeros_like(m)
blobs = []
H, W = m.shape
for y in range(H):
    for x in range(W):
        if m[y, x] and not seen[y, x]:
            q = deque([(y, x)]); seen[y, x] = True; pts = []
            while q:
                cy, cx = q.popleft(); pts.append((cy, cx))
                for dy in (-1, 0, 1):
                    for dx in (-1, 0, 1):
                        ny, nx = cy + dy, cx + dx
                        if 0 <= ny < H and 0 <= nx < W and m[ny, nx] and not seen[ny, nx]:
                            seen[ny, nx] = True; q.append((ny, nx))
            if len(pts) >= 3:
                ys = [p[0] for p in pts]; xs = [p[1] for p in pts]
                blobs.append((np.mean(xs), np.mean(ys), len(pts)))
for bx, by, n in sorted(blobs, key=lambda t: (t[1], t[0])):
    print('%.1f %.1f %d' % (bx, by, n))
