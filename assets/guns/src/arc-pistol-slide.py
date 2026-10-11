"""Original arc-pistol-slide; GPL-3.0-only source, CC0 generated asset.

Authored dimensions, no imported mesh, tracing, logos or markings.
"""
from pathlib import Path
import sys
PARAMETERS = dict(length_mm=255,barrel_width_mm=40,grip_angle_deg=15)

BODY_OUTLINE = [(-188,13),(-187,24),(-177,28),(-150,28),(-120,28),(-90,28),
 (-60,28),(-35,27),(-10,25),(12,24),(24,19),(30,10),(29,3),(16,-6),
 (20,-25),(27,-52),(35,-82),(34,-91),(28,-97),(16,-97),(2,-92),(-9,-87),
 (-14,-75),(-20,-53),(-26,-27),(-33,-8),(-41,5),(-63,9),(-88,9),
 (-115,10),(-145,11),(-174,11)]
SLIDE_OUTLINE = [(-195,30),(-195,52),(-190,61),(-180,65),(-135,65),
 (-75,65),(-20,65),(42,65),(56,61),(60,54),(60,31),(48,28),(-170,28)]
GRIP_OUTLINE = [(-24, 5), (10, 3), (15, -22), (23, -51), (31, -83), (25, -90), (7, -88), (-4, -81), (-12, -52), (-20, -24)]
def make_design():
    from common import Design
    d=Design(255)
    d.node('slide_recoil',(0,43,-55)); d.node('limit_recoil',(0,43,-40))
    d.outline('frame',BODY_OUTLINE,36)
    d.outline('barrel_shell',SLIDE_OUTLINE,40,parent='slide_recoil',bevel=3)
    for side in (-1,1): d.outline('hand_grip' if side==1 else 'grip_panel_left',GRIP_OUTLINE,8,'dark',x=side*18)
    d.ring('trigger_guard',[(-105,12),(-39,12),(-29,2),(-23,-35),(-34,-47),(-92,-47),(-108,-32),(-112,-1)],
           [(-98,4),(-47,4),(-39,-1),(-33,-29),(-39,-37),(-87,-37),(-98,-26),(-102,-2)],20)
    d.markers((0,46,-195),(0,67,-165),(0,67,39),(0,12,-43))
    d.functional_sights((0,67,-165),(0,67,39),'slide_recoil')
    d.outline('trigger',[(-48,16),(-39,16),(-39,5),(-45,-7),(-50,-22),(-48,-28),(-55,-25),(-56,-18),(-51,-4),(-47,5)],12,'accent','pivot_trigger',bevel=1)
    d.cylinder('muzzle_lens',(0,46,-193),10,4,'glass','slide_recoil')
    d.cylinder('cable_boss',(0,-97,23),10,14,'accent',axis='y')
    # Proud seated ribs, a dark longitudinal seam and a flat crown distinguish
    # the moving slide. Every rib crosses the slide surface by half a millimetre.
    for side in (-1,1):
        d.box('slide_seam_'+str(side),(side*20,31,-50),(2,2,204),'dark','slide_recoil',.4)
        for i in range(6): d.box('serration_'+str(side)+'_'+str(i),(side*20,47,4+i*7),(2,24,2),'accent','slide_recoil',.4)
    return d

if __name__ == '__main__':
    sys.path.insert(0,str(Path(__file__).resolve().parent))
    from common import run
    run('arc-pistol-slide',PARAMETERS)
