# SPDX-License-Identifier: GPL-3.0-only
from common import main

def build(p,s):
    p.box('mount',(0,.025,0),(.07,.05,.18),'body',bevel=.008)
    p.cylinder('hinge',(0,.065,.055),.026,.08,'accent',axis=(1,0,0))
    pivot=p.anchor('pivot_handbrake'+s,(0,.065,.055))
    p.bar('lever',(0,0,0),(0,.15,-.08),.013,'accent',pivot)
    p.bar('handle',(0,.12,-.064),(0,.24,-.128),.022,'dark',pivot)
    p.cylinder('end_button',(0,.244,-.130),.014,.010,'glass',pivot,axis=(0,.88,-.47))
    p.anchor('grab_handle'+s,(0,.19,-.10),pivot)
    p.anchor('label_handbrake'+s,(0,.26,-.14),pivot)

if __name__=='__main__':main('handbrake-lever',build)
