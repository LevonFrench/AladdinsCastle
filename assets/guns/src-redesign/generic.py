"""Fresh short-frame compact pistol; original mesh and outline data."""
from fresh import *

def build(model_id,detail=0):
    # A curved undercut beavertail and short dust cover, deliberately compact.
    profile('compact_alloy_frame',[(-32,28),(-24,34),(-4,35),(25,31),(134,27),(143,18),(141,6),(72,2),(40,1),(17,-2),(11,-16),(20,-33),(16,-65),(12,-75),(-5,-84),(-31,-83),(-39,-74),(-29,-47),(-17,-14),(-19,3),(-34,10)],30,bevel=2.2)
    node('slide_recoil',(0,33,0))
    profile('sculpted_slide',[(-39,31),(-39,51),(-31,59),(124,59),(143,53),(149,46),(148,32),(137,29),(-31,29)],32,'body',2.3,parent='slide_recoil')
    profile('slide_crown',[(-34,54),(-25,62),(125,62),(143,54)],21,'accent',1.4,parent='slide_recoil')
    for side in (-1,1):
        profile('grip_panel',[(-10,-9),(10,-14),(20,-33),(8,-68),(-7,-75),(-28,-71),(-20,-46),(-9,-22)],5,'dark',2.4,lateral=side*14.5)
        profile('grip_inset',[(-6,-22),(6,-25),(11,-36),(2,-59),(-10,-65),(-20,-63),(-13,-42)],1.4,'dark',1,lateral=side*17.2)
        if not detail:
            # v2 fix 6: four slide cuts a side (was seven); fasteners and the
            # 0.35 mm checkering rods removed.
            for i in range(4):
                profile('rear_slide_cut',[(-30+i*7,35),(-29+i*7,51),(-27+i*7,51),(-28+i*7,35)],.6,'dark',.2,lateral=side*16.1,parent='slide_recoil')
        profile('slide_release',[(-6,14),(21,14),(25,17),(21,20),(-5,20)],3,'accent',.6,lateral=side*16)
    ring('curved_guard',smooth_loop([(17,3),(50,2),(66,-9),(64,-27),(49,-42),(21,-42),(11,-32),(10,-12)]),smooth_loop([(21,-5),(47,-5),(57,-12),(56,-24),(46,-33),(25,-34),(20,-28),(18,-14)]),13,'body',1.7)  # v2: rounded guard
    node('pivot_trigger',(24,0,0))
    profile('swept_trigger',[(21,1),(28,0),(27,-13),(23,-23),(27,-28),(24,-31),(17,-27),(17,-21),(21,-12)],7,'accent',.8,parent='pivot_trigger')
    tube('recessed_muzzle',(148,43,0),9.5,6.5,5,'accent',vertices=32)
    cyl('optical_lens',(149.3,43,0),6.3,1,'glass',vertices=24)
    tube('lower_socket',(140,17,0),5,3.2,6,'dark')
    # v2 fix 3/4: level notch-blade sights on the slide crown (which crests at 62).
    sights=functional_sights((131,59.3),(-26,61),64.5,'slide_recoil','slide_recoil')
    profile('magazine_floor',[(-39,-77),(-9,-86),(11,-78),(13,-84),(-11,-92),(-41,-84)],33,'accent',1.5)
    # v2 fix 1: 191 mm against the 230 mm toml. The slide and frame forward of
    # the trigger (20 mm) are lengthened 1.19x; the grip is not enlarged.
    return {'_deform':{'stretch':(20,1.19)},'muzzle':(151,43,0),**sights,'pivot_trigger':(24,0,0),'limit_recoil':(-15,33,0),'length_mm':195}
