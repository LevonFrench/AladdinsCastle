# Gun script for ACVR_GUN: shots over a point list (picture px of 640x480), a short settle before each pull.  python gunplan.py <start s> <spacing s> "x:y;x:y;..." [reload_every]
import sys
start=float(sys.argv[1]); step=float(sys.argv[2]); pts=[tuple(float(v) for v in p.split(':')) for p in sys.argv[3].split(';')]
every=int(sys.argv[4]) if len(sys.argv)>4 else 0
g=[]; t=start; n=0
for x,y in pts:
    if every and n and n%every==0:
        g+=['%.2f:-10000:-10000:2'%t,'%.2f:-10000:-10000:0'%(t+0.15)]; t+=0.8
    X,Y=round(x/640*10000),round(y/480*10000)
    g+=['%.2f:%d:%d:0'%(t,X,Y),'%.2f:%d:%d:1'%(t+step*0.4,X,Y),'%.2f:%d:%d:0'%(t+step*0.55,X,Y)]
    t+=step; n+=1
print(';'.join(g)); print('end %.2f'%t,file=sys.stderr)
