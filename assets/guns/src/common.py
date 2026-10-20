"""Original parametric toy peripherals. GPL-3.0-only; generated assets CC0.

Geometry parameters use glTF coordinates (metres, +Y up, -Z bore).
Importing this module does not load Blender or start any application.
"""
from __future__ import annotations

import argparse
import json
import math
from pathlib import Path
import sys
import tomllib

ROOT = Path(__file__).resolve().parents[3]
TIERS = ('generic-pistol', 'arc-pistol-slide', 'arc-pistol-twin',
         'con-pistol-slim', 'con-pistol-dpad', 'con-revolver-chunky', 'mnt-mg-heavy')
BUILT_MODELS = TIERS + ('con-remote-shell','con-stick-grip')
MATERIAL_RGBA = {'body':(.55,.55,.55,1), 'accent':(.32,.32,.34,1),
                 'dark':(.012,.012,.018,1), 'glass':(.025,.10,.16,1)}
RUNTIME_LIGHTING=json.loads((ROOT/'assets/guns/runtime-lighting.json').read_text(encoding='utf-8'))


def runtime_preview_rgba(normal_view, color):
    """Reference camera-space Lambert result before display encoding/premultiply."""
    n=math.sqrt(sum(v*v for v in normal_view)); light=RUNTIME_LIGHTING['direction_to_light']
    l=math.sqrt(sum(v*v for v in light))
    if not n or not l: raise ValueError('Lighting normal and direction must be nonzero')
    factor=RUNTIME_LIGHTING['ambient']+RUNTIME_LIGHTING['directional']*max(sum(a*b for a,b in zip(normal_view,light))/(n*l),0)
    return tuple(v*factor for v in color[:3])+(color[3],)


def runtime_preview_material(mat, color):
    """Emission-only shader: no Cycles lights, BSDF, specular or shadows."""
    tree=mat.node_tree; tree.nodes.clear()
    geometry_node=tree.nodes.new('ShaderNodeNewGeometry')
    view=tree.nodes.new('ShaderNodeVectorTransform'); view.vector_type='NORMAL'
    view.convert_from='WORLD'; view.convert_to='CAMERA'
    tree.links.new(geometry_node.outputs['Normal'],view.inputs['Vector'])
    normal=tree.nodes.new('ShaderNodeVectorMath'); normal.operation='NORMALIZE'
    tree.links.new(view.outputs['Vector'],normal.inputs[0])
    dot=tree.nodes.new('ShaderNodeVectorMath'); dot.operation='DOT_PRODUCT'
    light=RUNTIME_LIGHTING['direction_to_light']; length=math.sqrt(sum(v*v for v in light))
    dot.inputs[1].default_value=tuple(v/length for v in light)
    tree.links.new(normal.outputs['Vector'],dot.inputs[0])
    positive=tree.nodes.new('ShaderNodeMath'); positive.operation='MAXIMUM'; positive.inputs[1].default_value=0
    tree.links.new(dot.outputs['Value'],positive.inputs[0])
    diffuse=tree.nodes.new('ShaderNodeMath'); diffuse.operation='MULTIPLY'; diffuse.inputs[1].default_value=RUNTIME_LIGHTING['directional']
    tree.links.new(positive.outputs[0],diffuse.inputs[0])
    ambient=tree.nodes.new('ShaderNodeMath'); ambient.operation='ADD'; ambient.inputs[1].default_value=RUNTIME_LIGHTING['ambient']
    tree.links.new(diffuse.outputs[0],ambient.inputs[0])
    emission=tree.nodes.new('ShaderNodeEmission'); emission.inputs['Color'].default_value=color
    tree.links.new(ambient.outputs[0],emission.inputs['Strength'])
    transparent=tree.nodes.new('ShaderNodeBsdfTransparent')
    mix=tree.nodes.new('ShaderNodeMixShader'); mix.inputs[0].default_value=color[3]
    tree.links.new(transparent.outputs[0],mix.inputs[1]); tree.links.new(emission.outputs[0],mix.inputs[2])
    output=tree.nodes.new('ShaderNodeOutputMaterial'); tree.links.new(mix.outputs[0],output.inputs['Surface'])


def preview_material_rgba(tint):
    """Match native linear base × palette tint, retaining fixed dark/glass."""
    result = dict(MATERIAL_RGBA)
    for name in ('body','accent'):
        rgb = [int(tint[name][i:i+2],16)/255 for i in (1,3,5)]
        linear = tuple(v/12.92 if v <= .04045 else ((v+.055)/1.055)**2.4 for v in rgb)+(1,)
        result[name] = tuple(base*channel for base,channel in zip(MATERIAL_RGBA[name],linear))
    return result


