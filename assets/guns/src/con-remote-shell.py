"""Original remote shell; GPL-3.0-only script, CC0 generated asset.

Private pictures informed proportion and split hand position only. All contours
and dimensions below are authored parameters, without tracing or imported art.
"""
from pathlib import Path
import sys
PARAMETERS=dict(length_mm=300,barrel_width_mm=46,remote_width_mm=38,grip_angle_deg=15)
BARREL_OUTLINE=[(-110,32),(-110,48),(-103,53),(49,53),(61,47),(67,32)]
REMOTE_OUTLINE=[(-107,49),(-107,73),(-101,77),(46,77),(51,73),(51,49)]
GRIP_OUTLINE=[(-19,34),(17,33),(19,5),(23,-20),(31,-67),(25,-80),(6,-81),(-7,-72),(-15,-33)]
REAR_OUTER=[(42,39),(71,39),(190,-47),(183,-66),(171,-74),(45,-58),(33,-30)]
REAR_INNER=[(60,23),(73,23),(171,-43),(168,-49),(158,-53),(61,-42),(51,-23)]

def make_design():
    from common import Design
    d=Design(PARAMETERS['length_mm'])
    d.outline('barrel_shell',BARREL_OUTLINE,46,bevel=2.5)
    d.outline('hand_grip',GRIP_OUTLINE,34,bevel=4)
    d.ring('rear_handle',REAR_OUTER,REAR_INNER,32)
    # The remote-shaped bar is retained by low rails, with its complete top
    # face and buttons visible. No integrated stick or cable is drawn.
    d.outline('remote_bar',REMOTE_OUTLINE,38,'body',bevel=2.5)
    for x in (-22,22): d.box('channel_rail_'+str(x),(x,53,-29),(5,10,155),'accent',bevel=1)
    d.ring('trigger_guard',[(-42,30),(-10,30),(1,15),(0,-24),(-32,-28),(-46,-14)],
           [(-34,22),(-17,22),(-8,12),(-8,-15),(-27,-19),(-37,-10)],18)
    d.markers((0,63,-111),(0,80,-96),(0,80,43),(0,20,-22))
    d.functional_sights((0,80,-96),(0,80,43))
    d.node('grip_two',(0,-29,140))
    d.outline('trigger',[(-26,24),(-16,24),(-17,8),(-24,-12),(-29,-17),(-34,-13),(-30,2)],10,'accent','pivot_trigger',bevel=.8)
    d.box('sensor_window',(0,62,-108),(24,14,3),'glass',bevel=1)
    d.box('grip_inlay',(0,-39,18),(27,49,8),'accent',bevel=3)
    for bid,x,z,r in (('a',0,-37,7),('minus',-11,-15,3.2),('plus',11,-15,3.2),('home',0,0,3.7),('one',0,18,4),('two',0,34,4)):
        d.node('button_'+bid,(x,78,z))
        d.cylinder('button_'+bid+'_cap',(x,78,z),r,4,'accent','button_'+bid,axis='y')
    d.node('button_dpad',(0,78,-70))
    d.box('button_dpad_vertical',(0,78,-70),(5,4,21),'dark','button_dpad',.5)
    d.box('button_dpad_horizontal',(0,78,-70),(21,4,5),'dark','button_dpad',.5)
    return d

if __name__=='__main__':
    sys.path.insert(0,str(Path(__file__).resolve().parent))
    from common import run
    run('con-remote-shell',PARAMETERS)
