"""Original heavy-revolver prop with a long underlug and exposed cylinder."""
from pathlib import Path
import sys
PARAMETERS=dict(length_mm=285,cylinder_radius_mm=24,grip_angle_deg=15)

def make_design():
    from common import Design
    from game_parts import grip,controls,chamber_ring,sight_bead
    d=Design(285)
    d.node('cylinder_spin',(0,39,-64))
    # An open cylinder window, with narrower top/lower straps than the drum.
    d.outline('rear_frame',[(-36,12),(-36,72),(-19,72),(25,54),(35,27),(28,5),(3,-10),(-19,-6)],32,bevel=2)
    d.box('front_frame',(0,39,-119),(28,57,24),bevel=2)
    d.box('top_strap',(0,69,-71),(27,9,90),bevel=1)
    d.box('lower_strap',(0,13,-70),(24,8,86),bevel=1)
    d.box('barrel_shell',(0,43,-188),(34,35,124),bevel=3)
    d.box('barrel_underlug',(0,18,-188),(29,20,126),'dark',bevel=2)
    d.cylinder('cylinder',(0,39,-64),24,58,'body','cylinder_spin')
    chamber_ring(d,'cylinder_spin',39,-94)
    d.cylinder('cylinder_back',(0,39,-35),20,3,'dark','cylinder_spin')
    grip(d);controls(d)
    d.markers((0,44,-252),(0,76.5,-224),(0,76.5,-13),(0,8,-43))
    d.functional_sights((0,76.5,-224),(0,76.5,-13))
    sight_bead(d,76.5,-224)
    d.box('rear_sight_seat',(0,67,-13),(18,13,12),bevel=.4)
    d.box('top_rib',(0,70,-139),(12,7,199),bevel=.7)
    d.box('front_rib_seat',(0,63.5,-224),(12,8,13),bevel=.5)
    d.cylinder('bore_shadow',(0,44,-250.5),7,1.5,'dark')
    d.box('hammer',(0,63,21),(9,13,17),'dark',bevel=1)
    return d

if __name__=='__main__':
    sys.path.insert(0,str(Path(__file__).resolve().parent))
    from common import run
    run('game-revolver-heavy',PARAMETERS)
