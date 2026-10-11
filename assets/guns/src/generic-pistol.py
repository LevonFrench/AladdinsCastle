"""Original generic-pistol; GPL-3.0-only source, CC0 generated asset.

Authored dimensions, no imported mesh, tracing, logos or markings.
"""
from pathlib import Path
import sys
PARAMETERS = dict(length_mm=230,barrel_width_mm=36,grip_angle_deg=15)

# Plain compact frame: (longitudinal z, vertical y), mm; closed by extrusion.
BODY_OUTLINE = [(-170,18),(-170,28),(-160,34),(-130,34),(-110,34),(-85,34),
 (-60,34),(-35,34),(-15,32),(5,28),(14,20),(17,10),(16,2),(10,-6),
 (18,-30),(25,-55),(33,-82),(33,-90),(27,-95),(18,-95),(5,-91),(-7,-87),
 (-12,-78),(-18,-55),(-24,-30),(-30,-10),(-38,4),(-60,10),(-90,12),
 (-115,13),(-145,14),(-168,16)]
BARREL_OUTLINE = [(-195,30),(-195,49),(-171,54),(-150,55),(-45,55),
 (-12,53),(12,48),(18,42),(18,31),(-25,29),(-120,29)]
GRIP_OUTLINE = [(-23, 5), (5, 3), (13, -28), (20, -53), (29, -83), (23, -89), (5, -86), (-5, -79), (-11, -55), (-17, -30)]
def make_design():
    from common import Design
    d=Design(230)
    d.outline('frame',BODY_OUTLINE,34)
    d.outline('barrel_shell',BARREL_OUTLINE,36)
    for side in (-1,1): d.outline('hand_grip' if side==1 else 'grip_panel_left',GRIP_OUTLINE,7,'dark',x=side*17,bevel=3)
    d.ring('trigger_guard',[(-94,15),(-36,15),(-27,6),(-23,-34),(-30,-43),(-86,-43),(-98,-32),(-100,4)],
           [(-88,7),(-43,7),(-36,2),(-32,-28),(-37,-34),(-80,-34),(-89,-26),(-91,1)],18)
    d.markers((0,41,-195),(0,57,-148),(0,54,-5),(0,12,-40))
    d.sights((0,57,-148),(0,54,-5))
    d.outline('trigger',[(-43,14),(-35,14),(-36,3),(-42,-7),(-47,-18),(-47,-24),(-53,-22),(-54,-16),(-49,-3),(-44,6)],10,'accent','pivot_trigger',bevel=1)
    d.cylinder('muzzle_lens',(0,41,-193),9,4,'glass')
    return d

if __name__ == '__main__':
    sys.path.insert(0,str(Path(__file__).resolve().parent))
    from common import run
    run('generic-pistol',PARAMETERS)
