"""Bounded CPU coverage of proposal grips using the actual assembly metadata."""
import argparse
import json
from pathlib import Path
import sys
import tomllib
ROOT=Path(__file__).resolve().parents[2]
sys.path.insert(0,str(ROOT/'assets/guns/src'))
from common import metadata,preview_material_rgba,runtime_preview_material,RUNTIME_LIGHTING

def cases(root=ROOT):
    assembly=json.loads((root/'assets/hands/assembly.json').read_text())
    proposals=json.loads((root/'assets/guns/preview/wii-proposal-sheet.json').read_text())
    ids=sorted({r['model'] for r in proposals['games']});result=[]
    for mid in ids+['con-stick-grip']:
        for hand in ('right','left'):
            result.append(dict(id=mid+'-'+hand+'-primary',model=mid,primary_hand=hand,hold='primary',hands=[dict(hand=hand,pose=assembly['models'][mid]['hands'][hand])]))
    for mid in ids:
        support=assembly['models'][mid].get('support_grip')
        if not support:continue
        for hand in ('right','left'):
            other='left' if hand=='right' else 'right';pose=dict(assembly['models'][mid]['hands'][other]);pose.update({k:support[k] for k in ('node','translation','rotation_xyzw')})
            result.append(dict(id=mid+'-'+hand+'-support',model=mid,primary_hand=hand,hold='support',hands=[dict(hand=hand,pose=assembly['models'][mid]['hands'][hand]),dict(hand=other,pose=pose)]))
    source=tomllib.loads((root/'data/controls/wii/shell-and-stick.toml').read_text())
    for hand in ('right','left'):
        other='left' if hand=='right' else 'right'
        result.append(dict(id='con-remote-shell-'+hand+'-docked',model='con-remote-shell',primary_hand=hand,hold='docked',hands=[dict(hand=hand,pose=assembly['models']['con-remote-shell']['hands'][hand])],accessory=dict(model='con-stick-grip',node=source['holds']['docked']['attach_node'],rotation_xyzw=source['holds']['docked']['accessory_rotation_xyzw'],hand=other,pose=assembly['models']['con-stick-grip']['hands'][other])))
    if len(result)>24:raise ValueError('Coverage exceeds the approved image budget')
    return result

