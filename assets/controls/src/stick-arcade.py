# SPDX-License-Identifier: GPL-3.0-only
from common import main

def build(p,s):
    p.box('housing',(0,.025,0),(.16,.05,.17),'body',bevel=.012)
    p.box('deck',(0,.055,0),(.145,.012,.155),'accent',bevel=.007)
    p.cylinder('dust_ring',(0,.065,0),.037,.005,'dark')
    roll=p.anchor('pivot_stick_x'+s,(0,.062,0))
    pitch=p.anchor('pivot_stick_y'+s,(0,0,0),roll)
    p.cylinder('shaft',(0,.047,0),.007,.085,'accent',pitch)
    p.cylinder('lower_collar',(0,.012,0),.015,.013,'dark',pitch)
    p.sphere('ball_top',(0,.105,0),(.029,.029,.029),'glass',pitch)
    p.cylinder('cap_inset',(0,.131,0),.010,.003,'body',pitch)
    p.anchor('grab_knob'+s,(0,.105,0),pitch)
    p.anchor('label_joystick'+s,(-.05,.08,0),pitch)
    if not p.lod:
        for x in (-.061,.061):
            for z in (-.066,.066):p.cylinder('fastener',(x,.062,z),.003,.002,'dark')

if __name__=='__main__':main('stick-arcade',build)
