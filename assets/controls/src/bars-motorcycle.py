# SPDX-License-Identifier: GPL-3.0-only
from common import main

def build(p,s):
    p.box('stem_mount',(0,-.025,0),(.10,.05,.09),'body',bevel=.01)
    pivot=p.anchor('pivot_steering'+s,(0,0,0))
    for side in (-1,1):
        p.bar('bar_inner',(0,0,0),(side*.13,.025,-.015),.012,'accent',pivot)
        p.bar('bar_rise',(side*.13,.025,-.015),(side*.19,.052,.015),.012,'accent',pivot)
        grip=p.anchor(('pivot_throttle' if side==1 else 'grip_left')+s,(side*.25,.052,.015),pivot)
        p.cylinder('grip',(0,0,0),.019,.14,'dark',grip,axis=(1,0,0))
        if not p.lod:
            for i in (-3,-2,-1,0,1,2,3):p.torus('grip_ring',(i*.016,0,0),.019,.0012,'dark',grip,axis=(1,0,0),minor_segments=4)
        p.cylinder('end_cap',(side*.071,0,0),.020,.008,'accent',grip,axis=(1,0,0))
        if side==1:p.box('throttle_index',(-.046,.019,0),(.024,.003,.006),'glass',grip,bevel=.001)
        brake=p.anchor(('pivot_front_brake' if side==1 else 'pivot_clutch')+s,(side*.18,.05,-.016),pivot)
        p.bar('lever_root',(0,0,0),(side*.035,-.003,-.042),.006,'accent',brake)
        p.bar('lever_tip',(side*.035,-.003,-.042),(side*.11,-.010,-.036),.006,'accent',brake)
        p.sphere('lever_end',(side*.11,-.010,-.036),(.009,.009,.009),'accent',brake)
    p.anchor('grab_right'+s,(.25,.052,.015),pivot)
    p.anchor('label_steering'+s,(0,.07,0),pivot)
    p.anchor('label_throttle'+s,(.25,.08,.015),pivot)
    p.anchor('label_front_brake'+s,(.24,.06,-.05),pivot)

if __name__=='__main__':main('bars-motorcycle',build)