def main():
    import bpy
    from mathutils import Quaternion,Vector
    parser=argparse.ArgumentParser();parser.add_argument('--out',type=Path,required=True);args=parser.parse_args(sys.argv[sys.argv.index('--')+1:]);args.out=args.out.resolve()
    if not bpy.app.background or bpy.app.version[:2]!=(5,2) or not args.out.is_relative_to(ROOT):raise ValueError('Confined headless Blender5.2 CPU review required')
    args.out.mkdir(parents=True,exist_ok=True);scene=bpy.context.scene;records=[]
    def vec(v):return Vector((v[0],-v[2],v[1]))
    def quat(v):return Quaternion((v[3],v[0],-v[2],v[1]))
    def imported(path,meta,prefix):
        before=set(bpy.data.objects);bpy.ops.import_scene.gltf(filepath=str(ROOT/path));created=set(bpy.data.objects)-before;nodes={};meshes=[]
        root=next(o for o in created if o.parent is None)
        names={o:o.name.split('.')[0] for o in created}
        for o in created:
            name=names[o];ancestor=o;lod1=False
            while ancestor:
                lod1 |= names.get(ancestor,'')=='LOD1';ancestor=ancestor.parent
            o.hide_render=lod1
            if not lod1:nodes[name]=o
            if o.type=='MESH' and not lod1:
                meshes.append(o)
                for i,mat in enumerate(o.data.materials):
                    kind=mat.name.split('.')[0];new=mat.copy();o.data.materials[i]=new;runtime_preview_material(new,preview_material_rgba(meta['tints'][meta['tints']['p1']])[kind])
        for o in created:o.name=prefix+'_'+o.name
        return root,nodes,meshes
    def hand_at(entry,nodes,prefix):
        hand=entry['hand'];pose=entry['pose'];meta=tomllib.loads((ROOT/'assets/hands'/f'grip-hand-{hand}.toml').read_text())
        root,fingers,meshes=imported(pose['asset'],meta,prefix);anchor=nodes[pose['node']]
        root.location=anchor.matrix_world.translation+vec(pose['translation']);root.rotation_mode='QUATERNION';root.rotation_quaternion=quat(pose['rotation_xyzw'])@root.rotation_quaternion
        root.scale=Vector(pose.get('scale',[1,1,1]))
        for node,override in pose['finger_pose'].items():
            o=fingers[node];o.location+=vec(override['translation_offset']);o.rotation_mode='QUATERNION';o.rotation_quaternion=quat(override['rotation_xyzw'])@o.rotation_quaternion;o.scale=Vector(override.get('scale',[1,1,1]))
        return meshes,root
    scene.render.engine='CYCLES';scene.cycles.device='CPU';scene.cycles.samples=64;scene.cycles.use_denoising=True;scene.cycles.denoiser='OPENIMAGEDENOISE';scene.cycles.denoising_use_gpu=False;scene.cycles.preview_denoising_use_gpu=False
    scene.render.resolution_x=scene.render.resolution_y=512;scene.render.resolution_percentage=100;scene.render.image_settings.file_format='PNG';scene.view_settings.view_transform='Standard';scene.view_settings.look='None';scene.view_settings.exposure=0;scene.view_settings.gamma=1
    scene.world.use_nodes=True;bg=scene.world.node_tree.nodes['Background'];bg.inputs['Color'].default_value=(.04,.04,.04,1);bg.inputs['Strength'].default_value=1
    for case in cases():
        for o in list(bpy.data.objects):bpy.data.objects.remove(o,do_unlink=True)
        mid=case['model'];meta=metadata(mid);_,nodes,meshes=imported(meta['model'],meta,'model');hand_roots=[]
        for i,entry in enumerate(case['hands']):
            part,root=hand_at(entry,nodes,'hand'+str(i));meshes+=part;hand_roots.append(root)
        if accessory:=case.get('accessory'):
            ameta=metadata(accessory['model']);aroot,anodes,ameshes=imported(ameta['model'],ameta,'accessory');meshes+=ameshes
            aroot.location=nodes[accessory['node']].matrix_world.translation;aroot.rotation_mode='QUATERNION';aroot.rotation_quaternion=quat(accessory['rotation_xyzw'])@aroot.rotation_quaternion
            bpy.context.view_layer.update();part,root=hand_at(dict(hand=accessory['hand'],pose=accessory['pose']),anodes,'offhand')
            # Match the docked accessory frame, including its authored translation.
            root.rotation_quaternion=quat(accessory['rotation_xyzw'])@root.rotation_quaternion
            root.location=aroot.location+quat(accessory['rotation_xyzw'])@vec(accessory['pose']['translation']);meshes+=part;hand_roots.append(root)
        bpy.context.view_layer.update();points=[o.matrix_world@Vector(v) for o in meshes for v in o.bound_box];lo=Vector(tuple(min(p[i] for p in points) for i in range(3)));hi=Vector(tuple(max(p[i] for p in points) for i in range(3)))
        camdata=bpy.data.cameras.new('coverage');cam=bpy.data.objects.new('coverage',camdata);scene.collection.objects.link(cam);scene.camera=cam;camdata.type='ORTHO'
        hand_center=sum((r.matrix_world.translation for r in hand_roots),Vector((0,0,0)))/len(hand_roots)
        side=1 if case['primary_hand']=='right' else -1
        for view in ('assembly','grip'):
            target=(lo+hi)/2 if view=='assembly' else hand_center+Vector((0,-.01,-.03))
            camdata.ortho_scale=max(hi-lo)*1.35 if view=='assembly' else .22 if len(hand_roots)==1 else .36
            cam.location=target+Vector((side*2.1,2.2,1.25))*.3;cam.rotation_euler=(target-cam.location).to_track_quat('-Z','Y').to_euler()
            filename=case['id']+'-'+view+'.png';scene.render.filepath=str(args.out/filename);bpy.ops.render.render(write_still=True);records.append(dict(case=case['id'],view=view,image=filename))
    (args.out/'coverage.json').write_text(json.dumps(dict(schema='hand-grip-review/1',cases=cases(),images=records,lighting=RUNTIME_LIGHTING,device='CPU',samples=64,acceptance='Visual CPU fit coverage only; no physical acceptance'),indent=2)+'\n')
    print('RESULT bounded grip coverage complete: '+str(len(records))+' CPU images')
if __name__=='__main__':main()
