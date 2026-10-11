"""Original arc-pistol-twin; GPL-3.0-only source, CC0 generated asset.

Authored dimensions, no imported mesh, tracing, logos or markings.
"""
from pathlib import Path
import sys
PARAMETERS = dict(length_mm=245,barrel_width_mm=48,grip_angle_deg=15)

BODY_OUTLINE = [(-180,16),(-179,30),(-170,39),(-151,43),(-125,44),(-100,46),
 (-75,49),(-52,49),(-29,47),(-9,43),(12,36),(24,26),(28,14),(25,4),
 (10,-6),(16,-25),(23,-51),(31,-78),(30,-87),(23,-94),(10,-94),(-1,-89),
 (-11,-84),(-16,-72),(-22,-51),(-27,-28),(-34,-10),(-43,4),(-64,8),
 (-87,8),(-111,9),(-139,11),(-168,12)]
BARREL_OUTLINE = [(-180,29),(-180,65),(-176,77),(-165,84),(-147,86),
 (-63,86),(-38,83),(-14,77),(9,74),(45,74),(61,66),(65,52),
 (62,30),(42,24),(-15,25),(-100,27)]
GRIP_OUTLINE = [(-26, 5), (7, 3), (13, -25), (21, -53), (28, -79), (22, -86), (4, -84), (-7, -76), (-14, -49), (-21, -24)]
def make_design():
    from common import Design
    d=Design(245); d.node('visual_kick',(0,0,0))
    d.outline('frame',BODY_OUTLINE,40,parent='visual_kick')
    d.outline('barrel_shell',BARREL_OUTLINE,48,parent='visual_kick',bevel=4)
    for side in (-1,1): d.outline('hand_grip' if side==1 else 'grip_panel_left',GRIP_OUTLINE,12,'dark','visual_kick',x=side*20)
    d.cylinder('fixed_cylinder',(0,45,-43),30,64,'accent','visual_kick')
    d.ring('trigger_guard',[(-122,13),(-36,13),(-25,1),(-20,-37),(-33,-49),(-102,-49),(-121,-36),(-130,-7)],
           [(-113,3),(-46,3),(-37,-4),(-32,-29),(-41,-39),(-99,-39),(-111,-29),(-119,-7)],24,parent='visual_kick')
    d.markers((0,54,-180),(0,88,-144),(0,76,36),(0,14,-43),'visual_kick')
    d.sights((0,88,-144),(0,76,36),'visual_kick')
    d.outline('trigger',[(-47,18),(-38,18),(-37,7),(-43,-6),(-52,-25),(-50,-30),(-58,-26),(-58,-17),(-51,-2),(-47,8)],12,'accent','pivot_trigger',bevel=1)
    d.cylinder('muzzle_lens',(0,54,-178),12,4,'glass','visual_kick')
    d.cylinder('cable_boss',(0,-94,20),10,14,'accent','visual_kick',axis='y')
    return d

if __name__ == '__main__':
    sys.path.insert(0,str(Path(__file__).resolve().parent))
    from common import run
    run('arc-pistol-twin',PARAMETERS)
