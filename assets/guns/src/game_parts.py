"""Original weapon-prop construction helpers; millimetres, no imported geometry."""
import math

def grip(d,z=0,fore=False):
    profile=[(-24,5),(13,5),(18,-18),(34,-89),(28,-102),(6,-100),(-7,-85),(-18,-35)]
    d.outline('fore_grip' if fore else 'hand_grip',[(a+z,b) for a,b in profile],32,bevel=2)
    panel=[(-18,-6),(7,-6),(25,-82),(21,-91),(7,-90),(-1,-79),(-12,-33)]
    for side in (-1,1):
        d.outline(('fore_panel_' if fore else 'grip_panel_')+str(side),[(a+z,b) for a,b in panel],3,'dark',x=side*16,bevel=1)
    d.box('player_inlay_'+str(z),(0,-98,z+15),(18,3,9),'accent',bevel=.4)
    if fore:d.node('grip_two',(0,0,z))

def controls(d):
    d.ring('trigger_guard',[(-78,12),(-34,12),(-25,0),(-20,-29),(-33,-42),(-68,-42),(-85,-23),(-86,-1)],
           [(-71,5),(-40,5),(-33,-1),(-29,-24),(-38,-33),(-63,-33),(-75,-19),(-77,-2)],15,'body')
    d.outline('trigger',[(-46,13),(-40,13),(-41,4),(-48,-13),(-53,-20),(-57,-18),(-52,-5),(-47,4)],9,'body','pivot_trigger',bevel=.7)
    d.node('button_reload',(18,-15,-7))
    d.cylinder('reload_catch',(18,-15,-7),3.5,3,'dark','button_reload',axis='x')
    for side in (-1,1):d.box('player_band_'+str(side),(side*15,16,-38),(1.5,5,14),'accent',bevel=.3)

def sweep(d,name,points,radii,material='body',parent=None,flatten=1):
    d.part(name,'sweep',(0,0,0),material,parent,0,points=[tuple(v/1000 for v in p) for p in points],radii=[r/1000 for r in radii],flatten=flatten)

def chamber_ring(d,parent,y,z,radius=16):
    for i in range(6):
        angle=i*math.tau/6
        d.cylinder('chamber_'+str(i),(math.cos(angle)*radius,y+math.sin(angle)*radius,z),4.5,2,'dark',parent)

def sight_bead(d,y,z,parent=None):
    """Seated optical bead on the lower blade, below the notch's clear corridor."""
    d.cylinder('front_sight_bead',(0,y-1,z+3.9),.7,.6,'glass',parent)
