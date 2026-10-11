"""Original mnt-mg-heavy; GPL-3.0-only source, CC0 generated asset.

Authored dimensions, no imported mesh, tracing, logos or markings.
"""
from pathlib import Path
import sys
PARAMETERS = dict(length_mm=650,barrel_width_mm=140,grip_angle_deg=0)

BODY_OUTLINE = [(-300,64),(-298,83),(-294,104),(-290,128),(-283,148),(-271,162),
 (-250,170),(-225,173),(-200,174),(-175,174),(-150,174),(-125,174),(-100,174),
 (-75,174),(-50,172),(-25,170),(0,170),(27,167),(48,156),(62,139),
 (65,115),(63,91),(55,71),(43,59),(22,53),(0,51),(-25,50),(-50,50),
 (-80,51),(-110,52),(-140,53),(-170,54),(-200,55),(-230,56),(-266,58),(-289,60)]
HANDLE_OUTLINE = [(82,137),(98,143),(112,137),(120,126),(120,33),(115,22),
 (99,19),(88,27),(84,51),(82,81),(80,110)]
def make_design():
    from common import Design
    # Controller position is the right rear handle web, aim follows model -Z.
    d=Design(650,origin=(130,112,102))
    d.node('pivot_yaw',(0,-60,-200)); d.node('pivot_pitch',(0,60,-200),'pivot_yaw')
    d.node('grip_two',(-130,112,102))
    d.outline('receiver',BODY_OUTLINE,140,parent='pivot_pitch',bevel=4)
    d.tube('barrel_shroud',(0,110,-405),60,50,250,'pivot_pitch')
    d.cylinder('muzzle_lens',(0,110,-527),51,6,'glass','pivot_pitch')
    d.box('yoke_base',(0,-70,-200),(240,35,125),'dark',bevel=4)
    for side in (-1,1):
        d.box('yoke_arm_'+str(side),(side*100,-5,-200),(28,145,85),'dark','pivot_yaw',4)
        d.outline('rear_handle_'+str(side),HANDLE_OUTLINE,44,'dark','pivot_pitch',x=side*130,bevel=4)
    d.cylinder('yoke_axle',(0,60,-200),24,235,'accent','pivot_pitch',axis='x')
    d.box('handle_bridge',(0,127,76),(310,40,55),'body','pivot_pitch',4)
    d.box('thumb_console',(0,139,98),(80,24,35),'dark','pivot_pitch',3)
    d.markers((0,110,-530),(0,175,-213),(0,172,4),(130,116,102),'pivot_pitch')
    d.sights((0,175,-213),(0,172,4),'pivot_pitch')
    d.outline('trigger',[(99,117),(108,117),(109,104),(105,92),(96,73),(96,66),
                        (88,69),(87,79),(94,95),(99,108)],14,'accent','pivot_trigger',x=130,bevel=1)
    d.node('button_special',(-40,140,99),'pivot_pitch')
    d.cylinder('button_special_cap',(-40,140,99),11,4,'accent','button_special',axis='x')
    return d

if __name__ == '__main__':
    sys.path.insert(0,str(Path(__file__).resolve().parent))
    from common import run
    run('mnt-mg-heavy',PARAMETERS)
