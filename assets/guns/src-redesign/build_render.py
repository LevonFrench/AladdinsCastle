"""Sequential CPU-only Blender author/export/render entry point."""
import sys, json, argparse, importlib, math, hashlib
from pathlib import Path
sys.path.insert(0,str(Path(__file__).resolve().parent))
import bpy, bmesh
from mathutils import Vector, Matrix
import fresh
ROOT=Path(__file__).resolve().parents[1]

def descendants(o):
    result=[o]
    for c in o.children: result += descendants(c)
    return result

def evaluated_mesh(o):
    ev=o.evaluated_get(bpy.context.evaluated_depsgraph_get())
    return bpy.data.meshes.new_from_object(ev,preserve_all_data_layers=True,depsgraph=bpy.context.evaluated_depsgraph_get())

def apply_modifiers(lod):
    meshes=[o for o in descendants(lod) if o.type=='MESH']
    for o in meshes:
        data=evaluated_mesh(o);o.modifiers.clear();o.data=data
        bm=bmesh.new();bm.from_mesh(data)
        bmesh.ops.remove_doubles(bm,verts=list(bm.verts),dist=1e-7)
        bmesh.ops.dissolve_degenerate(bm,edges=list(bm.edges),dist=1e-7)
        bm.to_mesh(data);bm.free()
    return meshes

def tris(o):
    o.data.calc_loop_triangles();return len(o.data.loop_triangles)

def collapse(lod,budget):
    """v2 (studio fixes 5 and 6): reduce only when the authored LOD is over budget,
    and then only large non-protected shells. Round 1 decimated every part over 50
    triangles, which collapsed thin grip pads and lens caps (inverted normals,
    welded apexes); small parts, glass, sights and thin inserts are now protected."""
    meshes=apply_modifiers(lod)
    total=sum(tris(o) for o in meshes)
    def thin(o):
        d=[max(v.co[i] for v in o.data.vertices)-min(v.co[i] for v in o.data.vertices) for i in range(3)]
        return min(d)<.004
    for floor in (200,120,80):  # widen only if the large shells cannot absorb it
        count=sum(tris(o) for o in meshes)
        if count<=budget:break
        reducible=[o for o in meshes if tris(o)>=floor and not o.get('protect_from_decimation') and not thin(o)]
        if not reducible:continue
        fixed=count-sum(tris(o) for o in reducible)
        ratio=max(.25,(budget-fixed)/(count-fixed)*.95)
        for o in reducible:
            d=o.modifiers.new('lod_budget','DECIMATE');d.ratio=ratio;d.use_collapse_triangulate=True
            data=evaluated_mesh(o);o.modifiers.clear();o.data=data
            bm=bmesh.new();bm.from_mesh(data)
            bmesh.ops.dissolve_degenerate(bm,edges=list(bm.edges),dist=1e-6)
            bm.to_mesh(data);bm.free()
    count=sum(tris(o) for o in meshes)
    parts=sorted(((o.name,tris(o)) for o in meshes),key=lambda x:-x[1])
    return {'triangles':count,'budget':budget,'within_budget':count<=budget,'meshes':len(meshes),
            'pre_reduction_triangles':total,'decimated':total>budget,'largest_parts':parts[:12]}

SHARP_ANGLE_DEG=35

def normals(lod):
    """v2 (studio fix 5): rebuild every mesh without inherited custom normals, mark
    edges sharper than 35 degrees, then apply face-area weighted normals that keep
    those sharp edges. The glTF export splits vertices there, so creases shade flat
    and curved shells stay smooth."""
    for o in [o for o in descendants(lod) if o.type=='MESH']:
        src=o.data
        verts=[v.co.copy() for v in src.vertices]
        faces=[tuple(p.vertices) for p in src.polygons]
        mats=list(src.materials)
        data=bpy.data.meshes.new(src.name+'_n');data.from_pydata(verts,[],faces);data.update()
        for m in mats:data.materials.append(m)
        bm=bmesh.new();bm.from_mesh(data)
        bmesh.ops.recalc_face_normals(bm,faces=list(bm.faces))
        bm.to_mesh(data);bm.free()
        for poly in data.polygons:poly.use_smooth=True
        data.set_sharp_from_angle(angle=math.radians(SHARP_ANGLE_DEG))
        o.data=data
        mod=o.modifiers.new('weighted','WEIGHTED_NORMAL');mod.keep_sharp=True;mod.weight=50;mod.mode='FACE_AREA'
        final=evaluated_mesh(o);o.modifiers.clear();o.data=final

