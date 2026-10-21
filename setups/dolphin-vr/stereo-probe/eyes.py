"""Classify each probe capture in a folder: 'stereo' (left|right eye layers, probe layout) or
'mono' (the presented XFB had one layer, so the probe fell back to Dolphin's normal picture).
The probe writes exactly 2 x XFB width; a mono picture keeps the display aspect instead.
Usage: python eyes.py <dir> [<dir> ...]"""
import re
import sys
from pathlib import Path

from PIL import Image

for d in map(Path, sys.argv[1:]):
    files = sorted((p for p in d.glob("p*.png") if re.fullmatch(r"p\d+", p.stem)), key=lambda p: int(p.stem[1:]))
    st, mono, names = 0, 0, []
    for f in files:
        w, h = Image.open(f).size
        eye_aspect = (w / 2) / h
        # stereo: each eye is the raw XFB (e.g. 640x448 -> 1.43, 608x456 -> 1.33);
        # mono: display aspect 16:9 (1.78) or 4:3 at full width (1.33 at w, i.e. 0.67 per half)
        if w % 2 == 0 and 1.2 <= eye_aspect <= 1.5:
            st += 1
        else:
            mono += 1
            names.append(f.stem)
    print(f"{d.name}: {len(files)} pictures, stereo {st}, mono {mono}" + (f" ({' '.join(names)})" if names else ""))
