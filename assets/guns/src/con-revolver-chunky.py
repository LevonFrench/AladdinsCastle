"""Original con-revolver-chunky; GPL-3.0-only source, CC0 generated asset.

Authored dimensions, no imported mesh, tracing, logos or markings.
"""
from pathlib import Path
import sys
PARAMETERS = dict(length_mm=230,barrel_width_mm=54,grip_angle_deg=15)

BODY_OUTLINE = [(-170,16),(-169,31),(-163,40),(-151,44),(-130,46),(-110,49),
 (-92,52),(-72,55),(-54,55),(-35,53),(-18,49),(1,44),(18,35),(27,23),
 (26,10),(15,1),(5,-7),(11,-28),(18,-53),(27,-81),(27,-89),(20,-94),
 (9,-94),(-3,-89),(-13,-80),(-17,-64),(-23,-40),(-30,-19),(-39,-5),
 (-50,5),(-72,9),(-94,10),(-115,11),(-141,12),(-162,13)]
BARREL_OUTLINE = [(-170,31),(-170,68),(-165,75),(-151,78),(-104,78),
 (-94,69),(-91,56),(-75,56),(-38,64),(5,65),(45,65),(56,59),(60,45),
 (57,31),(38,27),(-103,28)]
GRIP_OUTLINE = [(-26, 5), (1, 3), (9, -32), (17, -58), (23, -82), (17, -88), (1, -83), (-9, -73), (-15, -47), (-21, -26)]
def make_design():
    from common import Design
    d=Design(230)
    d.outline('frame',BODY_OUTLINE,46,bevel=4)
    d.outline('barrel_shell',BARREL_OUTLINE,54,bevel=4)
    for side in (-1,1): d.outline('hand_grip' if side==1 else 'grip_panel_left',GRIP_OUTLINE,14,'dark',x=side*23,bevel=4)
    d.cylinder('fixed_cylinder',(0,48,-47),33,70,'accent')
    d.ring('trigger_guard',[(-129,15),(-36,15),(-25,1),(-21,-39),(-35,-55),(-107,-55),(-129,-38),(-138,-6)],
           [(-117,3),(-48,3),(-38,-5),(-34,-33),(-44,-43),(-101,-43),(-116,-31),(-125,-6)],28)
    d.markers((0,54,-170),(0,80,-133),(0,67,23),(0,15,-42))
    d.sights((0,80,-133),(0,67,23))
    d.outline('trigger',[(-47,19),(-37,19),(-37,6),(-44,-9),(-54,-29),(-52,-35),(-61,-31),(-62,-20),(-53,-3),(-48,8)],13,'accent','pivot_trigger',bevel=1)
    d.cylinder('muzzle_lens',(0,54,-168),14,4,'glass')
    d.node('button_start',(-34,47,-47))
    d.cylinder('button_start_cap',(-34,47,-47),9,4,'accent','button_start',axis='x')
    return d

if __name__ == '__main__':
    sys.path.insert(0,str(Path(__file__).resolve().parent))
    from common import run
    run('con-revolver-chunky',PARAMETERS)
