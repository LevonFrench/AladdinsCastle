"""Original service-pistol prop; no maker details, tracing or imported mesh."""
from pathlib import Path
import sys
PARAMETERS=dict(length_mm=220,slide_width_mm=32,grip_angle_deg=15)

def make_design():
    from common import Design
    from game_parts import grip,controls,sight_bead
    d=Design(220)
    d.node('slide_recoil',(0,42,-74));d.node('limit_recoil',(0,42,-59))
    d.outline('receiver',[(-171,12),(-168,27),(-133,29),(-74,29),(-29,24),(18,22),(37,15),(31,2),(8,-8),(-16,-5),(-37,6)],30,bevel=1.5)
    d.outline('barrel_shell',[(-180,31),(-180,50),(-172,60),(29,60),(40,52),(40,32),(32,28),(-166,28)],32,parent='slide_recoil',bevel=1.5)
    grip(d);controls(d)
    d.markers((0,43,-181),(0,63,-147),(0,63,23),(0,8,-43))
    d.functional_sights((0,63,-147),(0,63,23),'slide_recoil')
    sight_bead(d,63,-147,'slide_recoil')
    d.cylinder('bore_shadow',(0,43,-180),6,1.5,'dark','slide_recoil')
    d.box('extractor_seam',(16,45,-64),(1.5,11,24),'dark','slide_recoil',.2)
    d.box('lower_rail',(0,6,-121),(24,8,58),'dark',bevel=1)
    for side in (-1,1):
        for i in range(5):d.box('rear_cut_'+str(side)+'_'+str(i),(side*16,46,3+i*5),(1.2,18,1.6),'dark','slide_recoil',.2)
    return d

if __name__=='__main__':
    sys.path.insert(0,str(Path(__file__).resolve().parent))
    from common import run
    run('game-pistol-service',PARAMETERS)
