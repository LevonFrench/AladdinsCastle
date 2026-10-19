# Draws where the gun was sent (red cross) on snapshots, 2x crops around it, into one sheet.
# python marks.py <folder> <out.png> name:x:y [name:x:y ...]   (x, y in 640x480 pixels)
import sys, os
from PIL import Image, ImageDraw

folder, out = sys.argv[1], sys.argv[2]
items = [a.split(':') for a in sys.argv[3:]]
R = 70
tiles = []
for name, x, y in items:
    x, y = float(x), float(y)
    im = Image.open(os.path.join(folder, name)).convert('RGB')
    crop = im.crop((int(x - R), int(y - R), int(x + R), int(y + R))).resize((4 * R, 4 * R), Image.NEAREST)
    d = ImageDraw.Draw(crop)
    c = 2 * R
    d.line((c - 18, c, c + 18, c), fill=(255, 0, 0), width=2)
    d.line((c, c - 18, c, c + 18), fill=(255, 0, 0), width=2)
    d.text((4, 4), '%s gun %.0f,%.0f' % (name, x, y), fill=(255, 255, 0))
    tiles.append(crop)
cols = 3
rows = (len(tiles) + cols - 1) // cols
sheet = Image.new('RGB', (cols * 4 * R, rows * 4 * R), (30, 30, 30))
for i, t in enumerate(tiles):
    sheet.paste(t, ((i % cols) * 4 * R, (i // cols) * 4 * R))
sheet.save(out)
print(out)