def metadata(model_id):
    with (ROOT / 'data/guns' / (model_id + '.toml')).open('rb') as stream:
        return tomllib.load(stream)


class Design:
    """Geometry mechanics only; all silhouettes and dimensions belong to each script.

    Side coordinates are (z, y) in millimetres, muzzle to the left (-z).
    Origin is the web-of-hand position; its orientation is the model bore frame.
    """
    def __init__(self, length, origin=(0,0,0)):
        self.length, self.origin = length, origin
        self.parts, self.nodes = [], {}

    def at(self, p):
        return tuple((v-o)/1000 for v,o in zip(p,self.origin))

    def node(self, name, at, parent=None):
        self.nodes[name] = dict(at=self.at(at),parent=parent)

    def markers(self, muzzle, front, rear, trigger, parent=None):
        for name in ('muzzle','fx_muzzle','fx_laser'):
            self.node(name,muzzle)
        self.node('sight_front',front)
        self.node('sight_rear',rear)
        self.node('pivot_trigger',trigger,parent)

    def part(self, name, shape, at, material, parent, bevel, **data):
        self.parts.append(dict(name=name,shape=shape,at=self.at(at),material=material,
                               parent=parent,bevel=bevel/1000,**data))

    def outline(self, name, points, width, material='body', parent=None, x=0, bevel=3):
        # Localise points before parenting; avoid applying world translations twice.
        z = (min(p[0] for p in points)+max(p[0] for p in points))/2
        y = (min(p[1] for p in points)+max(p[1] for p in points))/2
        self.part(name,'outline',(x,y,z),material,parent,bevel,
                  points=[((a-z)/1000,(b-y)/1000) for a,b in points],width=width/1000)

    def ring(self, name, outer, inner, width, material='body', parent=None):
        assert len(outer)==len(inner)
        self.part(name,'ring',(0,0,0),material,parent,2,
                  outer=[(z/1000,y/1000) for z,y in outer],
                  inner=[(z/1000,y/1000) for z,y in inner],width=width/1000)

    def box(self, name, at, size, material='body', parent=None, bevel=2):
        self.part(name,'box',at,material,parent,bevel,size=tuple(v/1000 for v in size))

    def cylinder(self, name, at, radius, depth, material='body', parent=None, axis='z'):
        self.part(name,'cylinder',at,material,parent,1,radius=radius/1000,depth=depth/1000,axis=axis)

    def tube(self, name, at, radius, inner, depth, parent=None):
        self.part(name,'vented_tube',at,'accent',parent,0,radius=radius/1000,
                  inner=inner/1000,depth=depth/1000)

    def zloft(self,name,rings,material='body',parent=None,power=4):
        """Rounded bar along Z: rings of (z, half width, half height, y centre) in mm, each a
        superellipse |x/a|^p + |y/b|^p = 1 (p=2 ellipse, larger p squarer), capped at both ends."""
        self.part(name,'zloft',(0,0,0),material,parent,0,power=power,
                  rings=[tuple(v/1000 for v in ring) for ring in rings])

    def loft(self,name,rings,material='body',parent=None):
        self.part(name,'loft',(0,0,0),material,parent,0,
                  rings=[tuple(v/1000 for v in ring) for ring in rings])

    def functional_sights(self, front, rear, parent=None):
        """Seated bases, level blade/notch tops and an open bore-parallel line.

        Inputs retain the authored mounting centres. Raise the shorter sight
        instead of lifting its base off the sloping roof. Both LODs retain the
        complete U profile; no boolean or low-detail solid-block substitute.
        """
        top=max(front[1],rear[1])+3
        for name,at,width in (('front_sight_geometry',front,3),('rear_sight_geometry',rear,18)):
            bottom=at[1]-3; height=top-bottom
            centre=(at[0],(bottom+top)/2,at[2])
            if name.startswith('front'):
                self.box(name,centre,(width,height,8),'dark',parent,.25)
            else:
                self.part(name,'notched_sight',centre,'dark',parent,0,
                          size=(width/1000,height/1000,.008),notch_width=.006,notch_depth=.003)
        self.node('sight_front',(front[0],top,front[2]))
        self.node('sight_rear',(rear[0],top,rear[2]))
        self.sight_setup=dict(kind='notch-blade',notch_width_m=.006,notch_depth_m=.003,front_width_m=.003)

    def result(self, mid):
        result=dict(id=mid,length_m=self.length/1000,nodes=self.nodes,parts=self.parts)
        if hasattr(self,'sight_setup'): result['sights']=self.sight_setup
        return result


