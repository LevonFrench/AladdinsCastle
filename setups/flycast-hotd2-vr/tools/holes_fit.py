# Bullet-hole finder: darker blobs between a "before" and an "after" picture (restricted to a box), matched to the aimed points.
# python -I holes_fit.py <before.png> <after.png> "x0,y0,x1,y1" "x:y;x:y;..."   (picture pixels; for a wide picture pass --ox offset of the original frame's left edge, --scale)
import sys, numpy as np
from PIL import Image
from collections import deque
args=[a for a in sys.argv[1:] if not a.startswith('--')]
opt=dict(a[2:].split('=') for a in sys.argv[1:] if a.startswith('--'))
ox=float(opt.get('ox',0)); oy=float(opt.get('oy',0)); sx=float(opt.get('scale',1)); thr=int(opt.get('thr',40))
a=np.asarray(Image.open(args[0]).convert('L')).astype(int); b=np.asarray(Image.open(args[1]).convert('L')).astype(int)
x0,y0,x1,y1=[int(v) for v in args[2].split(',')]
aim=[tuple(float(v) for v in p.split(':')) for p in args[3].split(';')]
d=a-b
m=np.zeros_like(d,bool); m[y0:y1,x0:x1]=d[y0:y1,x0:x1]>thr
seen=np.zeros_like(m); blobs=[]
H,W=m.shape
for y in range(H):
    for x in range(W):
        if m[y,x] and not seen[y,x]:
            q=deque([(y,x)]); seen[y,x]=True; pts=[]
            while q:
                cy,cx=q.popleft(); pts.append((cy,cx))
                for dy in (-1,0,1):
                    for dx in (-1,0,1):
                        ny,nx=cy+dy,cx+dx
                        if 0<=ny<H and 0<=nx<W and m[ny,nx] and not seen[ny,nx]:
                            seen[ny,nx]=True; q.append((ny,nx))
            if 2<=len(pts)<=60:
                w=np.array([d[p] for p in pts],float)
                blobs.append((sum(p[1]*wi for p,wi in zip(pts,w))/w.sum(), sum(p[0]*wi for p,wi in zip(pts,w))/w.sum(), len(pts)))
# convert picture px to game px
blobs=[((bx-ox)/sx,(by-oy)/sx,n) for bx,by,n in blobs]
print('blobs',len(blobs))
errs=[]
for ax,ay in aim:
    best=min(blobs,key=lambda t:(t[0]-ax)**2+(t[1]-ay)**2,default=None)
    if best is None: print('aim %.1f,%.1f: none'%(ax,ay)); continue
    ex,ey=best[0]-ax,best[1]-ay
    if (ex**2+ey**2)**.5>25: print('aim %.1f,%.1f: no hole within 25 px (nearest %.1f,%.1f)'%(ax,ay,best[0],best[1])); continue
    errs.append((ex,ey)); print('aim %.1f,%.1f hole %.1f,%.1f err %+.1f %+.1f n=%d'%(ax,ay,best[0],best[1],ex,ey,best[2]))
if errs:
    e=np.array(errs); print('mean err %+.2f %+.2f  max|err| %.2f %.2f  rms %.2f'%(e[:,0].mean(),e[:,1].mean(),abs(e[:,0]).max(),abs(e[:,1]).max(),np.sqrt((e**2).sum(1).mean())))
