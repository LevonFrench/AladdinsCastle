"""Original funcon (the Funcon); GPL-3.0-only source, CC0 generated asset.

The owner's PlayStation light-gun model: GunCon-like proportions (deep box barrel with a
round lens in a flat muzzle, long grip angled back, large trigger guard, one oval button on
each side below the barrel, cable from the grip base) with an arcade-style blowback slide.
Authored dimensions, no imported mesh, tracing, logos or markings.
"""
from pathlib import Path
import sys
PARAMETERS = dict(length_mm=246,barrel_width_mm=44,grip_angle_deg=18)

# Side profiles in millimetres: (z along the barrel, negative towards the muzzle; y up).
BODY_OUTLINE = [(-172,8),(-172,31),(-120,31),(-60,31),(0,31),(32,31),(36,24),(36,8),
 (44,-20),(54,-55),(62,-90),(64,-108),(58,-118),(44,-122),(28,-120),(18,-110),
 (12,-80),(4,-48),(-4,-20),(-14,0),(-30,8),(-80,8),(-130,8)]
SLIDE_OUTLINE = [(-200,32),(-200,68),(-195,73),(-185,75),(-120,75),(-40,75),(36,75),
 (44,71),(46,62),(46,32),(10,31),(-100,31)]
GRIP_OUTLINE = [(-8,4),(26,4),(40,-30),(50,-70),(56,-104),(50,-112),(32,-114),(22,-104),(14,-74),(4,-40)]
BUTTON_OVAL = [(-152,13),(-148,9),(-136,9),(-132,13),(-132,22),(-136,26),(-148,26),(-152,22)]

def make_design():
    from common import Design
    d=Design(246)
    d.node('slide_recoil',(0,53,-80)); d.node('limit_recoil',(0,53,-65))
    d.outline('frame',BODY_OUTLINE,38)
    d.outline('barrel_shell',SLIDE_OUTLINE,44,parent='slide_recoil',bevel=3)
    for side in (-1,1): d.outline('hand_grip' if side==1 else 'grip_panel_left',GRIP_OUTLINE,6,'dark',x=side*17,bevel=2)
    d.ring('trigger_guard',[(-112,10),(-34,10),(-22,-2),(-16,-36),(-28,-50),(-94,-50),(-112,-34),(-116,-4)],
           [(-104,2),(-40,2),(-30,-6),(-26,-32),(-34,-42),(-90,-42),(-104,-28),(-107,-4)],20)
    d.markers((0,52,-200),(0,77,-170),(0,77,30),(0,12,-45))
    d.functional_sights((0,77,-170),(0,77,30),'slide_recoil')
    d.outline('trigger',[(-50,16),(-41,16),(-41,5),(-47,-8),(-52,-24),(-50,-30),(-57,-27),(-58,-19),(-53,-4),(-49,5)],12,'accent','pivot_trigger',bevel=1)
    d.cylinder('muzzle_lens',(0,52,-199),12,4,'glass','slide_recoil')
    d.cylinder('cable_boss',(0,-121,38),8,12,'accent',axis='y')
    for bid,side in (('a',-1),('b',1)):
        d.node('button_'+bid,(side*19,17,-142))
        d.outline('button_'+bid+'_cap',BUTTON_OVAL,4,'accent','button_'+bid,x=side*19,bevel=1)
    # Rear grip serrations and a dark seam mark the moving slide.
    for side in (-1,1):
        d.box('slide_seam_'+str(side),(side*22,33,-80),(2,2,230),'dark','slide_recoil',.4)
        for i in range(6): d.box('serration_'+str(side)+'_'+str(i),(side*22,55,6+i*6),(2,26,2),'accent','slide_recoil',.4)
    return d

if __name__ == '__main__':
    sys.path.insert(0,str(Path(__file__).resolve().parent))
    from common import run
    run('funcon',PARAMETERS)
