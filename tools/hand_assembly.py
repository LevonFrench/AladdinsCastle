"""Compile original grip attachments and static label anchors, without Blender."""
import argparse
import json
import math
from pathlib import Path
import sys
import tomllib
ROOT=Path(__file__).resolve().parents[1]
sys.path.insert(0,str(ROOT/'assets/guns/src'))
from common import BUILT_MODELS,plan

def between(a,b):
    la=math.sqrt(sum(x*x for x in a));lb=math.sqrt(sum(x*x for x in b))
    a=[v/la for v in a];b=[v/lb for v in b]
    q=[a[1]*b[2]-a[2]*b[1],a[2]*b[0]-a[0]*b[2],a[0]*b[1]-a[1]*b[0],1+sum(x*y for x,y in zip(a,b))]
    length=math.sqrt(sum(x*x for x in q))
    return [v/length for v in q]

def compile_assembly():
    policy=tomllib.loads((ROOT/'data/controls/wii/shell-and-stick.toml').read_text())
    models={}
    for mid in BUILT_MODELS:
        spec=plan(mid,{})
        pitch=0 if mid in ('mnt-mg-heavy','con-stick-grip') else math.radians(15)
        rotation=[math.sin(pitch/2),0,0,math.cos(pitch/2)]
        hands={hand:dict(asset=f'assets/hands/grip-hand-{hand}.glb',node='grip',translation=[0,0,0],rotation_xyzw=rotation,finger_pose={}) for hand in ('right','left')}
        if mid=='con-stick-grip':
            for hand,pose in hands.items():
                pose['translation']=[0,-.005,.012]
                sign=1 if hand=='right' else -1
                # Keep the base at the palm; turn the distal direction to the cap.
                pose['finger_pose']={'pivot_thumb':dict(translation_offset=[0,0,0],rotation_xyzw=between([-sign*.037,-.010,.011],[-sign*.022,.045,-.024]),scale=[1.25,1.25,1.25])}
        anchors={n:dict(node=n,local_offset=[0,0,0],bind_position=list(d['at'])) for n,d in spec['nodes'].items() if n in ('muzzle','grip_two','pivot_trigger') or n.startswith('button_')}
        anchors['grip']=dict(node='grip',local_offset=[0,0,0],bind_position=[0,0,0])
        models[mid]=dict(hands=hands,label_anchors=anchors,primary_mount_by_hand={hand:dict(node=pose['node'],translation=[0,0,0],rotation_xyzw=[0,0,0,1]) for hand,pose in hands.items()})
        if 'grip_two' in spec['nodes']:
            models[mid]['support_grip']=dict(node='grip_two',translation=[0,0,0],rotation_xyzw=[0,0,0,1],state='provisional')
            if mid=='con-remote-shell':models[mid]['support_grip']['rotation_xyzw']=policy['holds']['docked']['accessory_rotation_xyzw']
            models[mid]['support_by_primary']={hand:dict(models[mid]['support_grip']) for hand in ('right','left')}
            if mid=='mnt-mg-heavy':
                models[mid]['hands']['left']['node']='grip_two'
                models[mid]['primary_mount_by_hand']['left']['node']='grip_two'
                models[mid]['support_by_primary']['left']['node']='grip'
    return dict(schema='hand-assembly/1',units='metres',axes=dict(up='+Y',forward='-Z',handedness='right'),quaternion_order='xyzw',hand_root='grip',state='provisional',physical_fit='CPU review only; owner sitting pending',lighting='assets/guns/runtime-lighting.json',models=models)

def main():
    parser=argparse.ArgumentParser(description=__doc__);parser.add_argument('--write',action='store_true');args=parser.parse_args()
    path=ROOT/'assets/hands/assembly.json';payload=json.dumps(compile_assembly(),indent=2)+'\n'
    if args.write:path.write_text(payload,encoding='utf-8')
    elif path.read_text(encoding='utf-8')!=payload:raise ValueError('Stale hand assembly data')
    print(f'hand-assembly/1: {len(BUILT_MODELS)} models, original left/right hands, semantic label anchors')
if __name__=='__main__':main()