def make_deform(spec):
    """Map authored FHL millimetres to final FHL millimetres (studio fix 1, R09 fix 8).
    origin: re-origin point; scale: uniform; stretch: (f0, k) scales forward of f0."""
    origin=Vector(spec.get('origin',(0,0,0)));scale=spec.get('scale',1.0);stretch=spec.get('stretch')
    def fhl(p):
        q=(Vector(p)-origin)*scale
        if stretch and q.x>stretch[0]:q.x=stretch[0]+(q.x-stretch[0])*stretch[1]
        return q
    def world(v):  # Blender metres (L,F,H) -> Blender metres
        q=fhl((v.y*1000,v.z*1000,v.x*1000));return Vector((q.z/1000,q.x/1000,q.y/1000))
    return fhl,world

def deform(root,world):
    objs=descendants(root)
    bpy.context.view_layer.update()
    before={o.name:o.matrix_world.copy() for o in objs}
    mesh_world={o.name:[before[o.name]@v.co for v in o.data.vertices] for o in objs if o.type=='MESH'}
    for o in objs:  # parents first: descendants() is pre-order
        if o is root:continue
        m=before[o.name].copy()
        if o.type=='MESH':
            m=Matrix.Identity(4)
        else:
            m.translation=world(before[o.name].translation)
        o.matrix_world=m
        bpy.context.view_layer.update()
    for o in objs:
        if o.type!='MESH':continue
        inv=o.matrix_world.inverted()
        for v,w in zip(o.data.vertices,mesh_world[o.name]):v.co=inv@world(w)
        o.data.update()

def bounds(objs):
    bpy.context.view_layer.update()
    pts=[o.matrix_world@Vector(v) for o in objs if o.type=='MESH' for v in o.bound_box]
    lo=Vector([min(p[i] for p in pts) for i in range(3)])
    hi=Vector([max(p[i] for p in pts) for i in range(3)])
    return lo,hi

def lighting(center,span):
    scene=bpy.context.scene
    scene.render.engine='CYCLES';scene.cycles.device='CPU';scene.cycles.samples=32
    scene.cycles.use_denoising=True;scene.cycles.max_bounces=4
    scene.render.threads_mode='FIXED';scene.render.threads=4
    scene.world.color=(.18,.18,.18)
    scene.world.use_nodes=True
    scene.world.node_tree.nodes.get('Background').inputs['Color'].default_value=(.30,.33,.38,1)
    scene.world.node_tree.nodes.get('Background').inputs['Strength'].default_value=.45
    scene.view_settings.view_transform='AgX';scene.view_settings.look='AgX - Medium High Contrast'
    scene.view_settings.exposure=.2
    scene.render.image_settings.file_format='PNG';scene.render.film_transparent=True
    for name,offset,power,size in [('Key',(1,-.7,1.4),5.5,1.3),('Fill',(-1,.2,.6),3.0,1.6),('Rim',(.2,1.3,1),6.0,.9)]:
        data=bpy.data.lights.new(name,'AREA');data.energy=power*(span/.30)**2;data.shape='DISK';data.size=size*span
        o=bpy.data.objects.new(name,data);scene.collection.objects.link(o)
        o.location=center+Vector(offset)*span*2
        o.rotation_euler=(center-o.location).to_track_quat('-Z','Y').to_euler()

