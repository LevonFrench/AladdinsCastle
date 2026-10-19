# Contact sheet of the pictures in a folder: python -I sheet.py <folder> [pattern] [columns] [width]
import sys, glob, os
from PIL import Image, ImageDraw

folder = sys.argv[1]
pattern = sys.argv[2] if len(sys.argv) > 2 else '*.png'
cols = int(sys.argv[3]) if len(sys.argv) > 3 else 4
tw = int(sys.argv[4]) if len(sys.argv) > 4 else 320
files = sorted(f for f in glob.glob(os.path.join(folder, pattern)) if not os.path.basename(f).startswith('sheet'))
if not files:
    sys.exit('no pictures')
ims = [Image.open(f).convert('RGB') for f in files]
th = int(tw * ims[0].height / ims[0].width)
rows = (len(ims) + cols - 1) // cols
sheet = Image.new('RGB', (cols * tw, rows * (th + 14)), (40, 40, 40))
d = ImageDraw.Draw(sheet)
for i, (f, im) in enumerate(zip(files, ims)):
    x, y = (i % cols) * tw, (i // cols) * (th + 14)
    sheet.paste(im.resize((tw, th)), (x, y + 14))
    d.text((x + 3, y + 1), os.path.basename(f), fill=(255, 255, 0))
out = os.path.join(folder, 'sheet.png')
sheet.save(out)
print(out)
