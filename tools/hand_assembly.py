"""Compile original grip attachments and static label anchors, without Blender."""
import argparse
import json
import math
from pathlib import Path
import sys
ROOT=Path(__file__).resolve().parents[1]
sys.path.insert(0,str(ROOT/'assets/guns/src'))
from common import BUILT_MODELS,plan

def compile_assembly():
    models={}
    for mid in BUILT_MODELS:
        spec=plan(mid,{})
        pitch=0 if mid in ('mnt-mg-heavy','con-stick-grip') else math.radians(15)
        rotation=[math.sin(pitch/2),0,0,math.cos(pitch/2)]
        hands={hand:dict(asset=f'assets/hands/grip-hand-{hand}.glb',node='grip',translation=[0,0,0],rotation_xyzw=rotation,finger_pose={}) for hand in ('right','left')}
        if mid=='con-stick-grip':
            for pose in hands.values():
                pose['translation']=[0,-.005,.012]
                pose['finger_pose']={'pivot_thumb':dict(translation_offset=[0,.040,-.026],rotation_xyzw=[-.3826834324,0,0,.9238795325])}
        anchors={n:dict(node=n,local_offset=[0,0,0],bind_position=list(d['at'])) for n,d in spec['nodes'].items() if n in ('muzzle','grip_two','pivot_trigger') or n.startswith('button_')}
        anchors['grip']=dict(node='grip',local_offset=[0,0,0],bind_position=[0,0,0])
        models[mid]=dict(hands=hands,label_anchors=anchors)
        if 'grip_two' in spec['nodes']:
            models[mid]['support_grip']=dict(node='grip_two',translation=[0,0,0],rotation_xyzw=[0,0,0,1],state='provisional')
    return dict(schema='hand-assembly/1',units='metres',axes=dict(up='+Y',forward='-Z',handedness='right'),quaternion_order='xyzw',hand_root='grip',state='provisional',physical_fit='CPU review only; owner sitting pending',lighting='assets/guns/runtime-lighting.json',models=models)

def main():
    parser=argparse.ArgumentParser(description=__doc__);parser.add_argument('--write',action='store_true');args=parser.parse_args()
    path=ROOT/'assets/hands/assembly.json';payload=json.dumps(compile_assembly(),indent=2)+'\n'
    if args.write:path.write_text(payload,encoding='utf-8')
    elif path.read_text(encoding='utf-8')!=payload:raise ValueError('Stale hand assembly data')
    print('hand-assembly/1: nine unchanged models, original left/right hands, semantic label anchors')
if __name__=='__main__':main()
