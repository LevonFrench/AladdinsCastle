"""Original con-pistol-dpad; GPL-3.0-only source, CC0 generated asset.

Authored dimensions, no imported mesh, tracing, logos or markings.
"""
from pathlib import Path
import sys
PARAMETERS = dict(length_mm=225,barrel_width_mm=38,grip_angle_deg=15)

BODY_OUTLINE = [(-175,16),(-174,28),(-166,37),(-151,42),(-129,45),(-108,46),
 (-87,47),(-66,48),(-47,49),(-26,50),(-8,52),(7,53),(21,50),(34,42),
 (41,30),(42,15),(34,5),(12,-5),(17,-24),(24,-52),(31,-82),(30,-91),
 (23,-97),(12,-97),(0,-92),(-10,-84),(-16,-66),(-22,-43),(-28,-21),
 (-37,-6),(-49,5),(-72,10),(-96,12),(-123,12),(-152,13),(-168,14)]
BARREL_OUTLINE = [(-175,29),(-175,49),(-171,57),(-161,63),(-141,65),
 (-87,66),(-41,67),(-21,71),(-9,77),(6,80),(21,77),(36,68),(47,54),
 (50,38),(50,29),(31,25),(-59,26)]
GRIP_OUTLINE = [(-25, 5), (9, 3), (15, -29), (23, -58), (28, -84), (21, -90), (4, -87), (-7, -78), (-14, -52), (-21, -28)]
def make_design():
    from common import Design
    d=Design(225)
    d.outline('frame',BODY_OUTLINE,34)
    d.outline('barrel_shell',BARREL_OUTLINE,38,bevel=4)
    for side in (-1,1): d.outline('hand_grip' if side==1 else 'grip_panel_left',GRIP_OUTLINE,8,'dark',x=side*17)
    d.ring('trigger_guard',[(-97,13),(-37,13),(-26,1),(-23,-33),(-32,-43),(-81,-43),(-98,-31),(-105,-4)],
           [(-89,5),(-46,5),(-37,-4),(-33,-27),(-39,-34),(-78,-34),(-88,-25),(-95,-4)],20)
    d.markers((0,45,-175),(0,67,-142),(0,81,6),(0,14,-41))
    d.sights((0,67,-142),(0,81,6))
    d.outline('trigger',[(-45,17),(-36,17),(-37,5),(-42,-5),(-48,-21),(-47,-27),(-54,-24),(-55,-16),(-49,-1),(-45,7)],11,'accent','pivot_trigger',bevel=1)
    d.cylinder('muzzle_lens',(0,45,-173),11,4,'glass')
    for bid,side in (('a',-1),('b',1)):
        d.node('button_'+bid,(side*17,19,-67))
        d.cylinder('button_'+bid+'_cap',(side*17,19,-67),7,4,'accent','button_'+bid,axis='x')
    d.node('button_c',(0,-97,22)); d.cylinder('button_c_cap',(0,-97,22),7,4,'accent','button_c',axis='y')
    for bid,z in (('select',-107),('start',-130)):
        d.node('button_'+bid,(-19,43,z))
        d.cylinder('button_'+bid+'_cap',(-19,43,z),4,4,'accent','button_'+bid,axis='x')
    d.node('button_dpad',(0,40,50))
    d.box('button_dpad_vertical',(0,40,50),(6,24,4),'accent','button_dpad',1)
    d.box('button_dpad_horizontal',(0,40,50),(24,6,4),'accent','button_dpad',1)
    return d

if __name__ == '__main__':
    sys.path.insert(0,str(Path(__file__).resolve().parent))
    from common import run
    run('con-pistol-dpad',PARAMETERS)