def render_views(objs,path,views,resolution,samples):
    scene=bpy.context.scene;lo,hi=bounds(objs);center=(lo+hi)/2;span=max(hi-lo)
    lighting(center,span)
    scene.cycles.samples=samples
    cam_data=bpy.data.cameras.new('review_camera');cam=bpy.data.objects.new('review_camera',cam_data)
    scene.collection.objects.link(cam);scene.camera=cam;cam_data.type='ORTHO';cam_data.lens=65
    scene.render.resolution_x=resolution;scene.render.resolution_y=round(resolution*.78);scene.render.resolution_percentage=100
    directions={'threequarter':Vector((1,.72,.55)),'side':Vector((1,0,.06)),'held':Vector((.24,-1,.40))}
    for view in views:
        cam.location=center+directions[view].normalized()*span*4
        cam.rotation_euler=(center-cam.location).to_track_quat('-Z','Y').to_euler()
        bpy.context.view_layer.update()
        inv=cam.matrix_world.inverted()
        p=[inv@(o.matrix_world@Vector(v)) for o in objs if o.type=='MESH' for v in o.bound_box]
        dx=max(v.x for v in p)-min(v.x for v in p);dy=max(v.y for v in p)-min(v.y for v in p)
        cam_data.ortho_scale=max(dx,dy/(scene.render.resolution_y/scene.render.resolution_x))*1.20
        scene.render.filepath=str(path/f'{view}.png')
        bpy.ops.render.render(write_still=True)
        print('VIEW '+json.dumps({'file':scene.render.filepath,'view':view}),flush=True)

