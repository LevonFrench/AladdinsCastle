# SPDX-License-Identifier: GPL-3.0-only
from common import main

def build(p,s):
    p.box('base',(0,.034,0),(.47,.068,.47),'body',bevel=.015)
    p.cylinder('turntable',(0,.075,0),.15,.026,'accent')
    swing=p.anchor('pivot_swing'+s,(0,.09,0))
    edge=p.anchor('pivot_edge'+s,(0,.025,0),swing)
    p.box('bridge',(0,0,0),(.29,.028,.12),'accent',edge)
    for x in (-.09,.09):
        p.box('ski_plate',(x,.027,0),(.125,.025,.36),'body',edge,bevel=.025)
        p.box('foot_rubber',(x,.044,0),(.095,.014,.245),'dark',edge,bevel=.012)
        for z in (-.065,.065):
            for sign in (-1,1):p.bar('binding_side',(x+sign*.048,.043,z),(x+sign*.048,.083,z),.006,'accent',edge)
            p.bar('binding_strap',(x-.048,.083,z),(x+.048,.083,z),.009,'dark',edge)
    for i,x in enumerate((-.29,.29),1):
        p.box('pole_foot',(x,.019,-.13),(.06,.038,.095),'body')
        p.bar('pole',(x,.025,-.13),(x,.93,-.13),.015,'accent')
        p.bar('pole_bend',(x,.93,-.13),(x,1.02,-.035),.015,'accent')
        p.bar('pole_grip',(x,1.02,-.035),(x,1.02,.08),.022,'dark')
        button=p.anchor('button_pole_'+str(i)+s,(x,1.02,.082))
        p.cylinder('pole_cap',(0,0,0),.019,.007,'glass',button,axis=(0,0,1))
        p.anchor('grab_pole_'+str(i)+s,(x,1.02,.01))
    p.anchor('label_swing'+s,(0,.13,-.20),swing)
    p.anchor('label_edge'+s,(0,.08,.20),edge)

if __name__=='__main__':main('platform-ski',build)
