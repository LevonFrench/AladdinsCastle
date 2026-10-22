"""Mario Strikers Charged pointer check: python measure_tip_hand.py <folder of aim-<i>_<u>_<v>.png>. Finds the blue hand cursor (colour 40,130,190), takes the TOP of the hand (the fingertip, the game's hot spot) and prints aim vs tip per point plus the per-axis scale and shift. Needs Pillow and numpy."""
import sys, glob, os, re, statistics
from PIL import Image
d=sys.argv[1]
rows=[]
for f in sorted(glob.glob(os.path.join(d,'aim-*_*.png'))):
    m=re.search(r'aim-(\d+)_(-?[\d.]+)_(-?[\d.]+)\.png',f)
    i,u,v=int(m.group(1)),float(m.group(2)),float(m.group(3))
    im=Image.open(f).convert('RGB'); w,h=im.size; px=im.load()
    pts=[(x,y) for y in range(h) for x in range(w) if (lambda p: abs(p[0]-40)+abs(p[1]-130)+abs(p[2]-190)<90 and p[2]>p[0]+60)(px[x,y])]
    mx=statistics.median(p[0] for p in pts); my=statistics.median(p[1] for p in pts)
    sel=[p for p in pts if abs(p[0]-mx)<40 and abs(p[1]-my)<50]
    ymin=min(p[1] for p in sel)
    top=[p[0] for p in sel if p[1]<=ymin+3]
    tip=(sum(top)/len(top)/w, ymin/h)
    aim=((u+1)/2,(1-v)/2)
    rows.append((i,aim,tip))
    print(i,'aim=(%.3f,%.3f) tip=(%.3f,%.3f) err=(%+.3f,%+.3f)'%(aim+tip+(tip[0]-aim[0],tip[1]-aim[1])))
import numpy as np
A=np.array([[r[1][0],r[1][1],r[2][0],r[2][1]] for r in rows])
for k,name in ((0,'x'),(1,'y')):
    a=A[:,k]-0.5; t=A[:,k+2]-0.5
    s=(a*t).sum()/(a*a).sum(); sh=(t-s*a).mean()
    print(name,'scale %.3f shift %+.3f maxerr %.3f'%(s,sh,abs(t-a).max()))
