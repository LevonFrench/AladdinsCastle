# SPDX-License-Identifier: GPL-3.0-only
from common import main

def build(p,s):
    p.box('pedestal',(0,.026,0),(.17,.052,.19),'body',bevel=.015)
    p.box('gimbal_plate',(0,.055,0),(.12,.012,.13),'accent',bevel=.012)
    p.cylinder('gimbal_collar',(0,.065,0),.044,.026,'dark')
    roll=p.anchor('pivot_stick_x'+s,(0,.075,0))
    pitch=p.anchor('pivot_stick_y'+s,(0,0,0),roll)
    p.sphere('joint',(0,0,0),(.029,.023,.029),'accent',pitch)
    p.bar('stem',(0,0,0),(0,.115,.010),.014,'accent',pitch)
    p.box('palm_rest',(0,.084,.022),(.079,.016,.066),'body',pitch,bevel=.009)
    p.sphere('grip_core',(0,.143,.008),(.026,.060,.029),'dark',pitch)
    for i in (-1,0,1):p.box('finger_ridge',(0,.123+i*.020,-.019),(.037,.007,.014),'dark',pitch,bevel=.003)
    p.box('thumb_head',(0,.198,0),(.060,.031,.060),'body',pitch,bevel=.012)
    event=p.anchor('button_event'+s,(.016,.216,-.007),pitch)
    p.cylinder('event_socket',(0,0,0),.013,.005,'accent',event)
    p.cylinder('event_cap',(0,.004,0),.010,.005,'glass',event)
    trigger=p.anchor('pivot_trigger'+s,(0,.175,-.030),pitch)
    p.box('trigger',(0,-.020,-.004),(.025,.044,.009),'glass',trigger,bevel=.004)
    p.anchor('grab_handle'+s,(0,.143,.008),pitch)
    p.anchor('label_stick'+s,(-.055,.145,0),pitch)
    p.anchor('label_trigger'+s,(.025,-.023,-.009),trigger)
    p.anchor('label_event'+s,(0,.018,0),event)
    if not p.lod:
        for x in (-.061,.061):
            for z in (-.073,.073):p.cylinder('fastener',(x,.054,z),.004,.002,'dark')

if __name__=='__main__':main('stick-flight',build)
