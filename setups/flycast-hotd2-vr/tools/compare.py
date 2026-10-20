# Compares same-named pictures in two run folders: python -I compare.py <folderA> <folderB> [pattern] [out.png]
# Prints per picture: identical / differing pixels (count, bounding box). With out.png: A | B | diff rows.
import sys, glob, os
from PIL import Image, ImageChops, ImageDraw

a_dir, b_dir = sys.argv[1], sys.argv[2]
pattern = sys.argv[3] if len(sys.argv) > 3 else 'snap_*.png'
out = sys.argv[4] if len(sys.argv) > 4 else None
rows = []
same = diff = 0
for fa in sorted(glob.glob(os.path.join(a_dir, pattern))):
    name = os.path.basename(fa)
    fb = os.path.join(b_dir, name)
    if not os.path.exists(fb):
        print(name, 'missing in B'); continue
    A = Image.open(fa).convert('RGB'); B = Image.open(fb).convert('RGB')
    if A.size != B.size:
        print(name, 'size differs', A.size, B.size); diff += 1; continue
    D = ImageChops.difference(A, B).convert('L')
    mask = D.point(lambda v: 255 if v > 24 else 0)
    n = sum(1 for v in mask.getdata() if v)
    box = mask.getbbox()
    if n == 0 and D.getbbox() is None:
        print(name, 'identical'); same += 1
    else:
        print(name, 'differs: %d pixels over 24, box %s, any-diff box %s' % (n, box, D.getbbox())); diff += 1
    rows.append((name, A, B, D))
print('identical %d, differing %d' % (same, diff))
if out and rows:
    w, h = rows[0][1].size
    tw = 320; th = int(h * tw / w)
    sheet = Image.new('RGB', (tw * 3, len(rows) * (th + 14)), (40, 40, 40))
    d = ImageDraw.Draw(sheet)
    for i, (name, A, B, D) in enumerate(rows):
        y = i * (th + 14)
        d.text((3, y + 1), name + '   A: ' + os.path.basename(a_dir.rstrip('\\/')) + '   B: ' + os.path.basename(b_dir.rstrip('\\/')) + '   |A-B| x4', fill=(255, 255, 0))
        sheet.paste(A.resize((tw, th)), (0, y + 14))
        sheet.paste(B.resize((tw, th)), (tw, y + 14))
        sheet.paste(D.point(lambda v: min(255, v * 4)).convert('RGB').resize((tw, th)), (2 * tw, y + 14))
    sheet.save(out)
    print(out)
