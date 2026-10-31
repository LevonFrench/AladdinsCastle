# Hit reports of a gun-grid run: the game shows a green body picture top left when a shot hits an enemy.
# python hits.py <proof folder> ... : per picture 0.45 s after each shot (snap times x.x5 or x.x0), green pixel count in the panel box.
import sys, os, re
from PIL import Image
import numpy as np
for d in sys.argv[1:]:
    out = []
    for f in sorted(os.listdir(d)):
        m = re.match(r'snap_(\d+)\.png', f)
        if not m:
            continue
        t = int(m.group(1)) / 100.0
        if not (75.5 <= t <= 88):
            continue
        im = np.asarray(Image.open(os.path.join(d, f)).convert('RGB')).astype(int)
        h, w, _ = im.shape
        box = im[int(40 * h / 480):int(130 * h / 480), int(10 * w / 640):int(110 * w / 640)]
        g = int(((box[..., 1] > 150) & (box[..., 0] < 90) & (box[..., 2] < 90)).sum())
        out.append((t, g))
    print(d, ' '.join('%.2f:%s' % (t, 'HIT' if g > 150 else '-') for t, g in out))
