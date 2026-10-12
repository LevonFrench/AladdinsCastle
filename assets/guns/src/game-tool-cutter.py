"""Original industrial tool-gun: asymmetric service panels, broad working head."""
from pathlib import Path
import sys
PARAMETERS=dict(length_mm=260,tool_head_width_mm=86,grip_angle_deg=15)

def make_design():
    from common import Design
    from game_parts import grip,controls
    d=Design(260)
    d.node('slide_recoil',(0,40,-156));d.node('limit_recoil',(0,40,-141))
    d.outline('receiver',[(-169,9),(-177,33),(-164,57),(-117,63),(-65,56),(31,48),(65,34),(59,9),(10,-9),(-26,-5),(-40,6)],43,bevel=2)
    grip(d);controls(d)
    d.box('working_head',(0,39,-181),(86,48,28),'body','slide_recoil',2)
    d.box('head_separator',(0,39,-196),(70,18,1.5),'dark','slide_recoil',.2)
    for i,x in enumerate((-27,0,27)):
        d.cylinder('working_socket_'+str(i),(x,39,-197),7,2,'glass','slide_recoil')
    d.box('battery_panel',(0,10,35),(32,15,42),'dark',bevel=1.5)
    d.box('tool_spine',(0,59,-87),(17,10,122),bevel=1)
    for side in (-1,1):
        d.box('access_panel_'+str(side),(side*22,31,-97),(2,25,64),'dark',bevel=1)
        for i in range(4):d.box('panel_rib_'+str(side)+'_'+str(i),(side*23,31,-113+i*10),(1.5,21,2),'body',bevel=.4)
    d.box('front_sight_seat',(0,61,-156),(10,8,12),bevel=.5)
    d.box('rear_sight_seat',(0,57,22),(20,16,12),bevel=.5)
    d.markers((0,39,-199),(0,68,-156),(0,68,22),(0,8,-43))
    d.functional_sights((0,68,-156),(0,68,22))
    return d

if __name__=='__main__':
    sys.path.insert(0,str(Path(__file__).resolve().parent))
    from common import run
    run('game-tool-cutter',PARAMETERS)
