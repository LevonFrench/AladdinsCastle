# SPDX-License-Identifier: GPL-3.0-only
from common import main

def build(p,s):
    p.box('housing',(0,.015,0),(.095,.03,.075),'body',bevel=.010)
    p.cylinder('socket',(0,.034,0),.025,.006,'accent')
    button=p.anchor('button_view'+s,(0,.035,0))
    p.cylinder('cap',(0,.007,0),.019,.012,'glass',button)
    p.anchor('label_view'+s,(0,.024,0),button)

if __name__=='__main__':main('panel-view-one',build)