def plan(model_id, parameters):
    """Load the model's own shape function without importing bpy."""
    import runpy
    module = runpy.run_path(str(Path(__file__).with_name(model_id+'.py')))
    return module['make_design']().result(model_id)


def geometry(part, lod):
    """Return glTF-local vertices/faces; also usable by pure-Python source checks."""
    shape = part['shape']
    if shape == 'loft':
        n=24 if lod==0 else 12
        vertices=[(rx*math.cos(i*math.tau/n),y,z+rz*math.sin(i*math.tau/n))
                  for y,rx,rz,z in part['rings'] for i in range(n)]
        faces=[tuple(reversed(range(n))),tuple(range(len(vertices)-n,len(vertices)))]
        for ring in range(len(part['rings'])-1):
            for i in range(n):
                a=ring*n+i;b=ring*n+(i+1)%n
                faces.append((a,b,b+n,a+n))
        return vertices,faces
    if shape == 'zloft':
        n=32 if lod==0 else 16;e=2/part['power'];vertices=[]
        for z,a,b,yc in part['rings']:
            for i in range(n):
                t=i*math.tau/n;c,s=math.cos(t),math.sin(t)
                vertices.append((a*math.copysign(abs(c)**e,c),yc+b*math.copysign(abs(s)**e,s),z))
        faces=[tuple(reversed(range(n))),tuple(range(len(vertices)-n,len(vertices)))]
        for ring in range(len(part['rings'])-1):
            for i in range(n):
                a=ring*n+i;b=ring*n+(i+1)%n
                faces.append((a,b,b+n,a+n))
        return vertices,faces
    if shape == 'sweep':
        # Authored finger volumes: closed swept rings, without skin weights or
        # Blender-only deformation. Stable frames follow the local tangent.
        points=part['points'];n=16 if lod==0 else 8;vertices=[]
        def unit(v):
            length=math.sqrt(sum(x*x for x in v));return tuple(x/length for x in v)
        def cross(a,b):return (a[1]*b[2]-a[2]*b[1],a[2]*b[0]-a[0]*b[2],a[0]*b[1]-a[1]*b[0])
        for i,p in enumerate(points):
            a=points[max(0,i-1)];b=points[min(len(points)-1,i+1)]
            tangent=unit(tuple(y-x for x,y in zip(a,b)))
            reference=(0,1,0) if abs(tangent[1])<.9 else (1,0,0)
            side=unit(cross(tangent,reference));up=cross(side,tangent)
            radius=part['radii'][i]
            for j in range(n):
                angle=j*math.tau/n
                vertices.append(tuple(p[k]+radius*(side[k]*math.cos(angle)+up[k]*math.sin(angle)) for k in range(3)))
        faces=[tuple(reversed(range(n))),tuple(range(len(vertices)-n,len(vertices)))]
        for ring in range(len(points)-1):
            for i in range(n):
                a=ring*n+i;b=ring*n+(i+1)%n;faces.append((a,b,b+n,a+n))
        return vertices,faces
    if shape == 'notched_sight':
        x,y,z=(v/2 for v in part['size']); gap=part['notch_width']/2
        floor=y-part['notch_depth']
        profile=[(-x,-y),(x,-y),(x,y),(gap,y),(gap,floor),(-gap,floor),(-gap,y),(-x,y)]
        verts=[(a,b,c) for c in (-z,z) for a,b in profile]
        n=len(profile)
        return verts,[tuple(reversed(range(n))),tuple(range(n,2*n))]+[(i,(i+1)%n,(i+1)%n+n,i+n) for i in range(n)]
    if shape == 'box':
        x,y,z = (v/2 for v in part['size'])
        return ([(-x,-y,-z),(-x,-y,z),(-x,y,z),(-x,y,-z),
                 (x,-y,-z),(x,-y,z),(x,y,z),(x,y,-z)],
                [(0,3,2,1),(4,5,6,7),(0,1,5,4),(3,7,6,2),(0,4,7,3),(1,2,6,5)])
    if shape in ('outline','ring'):
        contours = [part['points']] if shape=='outline' else [part['outer'],part['inner']]
        n = len(contours[0]); w = part['width']/2
        verts = [(x,y,z) for x in (-w,w) for contour in contours for z,y in contour]
        if shape=='outline':
            faces = [tuple(reversed(range(n))),tuple(range(n,2*n))]
            faces += [(i,(i+1)%n,(i+1)%n+n,i+n) for i in range(n)]
        else:
            faces=[]
            for i in range(n):
                j=(i+1)%n
                faces.extend([(i,j,j+2*n,i+2*n),(i+n,i+3*n,j+3*n,j+n),
                              (i,i+n,j+n,j),(i+2*n,j+2*n,j+3*n,i+3*n)])
        return verts,faces
    n = 24 if lod==0 else 8
    if shape=='cylinder':
        verts=[]
        for depth in (-part['depth']/2,part['depth']/2):
            for i in range(n):
                a=i*math.tau/n
                xyz=(math.cos(a)*part['radius'],math.sin(a)*part['radius'],depth)
                if part['axis']=='x': xyz=(xyz[2],xyz[0],xyz[1])
                elif part['axis']=='y': xyz=(xyz[1],xyz[2],xyz[0])
                verts.append(xyz)
        faces=[tuple(reversed(range(n))),tuple(range(n,2*n))]
        faces += [(i,(i+1)%n,(i+1)%n+n,i+n) for i in range(n)]
        return verts,faces
    if shape=='vented_tube':
        # Closed front/rear collars plus eight broad longitudinal ribs and intermediate collars. The windows
        # between ribs are actual openings, not painted dots or boolean cutters.
        verts,faces=[],[]
        def annulus(z0,z1,start,count):
            base=len(verts)
            for z in (z0,z1):
                for r in (part['radius'],part['inner']):
                    for i in range(count+1):
                        a=(start+i)*math.tau/n
                        verts.append((math.cos(a)*r,math.sin(a)*r,z))
            k=count+1
            for i in range(count):
                faces.extend([tuple(base+j for j in q) for q in
                              ((i,i+1,i+1+2*k,i+2*k),(i+k,i+3*k,i+1+3*k,i+1+k),
                               (i,i+k,i+1+k,i+1),(i+2*k,i+1+2*k,i+1+3*k,i+3*k))])
            faces.extend([tuple(base+j for j in q) for q in
                          ((0,2*k,3*k,k),(count,count+k,count+3*k,count+2*k))])
        n=24 if lod==0 else 12
        h=part['depth']/2
        annulus(-h,-h+.024,0,n); annulus(h-.024,h,0,n)
        for z in (-.060,0,.060): annulus(z-.006,z+.006,0,n)
        for i in range(0,n,3):
            annulus(-h+.022,h-.022,i,2)
        return verts,faces
    raise ValueError('Unknown authored primitive: '+shape)


