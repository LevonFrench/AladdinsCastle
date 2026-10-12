"""Original stock-and-rail crossbow, flexible-looking limbs and separate string."""
from pathlib import Path
import sys
PARAMETERS=dict(length_mm=580,span_mm=520,grip_angle_deg=15)

def make_design():
    from common import Design
    from game_parts import grip,controls,sweep,sight_bead
    d=Design(580)
    d.node('string_draw',(0,48,-95));d.node('bolt_release',(0,52,-225))
    d.outline('receiver',[(-385,21),(-385,43),(-345,51),(-123,51),(-48,42),(29,42),(64,26),(190,8),(190,-34),(167,-43),(56,-4),(17,-10),(-28,-4),(-50,15)],34,bevel=2)
    d.box('bolt_rail',(0,48,-222),(17,7,324),'dark',bevel=.6)
    d.box('sight_rail',(0,56,-154),(14,18,372),bevel=1)
    d.box('rear_rail_seat',(0,46.5,26),(18,19,15),bevel=.8)
    d.box('bolt',(0,55,-237),(3,3,276),'body','bolt_release',.2)
    d.box('butt_pad',(0,-12,184),(43,45,12),'dark',bevel=2)
    grip(d);grip(d,-185,fore=True);controls(d)
    d.box('limb_mount',(0,29,-282),(64,30,34),bevel=2)
    points=[(-260,36,-195),(-225,40,-230),(-163,42,-271),(-79,40,-289),(0,35,-295),(79,40,-289),(163,42,-271),(225,40,-230),(260,36,-195)]
    sweep(d,'bow_limbs',points,[12,15,17,19,20,19,17,15,12],flatten=.3)
    sweep(d,'bow_string',[(-260,36,-195),(-130,43,-145),(0,48,-95),(130,43,-145),(260,36,-195)],[1.4]*5,'dark','string_draw')
    d.markers((0,55,-389),(0,68,-334),(0,68,26),(0,8,-43))
    d.functional_sights((0,68,-334),(0,68,26))
    sight_bead(d,68,-334)
    return d

if __name__=='__main__':
    sys.path.insert(0,str(Path(__file__).resolve().parent))
    from common import run
    run('game-crossbow',PARAMETERS)