def main():
    p=argparse.ArgumentParser();p.add_argument('--model',required=True);p.add_argument('--views',default='threequarter');p.add_argument('--resolution',type=int,default=640);p.add_argument('--samples',type=int,default=24)
    args=p.parse_args(sys.argv[sys.argv.index('--')+1:])
    assert bpy.app.version[:2]==(5,2),bpy.app.version_string
    bpy.ops.object.select_all(action='SELECT');bpy.ops.object.delete(use_global=False)
    for m in list(bpy.data.materials):bpy.data.materials.remove(m)
    if args.model=='benchmark':
        review=json.loads((ROOT.parents[1]/'gun-contact-sheet/owner-review-2026-10-21.json').read_text(encoding='utf-8'))
        ref=next(x for x in review['records'] if x['id']=='001')
        source=Path(ref['representative_path'])
        assert hashlib.sha1(source.read_bytes()).hexdigest()==ref['sha1_at_contact_sheet'],'Benchmark changed since owner review'
        bpy.ops.import_scene.gltf(filepath=str(source))
        lod1=bpy.data.objects.get('LOD1')
        if lod1:
            for o in descendants(lod1):o.hide_render=True;o.hide_set(True)
        objs=[o for o in bpy.context.scene.objects if o.type=='MESH' and not o.hide_render]
        path=ROOT/'renders/benchmark';path.mkdir(parents=True,exist_ok=True)
        render_views(objs,path,args.views.split(','),args.resolution,args.samples)
        print('RESULT '+json.dumps({'benchmark_only':True,'source':str(source),'sha1':ref['sha1_at_contact_sheet']}),flush=True)
        return
    fresh.materials()
    models={
      'arcade':['arc-pistol-slide','arc-pistol-twin','con-pistol-slim','con-pistol-dpad'],
      'controllers':['con-pointer-remote','con-remote-shell','con-stick-grip','con-revolver-chunky'],
      'specials':['mnt-mg-heavy','game-energy-gun','game-revolver-heavy','game-tool-cutter'],
      'generic':['generic-pistol']}
    module=importlib.import_module(next(k for k,v in models.items() if args.model in v))
    root=bpy.data.objects.new('grip',None);bpy.context.scene.collection.objects.link(root)
    root['model_id']=args.model;root['source']='original complete redesign 2026-10-21, v2 studio fixes'
    lods=[];audits=[];metadata=[]
    for level,budget in [(0,8000),(1,2000)]:
        lod=bpy.data.objects.new(f'LOD{level}',None);bpy.context.scene.collection.objects.link(lod);lod.parent=root
        lod['level']=level;lods.append(lod);fresh.setup(lod,level)
        data=module.build(args.model,level);metadata.append(data)
        audits.append(collapse(lod,budget))
    fresh.setup(None,0)
    data=dict(metadata[0])
    spec=data.pop('_deform',None) or {}
    fhl,world=make_deform(spec)
    if spec:
        deform(root,world)
        for name,loc in list(data.items()):
            if isinstance(loc,(list,tuple)) and len(loc)==3 and all(isinstance(v,(int,float)) for v in loc):
                data[name]=tuple(round(v,3) for v in fhl(loc))
        if 'length_mm' in data:data['length_mm_authored']=data['length_mm']
    for lod in lods:normals(lod)
    functional=data.pop('functional_sights',False)
    # The original runtime contract declares 15 mm travel for recoil slides.
    slide=bpy.data.objects.get('slide_recoil')
    if slide:
        for n in [slide,bpy.data.objects.get('slide_recoil_lod1')]:
            if n:n['limit_recoil']=.015
        bpy.context.view_layer.update()
        pos=slide.matrix_world.translation
        data['limit_recoil']=(pos.y*1000-15,pos.z*1000,pos.x*1000)
    for name,loc in data.items():
        if data.get('asset_kind')!='controller' and name.startswith(('muzzle','sight_','fx_','grip_two','limit_recoil')) and isinstance(loc,(list,tuple)):
            n=fresh.node(name,loc,root)
    if 'muzzle' in data and data.get('asset_kind')!='controller':
        for name in ['fx_muzzle','fx_laser']:fresh.node(name,data['muzzle'],root)
    root['asset_kind']=data.get('asset_kind','gun')
    # v2 (studio fix 4): the declarations tools/check_gun_assets.py reads from the grip
    # root, written the way assets/guns/src/common.py blender_run() writes them.
    root['asset_id']=args.model;root['units']='metres'
    if root['asset_kind']=='gun':root['bore']='-Z'
    if functional:
        root['functional_sights']={'kind':'notch-blade','notch_width_m':.006,'notch_depth_m':.003,'front_width_m':.003}
    out=ROOT/'models'/args.model;out.mkdir(parents=True,exist_ok=True)
    renders=ROOT/'renders'/args.model;renders.mkdir(parents=True,exist_ok=True)
    all_objs=descendants(root)
    for o in bpy.context.selected_objects:o.select_set(False)
    for o in all_objs:o.select_set(True)
    bpy.context.view_layer.objects.active=root
    bpy.ops.export_scene.gltf(filepath=str(out/(args.model+'.glb')),export_format='GLB',use_selection=True,export_apply=True,export_extras=True,export_animations=False,export_yup=True,export_cameras=False,export_lights=False)
    for o in descendants(lods[1]):o.hide_render=True;o.hide_set(True)
    # Independent exported topology/budget receipt; rendered geometry is the exact exported LOD0.
    lo,hi=bounds(descendants(lods[0]));bad=[];zero=0
    for o in all_objs:
        if o.type!='MESH':continue
        bm=bmesh.new();bm.from_mesh(o.data)
        non=sum(not e.is_manifold for e in bm.edges)
        if non:bad.append({'mesh':o.name,'nonmanifold_edges':non})
        zero+=sum(f.calc_area()<1e-13 for f in bm.faces);bm.free()
    receipt={'model':args.model,'deform':spec,'blender':bpy.app.version_string,'device':'CPU','lods':audits,'bounds_m':{'min':list(lo),'max':list(hi)},'dimensions_mm':list((hi-lo)*1000),'anchors_fhl_mm':{k:v for k,v in data.items()},'nonmanifold':bad,'zero_area_faces':zero,'glb':str(out/(args.model+'.glb'))}
    (out/'receipt.json').write_text(json.dumps(receipt,indent=2)+'\n',encoding='utf-8')
    render_views(descendants(lods[0]),renders,args.views.split(','),args.resolution,args.samples)
    bpy.ops.wm.save_as_mainfile(filepath=str(out/(args.model+'.blend')))
    print('RESULT '+json.dumps(receipt),flush=True)

if __name__=='__main__':main()
