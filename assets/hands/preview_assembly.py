"""CPU-only assembly review of our own GLBs; no imported reference anatomy."""
import argparse
import json
from pathlib import Path
import sys
ROOT=Path(__file__).resolve().parents[2]
sys.path.insert(0,str(ROOT/'assets/guns/src'))
from common import runtime_preview_material,preview_material_rgba,RUNTIME_LIGHTING,metadata

def main():
    import bpy
    from mathutils import Quaternion,Vector
    import tomllib
    parser=argparse.ArgumentParser();parser.add_argument('--out',type=Path,required=True);args=parser.parse_args(sys.argv[sys.argv.index('--')+1:])
    if not bpy.app.background or bpy.app.version[:2]!=(5,2):raise RuntimeError('Headless Blender 5.2 required')
    args.out=args.out.resolve()
    if not args.out.is_relative_to(ROOT):raise ValueError('Review output must stay in this checkout')
    args.out.mkdir(parents=True,exist_ok=True)
    assembly=json.loads((ROOT/'assets/hands/assembly.json').read_text())
    scene=bpy.context.scene
    def vec(v):return Vector((v[0],-v[2],v[1]))
    def quat(v):return Quaternion((v[3],v[0],-v[2],v[1]))
    records=[]
    for mid,hand in [('arc-pistol-slide','right'),('con-stick-grip','left')]:
        for obj in list(bpy.data.objects):bpy.data.objects.remove(obj,do_unlink=True)
        meshes=[];finger_nodes={}
        pose=assembly['models'][mid]['hands'][hand]
        for prefix,path,meta in [('model',metadata(mid)['model'],metadata(mid)),('hand',pose['asset'],tomllib.loads((ROOT/'assets/hands'/f'grip-hand-{hand}.toml').read_text()))]:
            before=set(bpy.data.objects);bpy.ops.import_scene.gltf(filepath=str(ROOT/path));created=set(bpy.data.objects)-before
            root=next(o for o in created if o.parent is None)
            for o in created:
                original=o.name.split('.')[0];o.name=prefix+'_'+o.name
                ancestor=o
                lod1=False
                while ancestor:
                    if ancestor.name.startswith(prefix+'_LOD1'):lod1=True
                    ancestor=ancestor.parent
                o.hide_render=lod1
                if o.type=='MESH' and not lod1:
                    meshes.append(o)
                    for i,mat in enumerate(o.data.materials):
                        kind=mat.name.split('.')[0];new=mat.copy();o.data.materials[i]=new
                        runtime_preview_material(new,preview_material_rgba(meta['tints'][meta['tints']['p1']])[kind])
                if prefix=='hand' and original.startswith('pivot_') and not lod1:finger_nodes[original]=o
            if prefix=='hand':
                root.rotation_mode='QUATERNION';root.rotation_quaternion=quat(pose['rotation_xyzw'])@root.rotation_quaternion
                root.location+=vec(pose['translation'])
                for node,override in pose['finger_pose'].items():
                    o=finger_nodes[node];o.location+=vec(override['translation_offset']);o.rotation_mode='QUATERNION';o.rotation_quaternion=quat(override['rotation_xyzw'])@o.rotation_quaternion
                    o.scale=Vector(override.get('scale',[1,1,1]))
        bpy.context.view_layer.update()
        camera_data=bpy.data.cameras.new('review');camera=bpy.data.objects.new('review',camera_data);scene.collection.objects.link(camera);scene.camera=camera;camera_data.type='ORTHO'
        points=[o.matrix_world@Vector(v) for o in meshes for v in o.bound_box]
        lo=Vector(tuple(min(p[i] for p in points) for i in range(3)));hi=Vector(tuple(max(p[i] for p in points) for i in range(3)));target=(lo+hi)/2
        camera_data.ortho_scale=max(hi-lo)*1.35
        scene.render.engine='CYCLES';scene.cycles.device='CPU';scene.cycles.samples=64;scene.cycles.use_denoising=True;scene.cycles.denoiser='OPENIMAGEDENOISE';scene.cycles.denoising_use_gpu=False;scene.cycles.preview_denoising_use_gpu=False
        scene.render.resolution_x=scene.render.resolution_y=512;scene.render.resolution_percentage=100;scene.render.image_settings.file_format='PNG'
        scene.view_settings.view_transform='Standard';scene.view_settings.look='None';scene.view_settings.exposure=0;scene.view_settings.gamma=1
        scene.world.use_nodes=True;bg=scene.world.node_tree.nodes['Background'];bg.inputs['Color'].default_value=(.04,.04,.04,1);bg.inputs['Strength'].default_value=1
        for state in ('rest','stress'):
            if state=='stress':
                for name,o in finger_nodes.items():
                    o.rotation_mode='QUATERNION';o.rotation_quaternion=Quaternion(Vector((1,0,0)),-.17)@o.rotation_quaternion
            bpy.context.view_layer.update()
            for view,offset in [('side',(3 if hand=='right' else -3,0,.5)),('threequarter',(2.1 if hand=='right' else -2.1,2.2,1.25))]:
                camera.location=target+Vector(offset)*.3;camera.rotation_euler=(target-camera.location).to_track_quat('-Z','Y').to_euler()
                filename=f'{mid}-{hand}-{state}-{view}.png';scene.render.filepath=str(args.out/filename);bpy.ops.render.render(write_still=True)
                records.append(dict(model=mid,hand=hand,state=state,view=view,image=filename))
    (args.out/'assembly-review.json').write_text(json.dumps(dict(schema='hand-review/1',lighting=RUNTIME_LIGHTING,device='CPU',samples=64,images=records,acceptance='CPU pose review; physical fit pending'),indent=2)+'\n')
    print('RESULT assembly review: two models, both views, rest and maximum finger drives; CPU only')
if __name__=='__main__':main()
