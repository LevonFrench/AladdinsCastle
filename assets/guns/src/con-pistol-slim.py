"""Original con-pistol-slim; GPL-3.0-only source, CC0 generated asset.

Authored dimensions, no imported mesh, tracing, logos or markings.
"""
from pathlib import Path
import sys
PARAMETERS = dict(length_mm=240,barrel_width_mm=30,grip_angle_deg=15)

BODY_OUTLINE = [(-185,14),(-184,21),(-177,25),(-156,26),(-135,27),(-114,27),
 (-93,27),(-72,27),(-51,27),(-30,26),(-9,25),(9,22),(18,16),(22,8),
 (18,1),(7,-7),(14,-31),(21,-56),(28,-83),(27,-91),(20,-95),(10,-95),
 (-2,-91),(-10,-84),(-14,-72),(-20,-48),(-26,-24),(-34,-7),(-44,4),
 (-65,8),(-88,10),(-115,11),(-143,12),(-172,12)]
BARREL_OUTLINE = [(-190,25),(-190,46),(-186,51),(-171,53),(-126,53),
 (-69,53),(-19,53),(33,52),(46,47),(50,38),(47,25),(-95,24)]
GRIP_OUTLINE = [(-23, 5), (5, 3), (12, -32), (20, -59), (24, -83), (18, -89), (2, -85), (-6, -77), (-13, -51), (-19, -28)]
def make_design():
    from common import Design
    d=Design(240)
    d.outline('frame',BODY_OUTLINE,26)
    d.outline('barrel_shell',BARREL_OUTLINE,30,bevel=2)
    for side in (-1,1): d.outline('hand_grip' if side==1 else 'grip_panel_left',GRIP_OUTLINE,7,'dark',x=side*13,bevel=2)
    d.ring('trigger_guard',[(-91,12),(-38,12),(-28,1),(-23,-31),(-32,-41),(-76,-41),(-93,-28),(-99,-2)],
           [(-84,4),(-45,4),(-37,-3),(-33,-26),(-39,-32),(-73,-32),(-83,-23),(-89,-2)],16)
    d.markers((0,38,-190),(0,60,-166),(0,60,27),(0,12,-42))
    d.outline('sight_rail_geometry',[(-176,52),(-171,58),(27,58),(37,52)],16,'accent',bevel=2)
    d.sights((0,60,-166),(0,60,27))
    d.outline('trigger',[(-46,15),(-38,15),(-38,4),(-43,-6),(-47,-19),(-46,-24),(-52,-22),(-53,-15),(-48,-1),(-44,7)],9,'accent','pivot_trigger',bevel=1)
    d.cylinder('muzzle_lens',(0,38,-188),11,4,'glass')
    d.cylinder('cable_boss',(0,-95,19),8,12,'accent',axis='y')
    oval=[(-128,9),(-124,5),(-112,5),(-108,9),(-108,15),(-112,19),(-124,19),(-128,15)]
    for bid,side in (('a',-1),('b',1)):
        d.node('button_'+bid,(side*13,12,-118))
        d.outline('button_'+bid+'_cap',oval,4,'accent','button_'+bid,x=side*13,bevel=1)
    return d

if __name__ == '__main__':
    sys.path.insert(0,str(Path(__file__).resolve().parent))
    from common import run
    run('con-pistol-slim',PARAMETERS)
