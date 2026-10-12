"""Original fictional energy gun with a protected emitter and underslung guard."""
from pathlib import Path
import sys
PARAMETERS=dict(length_mm=295,emitter_width_mm=44,grip_angle_deg=15)

def make_design():
    from common import Design
    from game_parts import grip,controls
    d=Design(295)
    d.node('slide_recoil',(0,45,-148));d.node('limit_recoil',(0,45,-133))
    d.outline('receiver',[(-198,9),(-204,37),(-181,66),(-90,62),(-46,50),(30,50),(57,35),(51,5),(8,-7),(-27,-4),(-42,9)],39,bevel=2)
    grip(d);controls(d)
    d.outline('barrel_shell',[(-238,32),(-238,59),(-212,70),(-136,63),(-91,49),(-102,28),(-213,26)],44,parent='slide_recoil',bevel=2)
    d.outline('emitter_guard',[(-237,7),(-169,8),(-102,-16),(-85,-35),(-105,-44),(-164,-18),(-234,-11)],23,'dark',bevel=2)
    d.box('emitter_face',(0,44,-239),(30,20,2),'dark','slide_recoil',.5)
    d.cylinder('emitter_lens',(0,44,-240.5),8,1.5,'glass','slide_recoil')
    d.box('power_cell',(0,33,-52),(28,32,48),'dark',bevel=2)
    for side in (-1,1):
        for i in range(4):d.box('receiver_slot_'+str(side)+'_'+str(i),(side*20,38,-78+i*8),(1.5,15,2),'dark',bevel=.3)
    d.box('front_sight_seat',(0,71.5,-210),(10,3,12),bevel=.3)
    d.box('rear_sight_seat',(0,61.5,23),(20,23,13),bevel=.5)
    d.markers((0,44,-242),(0,76,-210),(0,76,23),(0,8,-43))
    d.functional_sights((0,76,-210),(0,76,23))
    return d

if __name__=='__main__':
    sys.path.insert(0,str(Path(__file__).resolve().parent))
    from common import run
    run('game-energy-gun',PARAMETERS)
