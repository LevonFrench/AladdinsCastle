# SPDX-License-Identifier: GPL-3.0-only
def build_panel(p,s,count):
    width=.066*count+.020
    p.box('panel_shell',(0,.017,0),(width,.034,.11),'body',bevel=.011)
    p.box('panel_face',(0,.038,0),(width-.016,.010,.096),'accent',bevel=.005)
    for i in range(1,count+1):
        x=(i-(count+1)/2)*.066
        p.cylinder('socket'+str(i),(x,.048,0),.024,.009,'dark')
        button=p.anchor('button_action_'+str(i)+s,(x,.048,0))
        p.cylinder('cap'+str(i),(0,.009,0),.019,.014,'glass',button)
        for mark in range(i):p.box('mark',((mark-(i-1)/2)*.005,.017,0),(.0025,.001,.010),'body',button,bevel=0)
        p.anchor('label_action_'+str(i)+s,(0,.029,0),button)
