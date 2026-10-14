# SPDX-License-Identifier: GPL-3.0-only
from common import main

def build(p,s):
    p.box('frame',(0,.03,0),(.62,.06,.49),'body',bevel=.016)
    for i,x in enumerate((.18,-.18),1):
        p.box('pump_plinth',(x,.082,-.12),(.095,.105,.12),'body',bevel=.009)
        p.cylinder('pump_hinge',(x,.145,-.12),.026,.13,'accent',axis=(1,0,0))
        pivot=p.anchor('pivot_pump_'+str(i)+s,(x,.145,-.12))
        p.bar('lever',(0,0,0),(0,.11,.28),.018,'accent',pivot)
        p.cylinder('handle',(0,.11,.28),.020,.13,'dark',pivot,axis=(1,0,0))
        for side in (-1,1):p.cylinder('handle_cap',(side*.067,.11,.28),.022,.005,'glass',pivot,axis=(1,0,0))
        p.anchor('grab_handle_'+str(i)+s,(0,.11,.28),pivot)
        p.anchor('label_pump_'+str(i)+s,(0,.15,.28),pivot)
        # Hinge stands above the deck; even the forward tip clears it at .18 rad.
        p.box('pedal_mount',(x,.075,.045),(.085,.030,.040),'body',bevel=.004)
        p.cylinder('pedal_hinge',(x,.093,.045),.012,.115,'accent',axis=(1,0,0))
        pedal=p.anchor('button_pedal_'+str(i)+s,(x,.093,.045))
        p.box('pedal',(0,.012,.06),(.105,.022,.14),'accent',pedal,bevel=.007)
        for j in (-1,0,1):p.box('pedal_tread',(0,.026,.06+j*.035),(.085,.008,.008),'dark',pedal,bevel=.001)
    p.box('centre_trim',(0,.064,0),(.034,.008,.45),'accent',bevel=.002)

if __name__=='__main__':main('pump-handcar',build)
