# Where the game's picture lands in a headset-shaped render compared with the true (4:3) picture, from two pictures of the same moment.
#   python register_x.py <reference.png> <test.png> [...]   fits x_test = a * x_ref + b (picture x as a share of 640 after resampling both to 640 wide)
import sys, numpy as np
from PIL import Image
from scipy.optimize import minimize
from scipy.ndimage import map_coordinates
def gray(p):
    im=Image.open(p).convert('L'); w,h=im.size
    if w!=640: im=im.resize((640,h),Image.BILINEAR)
    return np.asarray(im,float)
def fit(R,T):
    ys=np.arange(60,400,3); xs=np.arange(8,632,2)
    gy,gx=np.meshgrid(ys,xs,indexing='ij')
    ref=R[gy,gx]
    def cost(p):
        a,b=p; xt=a*gx+b
        ok=(xt>=2)&(xt<=637)
        if ok.sum()<2000: return 1e9
        v=map_coordinates(T,[gy[ok],xt[ok]],order=1)
        return float(np.mean((v-ref[ok])**2))
    best=None
    for a in np.arange(0.6,1.15,0.05):
        for b in np.arange(-320,120,20):
            c=cost((a,b))
            if best is None or c<best[0]: best=(c,a,b)
    r=minimize(cost,[best[1],best[2]],method='Nelder-Mead',options={'xatol':0.05,'fatol':0.01,'maxiter':400})
    return r.x[0],r.x[1],r.fun
if __name__=='__main__':
    ref=sys.argv[1]; R=gray(ref)
    for p in sys.argv[2:]:
        a,b,c=fit(R,gray(p))
        # apparent position of true pixel x: a*x+b ; aim error (apparent - true) at x = 60, 320, 580
        e=[(a*x+b)-x for x in (60,320,580)]
        print('%s: x_apparent = %.3f * x %+.1f (rms %.1f grey levels); aim error at x=60/320/580: %+.0f / %+.0f / %+.0f px'%(p.split('\\')[-2]+'\\'+p.split('\\')[-1],a,b,c**.5,e[0],e[1],e[2]))
