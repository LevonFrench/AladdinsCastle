# SPDX-License-Identifier: GPL-3.0-only
from common import main

def build(p,s):
    p.box('housing',(0,.023,0),(.15,.046,.17),'body',bevel=.009)
    # Four islands expose both vertical slots and a broad neutral cross-slot.
    # The small boot and knob cannot cover the cross-slot from above.
    for i,(x,z,dx,dz) in enumerate([(-.061,0,.026,.15),(.061,0,.026,.15),(0,-.054,.050,.042),(0,.054,.050,.042)]):
        p.box('gate_plate'+str(i),(x,.050,z),(dx,.005,dz),'accent',bevel=.001)
    p.box('gate_well',(0,.046,0),(.100,.004,.150),'dark',bevel=.001)
    lever=p.anchor('pivot_shift'+s,(0,.05,0))
    p.sphere('boot',(0,.004,0),(.011,.006,.011),'dark',lever)
    p.cylinder('lever',(0,.062,0),.006,.11,'accent',lever)
    p.sphere('knob',(0,.123,0),(.020,.025,.020),'dark',lever)
    p.cylinder('knob_cap',(0,.146,0),.012,.003,'glass',lever)
    segments={'1':'bc','2':'abged','3':'abgcd','4':'fgbc'}
    lines={'a':((-1,-2),(1,-2)),'b':((1,-2),(1,0)),
           'c':((1,0),(1,2)),'d':((-1,2),(1,2)),
           'e':((-1,0),(-1,2)),'f':((-1,-2),(-1,0)),
           'g':((-1,0),(1,0))}
    for i,(x,z) in enumerate([(-.037,-.054),(-.037,.054),(.037,-.054),(.037,.054)],1):
        p.anchor('gear_'+str(i)+s,(x,.053,z))
        for segment in segments[str(i)]:
            a,b=lines[segment]
            p.bar('gear_mark_'+str(i)+'_'+segment,((-.061 if x<0 else .061)+a[0]*.003,.054,z+a[1]*.003),
                  ((-.061 if x<0 else .061)+b[0]*.003,.054,z+b[1]*.003),.0008,'dark')
    p.anchor('grab_knob'+s,(0,.123,0),lever);p.anchor('label_shifter'+s,(.07,.09,0))

if __name__=='__main__':main('shifter-four-gate',build)
