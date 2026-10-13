# SPDX-License-Identifier: GPL-3.0-only
from common import main

def build(p,s):
    p.box('housing',(0,.023,0),(.15,.046,.17),'body',bevel=.009)
    # Five solid plate islands leave a real H-shaped opening, not a painted H.
    for i,(x,z,dx,dz) in enumerate([(-.061,0,.026,.15),(.061,0,.026,.15),(0,-.050,.050,.062),(0,.050,.050,.062),(0,0,.034,.008)]):
        if i==4:continue
        p.box('gate_plate'+str(i),(x,.050,z),(dx,.005,dz),'accent',bevel=.001)
    p.box('gate_well',(0,.046,0),(.082,.004,.143),'dark',bevel=.001)
    lever=p.anchor('pivot_shift'+s,(0,.05,0))
    p.sphere('boot',(0,.010,0),(.034,.02,.034),'dark',lever)
    p.cylinder('lever',(0,.062,0),.006,.11,'accent',lever)
    p.sphere('knob',(0,.123,0),(.025,.029,.025),'dark',lever)
    p.cylinder('knob_cap',(0,.149,0),.014,.003,'glass',lever)
    for i,(x,z) in enumerate([(-.037,-.054),(-.037,.054),(.037,-.054),(.037,.054)],1):p.anchor('gear_'+str(i)+s,(x,.053,z))
    p.anchor('grab_knob'+s,(0,.123,0),lever);p.anchor('label_shifter'+s,(.07,.09,0))

if __name__=='__main__':main('shifter-four-gate',build)