def blender_run(model_id, parameters, spec=None, meta=None):
    import bpy
    import bmesh
    from mathutils import Vector
    from bpy_extras.object_utils import world_to_camera_view
    if bpy.app.version[:2] != (5,2):
        raise RuntimeError('This approved batch requires installed Blender 5.2')
    parser=argparse.ArgumentParser()
    parser.add_argument('--out',type=Path,required=True)
    parser.add_argument('--preview-dir',type=Path)
    parser.add_argument('--samples',type=int,default=64)
    args=parser.parse_args(sys.argv[sys.argv.index('--')+1:])
    if not bpy.app.background or args.samples!=64:
        raise RuntimeError('Approved art pass requires headless Blender and 64 samples')
    scene=bpy.context.scene
    for obj in list(bpy.data.objects): bpy.data.objects.remove(obj,do_unlink=True)
    scene.unit_settings.system='METRIC'
    if spec is None: spec=plan(model_id,parameters)
    if meta is None: meta=metadata(model_id)
    def vec(g): return Vector((g[0],-g[2],g[1]))
    def empty(name,at=(0,0,0),parent=None):
        obj=bpy.data.objects.new(name,None); scene.collection.objects.link(obj)
        obj.parent=parent
        obj.location=vec(at)-(parent.matrix_world.translation if parent else Vector())
        bpy.context.view_layer.update()
        return obj
    mats={}
    for name,color in MATERIAL_RGBA.items():
        mat=bpy.data.materials.new(name); mat.diffuse_color=color
        bsdf=mat.node_tree.nodes['Principled BSDF']
        bsdf.inputs['Base Color'].default_value=color
        bsdf.inputs['Roughness'].default_value=.32 if name=='glass' else .58
        mats[name]=mat
    grip=empty('grip'); grip['asset_id']=model_id; grip['units']='metres'
    grip['asset_kind']=meta.get('asset_kind','gun')
    if meta.get('asset_kind','gun')=='gun': grip['bore']='-Z'
    if 'sights' in spec: grip['functional_sights']=spec['sights']
    fixed={name:empty(name,data['at'],grip) for name,data in spec['nodes'].items()
           if name in ('muzzle','fx_muzzle','fx_laser','sight_front','sight_rear','grip_two','limit_recoil')}
    lod_objects,semantic={},{}
    for lod in (0,1):
        group=empty('LOD'+str(lod),parent=grip); group['lod']=lod; group['distance_m']=0 if lod==0 else 2
        objects={}; pending={n:d for n,d in spec['nodes'].items() if n not in fixed}
        while pending:
            progressed=False
            for name,data in list(pending.items()):
                if data['parent'] and data['parent'] not in objects: continue
                obj=empty(name+('_lod1' if lod else ''),data['at'],objects.get(data['parent'],group))
                obj['semantic_node']=name
                if name=='slide_recoil': obj['limit_recoil']=.015
                objects[name]=obj
                if not lod: semantic[name]=obj
                del pending[name]; progressed=True
            if not progressed: raise ValueError('Unresolved authored node parents')
        meshes=[]
        for part in spec['parts']:
            verts,faces=geometry(part,lod)
            mesh=bpy.data.meshes.new(part['name']+'_lod'+str(lod))
            mesh.from_pydata([vec(v) for v in verts],[],faces); mesh.validate(); mesh.update()
            bm=bmesh.new(); bm.from_mesh(mesh); bmesh.ops.recalc_face_normals(bm,faces=list(bm.faces))
            bm.to_mesh(mesh); bm.free(); mesh.update()
            if part['shape'] in ('loft','sweep','zloft'):
                for polygon in mesh.polygons: polygon.use_smooth=len(polygon.vertices)==4
            obj=bpy.data.objects.new(mesh.name,mesh); scene.collection.objects.link(obj)
            parent=objects.get(part['parent'],group); obj.parent=parent
            obj.location=vec(part['at'])-parent.matrix_world.translation
            obj.data.materials.append(mats[part['material']])
            # Preserve the hand-rounded silhouette at distance; secondary details
            # use their authored coarse edges instead of spending LOD1 on bevels.
            bevel=part['bevel'] if not lod or part['name'] in ('frame','receiver','barrel_shell','hand_grip') else 0
            if bevel:
                modifier=obj.modifiers.new('rounded_edges','BEVEL'); modifier.width=bevel
                modifier.segments=2 if not lod else 1; modifier.limit_method='ANGLE'
            meshes.append(obj)
        lod_objects[lod]=meshes
    bpy.context.view_layer.update()
    args.out.parent.mkdir(parents=True,exist_ok=True)
    bpy.ops.object.select_all(action='SELECT')
    bpy.ops.export_scene.gltf(filepath=str(args.out.resolve()),export_format='GLB',use_selection=True,
                              export_yup=True,export_apply=True,export_extras=True,
                              export_cameras=False,export_lights=False,export_animations=False,export_texcoords=False)
    if args.preview_dir:
        args.preview_dir.mkdir(parents=True,exist_ok=True)
        for obj in lod_objects[1]: obj.hide_render=True
        for name,color in preview_material_rgba(meta['tints'][meta['tints']['p1']]).items():
            runtime_preview_material(mats[name],color)
        scene.render.engine='CYCLES'; scene.cycles.device='CPU'; scene.cycles.samples=64
        scene.cycles.use_denoising=True; scene.cycles.denoiser='OPENIMAGEDENOISE'
        scene.cycles.denoising_use_gpu=False; scene.cycles.preview_denoising_use_gpu=False
        scene.render.resolution_x=scene.render.resolution_y=512; scene.render.resolution_percentage=100
        scene.render.image_settings.file_format='PNG'; scene.render.film_transparent=False
        scene.view_settings.view_transform='Standard'; scene.view_settings.look='None'
        exposure=0
        scene.view_settings.exposure=exposure; scene.view_settings.gamma=1
        scene.world.use_nodes=True
        background=scene.world.node_tree.nodes['Background']
        background.inputs['Color'].default_value=(.04,.04,.04,1); background.inputs['Strength'].default_value=1
        points=[obj.matrix_world@Vector(v) for obj in lod_objects[0] for v in obj.bound_box]
        lo=Vector(tuple(min(p[i] for p in points) for i in range(3)))
        hi=Vector(tuple(max(p[i] for p in points) for i in range(3)))
        target=(lo+hi)/2; length=spec['length_m']
        camera_data=bpy.data.cameras.new('preview_camera'); camera=bpy.data.objects.new('preview_camera',camera_data)
        scene.collection.objects.link(camera); scene.camera=camera; camera_data.type='ORTHO'
        camera_data.ortho_scale=max(length,hi.z-lo.z,hi.x-lo.x)*1.38
        callouts=dict(version='0.1',model=model_id,size=[512,512],views={},
                      render=dict(device='CPU',samples=64,denoiser='OPENIMAGEDENOISE',denoising_use_gpu=False,
                                  view_transform='Standard',exposure=exposure,lighting=RUNTIME_LIGHTING),silhouette=model_id+'-silhouette.png')
        # Side: -X makes -Z bore read left. Held: behind (+Z) and above in the
        # model's bore frame; no grip-pose orientation is baked into this view.
        for view,offset in {'front':(-3,0,0),'threequarter':(-2.1,2.2,1.25),'held':(-.65,-3,1.5)}.items():
            camera.location=target+Vector(offset)*length
            camera.rotation_euler=(target-camera.location).to_track_quat('-Z','Y').to_euler()
            bpy.context.view_layer.update()
            filename=model_id+({'front':'-front','threequarter':'','held':'-held'}[view])+'.png'
            scene.render.filepath=str((args.preview_dir/filename).resolve()); bpy.ops.render.render(write_still=True)
            anchors={}
            for name,obj in {**fixed,**semantic,'grip':grip}.items():
                screen=world_to_camera_view(scene,camera,obj.matrix_world.translation)
                anchors[name]=[round(screen.x,6),round(1-screen.y,6)]
            callouts['views'][view]=dict(image=filename,nodes=anchors)
        callouts['asset_kind']=meta.get('asset_kind','gun')
        callouts['sight_reviews']={}
        camera_data.ortho_scale=.035
        for lod in ((0,1) if 'sights' in spec else ()):
            for group,objects in lod_objects.items():
                for obj in objects: obj.hide_render=group!=lod
            rear=fixed['sight_rear'].matrix_world.translation
            camera.location=rear+vec((0,0,.12))
            target=rear+vec((0,0,-.10))
            camera.rotation_euler=(target-camera.location).to_track_quat('-Z','Y').to_euler()
            bpy.context.view_layer.update()
            filename=model_id+'-sight-lod'+str(lod)+'.png'
            scene.render.filepath=str((args.preview_dir/filename).resolve()); bpy.ops.render.render(write_still=True)
            callouts['sight_reviews']['LOD'+str(lod)]=filename
        for obj in lod_objects[0]: obj.hide_render=False
        for obj in lod_objects[1]: obj.hide_render=True
        # Uniform metric scale across all seven, white background, black unlit
        # material. This is an actual side projection, not a scaled thumbnail.
        black=bpy.data.materials.new('silhouette_black')
        black.node_tree.nodes.clear(); output=black.node_tree.nodes.new('ShaderNodeOutputMaterial')
        emission=black.node_tree.nodes.new('ShaderNodeEmission'); emission.inputs['Color'].default_value=(0,0,0,1)
        black.node_tree.links.new(emission.outputs[0],output.inputs['Surface'])
        for obj in lod_objects[0]: obj.data.materials[0]=black
        background.inputs['Color'].default_value=(1,1,1,1); background.inputs['Strength'].default_value=1
        scene.view_settings.exposure=0; camera_data.ortho_scale=.82
        camera.location=target+Vector((-3,0,0))*length
        camera.rotation_euler=(target-camera.location).to_track_quat('-Z','Y').to_euler()
        scene.render.filepath=str((args.preview_dir/callouts['silhouette']).resolve())
        bpy.ops.render.render(write_still=True)
        callouts['silhouette_scale_m']=.82
        (args.preview_dir/(model_id+'.json')).write_text(json.dumps(callouts,indent=2)+'\n',encoding='utf-8')
    print('RESULT '+json.dumps(dict(model=model_id,blender=bpy.app.version_string,render_device='CPU',
                                    denoising_device='CPU',samples=64)),flush=True)


def run(model_id,parameters):
    blender_run(model_id,parameters)
