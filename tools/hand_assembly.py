"""Compile original grip attachments and static label anchors, without Blender."""
import argparse
import json
import math
from pathlib import Path
import sys
import tomllib
ROOT=Path(__file__).resolve().parents[1]
sys.path.insert(0,str(ROOT/'assets/guns/src'));sys.path.insert(0,str(ROOT/'assets/hands/src'))
from common import BUILT_MODELS,plan
from grip_hand import FINGERS
from check_gun_assets import IDENTITY,accessor,local_matrix,multiply,read_glb,transform
# The index pad rests on the trigger's front face this far down the blade: low
# enough that the rigid middle finger clears under the guard on the pistols.
TRIGGER_PAD=.75

def between(a,b):
    la=math.sqrt(sum(x*x for x in a));lb=math.sqrt(sum(x*x for x in b))
    a=[v/la for v in a];b=[v/lb for v in b]
    q=[a[1]*b[2]-a[2]*b[1],a[2]*b[0]-a[0]*b[2],a[0]*b[1]-a[1]*b[0],1+sum(x*y for x,y in zip(a,b))]
    length=math.sqrt(sum(x*x for x in q))
    return [v/length for v in q]

def trigger_blade(mid):
    """LOD0 trigger-blade triangles of the exported gun, in grip space; empty without a trigger."""
    doc,binary=read_glb(ROOT/'assets/guns'/f'{mid}.glb');triangles=[]
    def visit(index,world,lod0):
        node=doc['nodes'][index];name=node.get('name','');world=multiply(world,local_matrix(node));lod0=lod0 or name=='LOD0'
        if lod0 and 'mesh' in node and 'trigger' in name and 'guard' not in name:
            for prim in doc['meshes'][node['mesh']]['primitives']:
                points=[transform(world,p) for p in accessor(doc,binary,prim['attributes']['POSITION'])]
                order=[row[0] for row in accessor(doc,binary,prim['indices'])]
                triangles.extend([points[i] for i in order[k:k+3]] for k in range(0,len(order),3))
        for child in node.get('children',[]):visit(child,world,lod0)
    visit(doc['scenes'][0]['nodes'][0],IDENTITY,False)
    return triangles

def trigger_finger(mid,rotation):
    """Hand translation that puts the index fingertip pad on the trigger blade's front face.

    The pad height is TRIGGER_PAD down the blade; the face is read where the mesh
    crosses the gun's mid-plane (x=0) at that height."""
    triangles=trigger_blade(mid)
    if not triangles:return [0,0,0]
    heights=[p[1] for t in triangles for p in t];top=max(heights)
    y=top-(top-min(heights))*TRIGGER_PAD;front=[]
    for t in triangles:
        cut=[(a[1]+(b[1]-a[1])*a[0]/(a[0]-b[0]),a[2]+(b[2]-a[2])*a[0]/(a[0]-b[0])) for a,b in ((t[0],t[1]),(t[1],t[2]),(t[2],t[0])) if (a[0]<0)!=(b[0]<0)]
        if len(cut)==2 and (cut[0][0]-y)*(cut[1][0]-y)<=0 and cut[0][0]!=cut[1][0]:
            front.append(cut[0][1]+(y-cut[0][0])/(cut[1][0]-cut[0][0])*(cut[1][1]-cut[0][1]))
    points,radii=FINGERS['index'];tip=[v/1000 for v in points[-1]];pitch=2*math.atan2(rotation[0],rotation[3])
    tip_y=tip[1]*math.cos(pitch)-tip[2]*math.sin(pitch);tip_z=tip[1]*math.sin(pitch)+tip[2]*math.cos(pitch)
    return [0,round(y-tip_y,4),round(min(front)-radii[-1]/1000-tip_z,4)]

def compile_assembly():
    policy=tomllib.loads((ROOT/'data/controls/wii/shell-and-stick.toml').read_text())
    models={}
    for mid in BUILT_MODELS:
        spec=plan(mid,{})
        pitch=0 if mid in ('mnt-mg-heavy','con-stick-grip') else math.radians(15)
        rotation=[math.sin(pitch/2),0,0,math.cos(pitch/2)]
        # Spade grips and the stick grip keep the hand at the grip node; pistols put the index on the trigger.
        translation=[0,0,0] if mid in ('mnt-mg-heavy','con-stick-grip') else trigger_finger(mid,rotation)
        hands={hand:dict(asset=f'assets/hands/grip-hand-{hand}.glb',node='grip',translation=list(translation),rotation_xyzw=rotation,finger_pose={}) for hand in ('right','left')}
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
