# SPDX-License-Identifier: GPL-3.0-only
from common import main
import math

def build(p,s):
    p.box('foot_box',(0,.015,0),(.29,.03,.29),'body',bevel=.008)
    p.box('heel_rubber',(0,.033,.075),(.26,.008,.09),'dark')
    for name,x,width in [('brake',-.070,.078),('accelerator',.073,.055)]:
        p.cylinder(name+'_hinge',(x,.039,-.085),.011,width+.024,'accent',axis=(1,0,0))
        pivot=p.anchor('pivot_'+name+s,(x,.039,-.085))
        p.bar(name+'_arm',(0,0,0),(0,.060,.085),.010,'accent',pivot)
        face=p.box(name+'_pad',(0,.067,.09),(width,.014,.125),'accent',pivot,bevel=.006)
        face.rotation_euler.x=math.radians(-24)
        count=5 if p.lod==0 else 3
        for j in range(count):
            z=(j-(count-1)/2)*(.023 if p.lod==0 else .045)
            p.box(name+'_tread'+str(j),(0,.010,z),(width*.82,.007,.008),'dark',face,bevel=.001)
        p.anchor('label_'+name+s,(0,.086,.11),pivot)

if __name__=='__main__':main('pedals-dual',build)
