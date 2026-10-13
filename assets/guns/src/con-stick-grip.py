"""Original separate one-hand analog controller, no cable or muzzle. GPL-3.0-only."""
from pathlib import Path
import sys
PARAMETERS=dict(length_mm=120,width_mm=44,depth_mm=56)

def make_design():
    from common import Design
    d=Design(PARAMETERS['length_mm'])
    d.loft('hand_grip',[(-76,9,10,10),(-68,15,15,9),(-43,18,20,7),(-14,21,24,3),(14,22,28,0),(25,19,25,0),(28,12,16,0)],'body')
    d.box('front_inlay',(0,5,-26),(27,30,5),'accent',bevel=4)
    d.cylinder('stick_socket',(0,27,-7),14,5,'dark',axis='y')
    d.node('pivot_stick_x',(0,29,-7));d.node('pivot_stick_y',(0,29,-7),'pivot_stick_x')
    d.node('button_stick',(0,29,-7),'pivot_stick_y')
    d.cylinder('stick_stem',(0,35,-7),4,12,'accent','button_stick',axis='y')
    d.cylinder('stick_cap',(0,42,-7),11,5,'dark','button_stick',axis='y')
    for bid,y,z,r in (('c',17,-31,5.5),('z',0,-32,7)):
        d.node('button_'+bid,(0,y,z))
        d.cylinder('button_'+bid+'_cap',(0,y,z),r,5 if bid=='c' else 9,'dark','button_'+bid)
    d.box('rear_inset',(0,-28,28),(10,17,2),'glass',bevel=1)
    return d

if __name__=='__main__':
    sys.path.insert(0,str(Path(__file__).resolve().parent))
    from common import run
    run('con-stick-grip',PARAMETERS)
