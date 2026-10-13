# SPDX-License-Identifier: GPL-3.0-only
from common import main

def build(p,s):
    p.box('floor_mount',(0,.012,0),(.10,.024,.17),'body',bevel=.008)
    p.cylinder('hinge',(0,.034,-.055),.012,.10,'accent',axis=(1,0,0))
    pivot=p.anchor('pivot_pedal'+s,(0,.034,-.055))
    p.bar('pedal_arm',(0,0,0),(0,.032,.065),.010,'accent',pivot)
    p.box('foot_pad',(0,.040,.072),(.085,.016,.074),'accent',pivot,bevel=.006)
    for i in (-1,0,1):p.box('tread',(0,.051,.072+i*.021),(.072,.006,.008),'dark',pivot,bevel=.001)
    p.anchor('label_pedal'+s,(0,.06,.07),pivot)

if __name__=='__main__':main('pedal-single',build)
