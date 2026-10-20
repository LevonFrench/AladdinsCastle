# Open-frame proof: marks the original 4:3 frame inside a wider picture and measures how much of the area
# outside it holds picture (not black). python openframe.py <scale> <out.png> <in.png> [in.png ...]
# scale: the original frame's size as a fraction of the picture (presentation focal / stock focal), or sx,sy.
import sys, os
from PIL import Image, ImageDraw

sc = [float(v) for v in sys.argv[1].split(',')]; sx, sy = sc[0], sc[-1]; out = sys.argv[2]; files = sys.argv[3:]
tiles = []
for f in files:
    im = Image.open(f).convert('RGB')
    w, h = im.size
    bw, bh = w * sx, h * sy
    box = (int((w - bw) / 2), int((h - bh) / 2), int((w + bw) / 2), int((h + bh) / 2))
    px = im.load()
    outside = lit = 0
    for y in range(0, int(h * 0.92), 2):  # (the bottom 8% left out: credit lines)
        for x in range(0, w, 2):
            if box[0] <= x < box[2] and box[1] <= y < box[3]:
                continue
            outside += 1
            r, g, b = px[x, y]
            if r + g + b > 30:
                lit += 1
    d = ImageDraw.Draw(im)
    d.rectangle(box, outline=(255, 40, 40), width=2)
    d.text((6, 4), '%s: %.0f%% of the area outside the original frame (red) has picture' % (os.path.basename(f), 100.0 * lit / outside), fill=(255, 255, 0))
    print('%s outside-lit %.1f%%' % (f, 100.0 * lit / outside))
    tiles.append(im)
w, h = tiles[0].size
cols = 2 if len(tiles) > 1 else 1
rows = (len(tiles) + cols - 1) // cols
sheet = Image.new('RGB', (cols * w, rows * h), (30, 30, 30))
for i, t in enumerate(tiles):
    sheet.paste(t, ((i % cols) * w, (i // cols) * h))
sheet.save(out)
print(out)
