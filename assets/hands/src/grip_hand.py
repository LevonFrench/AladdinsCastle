"""Original scripted grip hands; GPL-3.0-only, generated geometry CC0.

SI, +Y up, -Z gun direction; web-of-hand origin. Rigid finger volumes are
parented to semantic pivots, not a skin/IK dependency. No scan or traced anatomy.
"""
from pathlib import Path
import sys
import tomllib
ROOT=Path(__file__).resolve().parents[3]
sys.path.insert(0,str(ROOT/'assets/guns/src'))
from common import Design,blender_run
PARAMETERS=dict(length_mm=110)

FINGERS={
 'index':([(29,-4,-10),(28,2,-27),(20,3,-41),(5,3,-46)],[8,7.5,7,4.5]),
 'middle':([(29,-25,-10),(21,-25,-28),(0,-25,-30),(-17,-25,-19),(-20,-25,-3)],[9,8,7.8,7,4.5]),
 'ring':([(29,-43,-4),(21,-43,-24),(0,-43,-25),(-16,-43,-13),(-18,-43,2)],[8,7.5,7,6.5,4]),
 'pinky':([(27,-59,2),(20,-59,-17),(1,-59,-18),(-12,-59,-7),(-14,-59,6)],[7,6.5,6,5.5,3.5]),
 'thumb':([(22,2,5),(14,7,18),(-2,2,22),(-15,-8,16)],[10,9,8,5])}

def make_design(hand):
    if hand not in ('right','left'):raise ValueError('Unknown hand')
    sign=1 if hand=='right' else -1;d=Design(110)
    d.loft('palm',[(-72,6,13,29),(-61,9,18,24),(-38,10,22,17),(-16,9,21,11),(1,7,17,6)],'body')
    d.parts[-1]['at']=d.at((sign*28,0,0))
    d.box('wrist_cuff',(sign*26,-76,30),(27,18,34),'dark',bevel=4)
    d.box('cuff_pad',(sign*39,-74,30),(4,12,18),'accent',bevel=2)
    d.box('cuff_fastener',(sign*41,-75,32),(2,6,7),'glass',bevel=.7)
    for name,(points,radii) in FINGERS.items():
        points=[(sign*x,y,z) for x,y,z in points];node='pivot_'+name
        d.node(node,points[0]);d.part(name+'_volume','sweep',(0,0,0),'accent',node,0,
            points=[tuple(v/1000 for v in p) for p in points],radii=[r/1000 for r in radii])
    return d

def metadata(hand):
    with (ROOT/'assets/hands'/f'grip-hand-{hand}.toml').open('rb') as f:return tomllib.load(f)

if __name__=='__main__':
    hand=sys.argv[sys.argv.index('--hand')+1]
    del sys.argv[sys.argv.index('--hand'):sys.argv.index('--hand')+2]
    blender_run('grip-hand-'+hand,PARAMETERS,spec=make_design(hand).result('grip-hand-'+hand),meta=metadata(hand))
