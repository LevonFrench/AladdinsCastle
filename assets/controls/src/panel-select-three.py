# SPDX-License-Identifier: GPL-3.0-only
from common import main

def build(p,s):
    p.box('panel_shell',(0,.014,0),(.24,.028,.09),'body',bevel=.010)
    p.box('panel_face',(0,.030,0),(.222,.009,.075),'accent',bevel=.004)
    for i,x in enumerate((-.072,0,.072),1):
        p.cylinder('socket'+str(i),(x,.038,0),.024,.006,'dark')
        button=p.anchor('button_select_'+str(i)+s,(x,.04,0))
        p.cylinder('select_'+str(i),(0,.007,0),.019,.012,'glass',button)
        for mark in range(i):p.box('mark',((mark-(i-1)/2)*.005,.014,0),(.0025,.001,.010),'accent',button,bevel=0)
        p.anchor('label_select_'+str(i)+s,(0,.022,0),button)

if __name__=='__main__':main('panel-select-three',build)
