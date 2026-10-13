# SPDX-License-Identifier: GPL-3.0-only
from common import main

def build(p,s):
    p.box('shell',(0,.025,0),(.12,.05,.18),'body',bevel=.01)
    for x in (-.043,.043):p.box('rail',(x,.055,0),(.028,.007,.16),'accent',bevel=.002)
    p.box('slot',(0,.052,0),(.045,.006,.15),'dark')
    pivot=p.anchor('pivot_shift'+s,(0,.056,0))
    p.sphere('boot',(0,.008,0),(.028,.015,.035),'dark',pivot)
    p.cylinder('stem',(0,.066,0),.007,.12,'accent',pivot)
    p.sphere('knob',(0,.128,0),(.024,.028,.028),'dark',pivot)
    p.anchor('grab_knob'+s,(0,.128,0),pivot)
    p.anchor('label_shifter'+s,(.07,.07,0))

if __name__=='__main__':main('shifter-sequential',build)
