# SPDX-License-Identifier: GPL-3.0-only
from common import main
import math

def build(p,s):
    p.box('column', (0,0,-.062),(.14,.13,.08),'body')
    p.cylinder('shaft',(0,0,-.028),.026,.04,axis=(0,0,1))
    spin=p.anchor('pivot_wheel'+s,(0,0,0))
    p.torus('rim',(0,0,0),.157,.019,parent=spin)
    p.cylinder('hub',(0,0,.006),.052,.038,'body',spin,axis=(0,0,1))
    p.cylinder('hub_inset',(0,0,.028),.032,.010,'accent',spin,axis=(0,0,1))
    for i,angle in enumerate((30,150,270)):
        a=math.radians(angle);x,y=math.cos(a),math.sin(a)
        p.bar('spoke'+str(i),(.033*x,.033*y,0),(.149*x,.149*y,0),.012,'accent',spin)
        if not p.lod:
            p.cylinder('hub_fastener'+str(i),(.043*x,.043*y,.029),.003,.004,'dark',spin,axis=(0,0,1),bevel=0)
    for name,pos in [('grab_left',(-.157,0,0)),('grab_right',(.157,0,0)),('label_steering',(0,.178,0))]:p.anchor(name+s,pos,spin)

if __name__=='__main__':main('wheel-three-spoke',build)
