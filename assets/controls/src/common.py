"""Original cabinet controls. GPL-3.0-only scripts; built meshes CC0-1.0.

Author in metres/+Y up/-Z forward; convert to Blender only at the boundary.
No external mesh, image, font or texture is loaded.
"""
import argparse
import math
from pathlib import Path
import sys
import bpy
from mathutils import Vector

def xyz(p):
    return Vector((p[0], -p[2], p[1]))

def node(name, pos=(0,0,0), parent=None):
    obj=bpy.data.objects.new(name,None); bpy.context.collection.objects.link(obj)
    obj.location=xyz(pos); obj.parent=parent
    return obj

def materials():
    palette={'body':((.025,.075,.10,1),0,.32),
             'accent':((.43,.49,.55,1),.8,.27),
             'dark':((.017,.021,.028,1),0,.78),
             'glass':((.035,.62,.49,1),.15,.24)}
    result={}
    for name,(color,metal,rough) in palette.items():
        m=bpy.data.materials.new(name); m.diffuse_color=color; m.use_nodes=True
        p=m.node_tree.nodes.get('Principled BSDF')
        p.inputs['Base Color'].default_value=color
        p.inputs['Metallic'].default_value=metal; p.inputs['Roughness'].default_value=rough
        result[name]=m
    return result

class Parts:
    def __init__(self,root,lod,mats):
        self.root=root; self.lod=lod; self.mats=mats; self.n=24 if lod==0 else 12
    def finish(self,o,name,parent,mat,bevel=.002,smooth=False):
        o.name=name; o.parent=parent or self.root; o.data.materials.append(self.mats[mat])
        bpy.context.view_layer.objects.active=o
        bpy.ops.object.transform_apply(location=False,rotation=False,scale=True)
        if bevel:
            b=o.modifiers.new('Rounded edges','BEVEL'); b.width=bevel; b.segments=2 if self.lod==0 else 1
            bpy.ops.object.modifier_apply(modifier=b.name)
        if smooth:
            for poly in o.data.polygons: poly.use_smooth=True
        else:
            w=o.modifiers.new('Face normals','WEIGHTED_NORMAL'); w.keep_sharp=True
            bpy.ops.object.modifier_apply(modifier=w.name)
        return o
    def box(self,name,pos,size,mat='body',parent=None,bevel=.003):
        bpy.ops.mesh.primitive_cube_add(size=1,location=xyz(pos))
        o=bpy.context.object; o.scale=(size[0],size[2],size[1])
        return self.finish(o,name,parent,mat,bevel)
    def cylinder(self,name,pos,radius,length,mat='accent',parent=None,axis=(0,1,0),bevel=.0015):
        bpy.ops.mesh.primitive_cylinder_add(vertices=self.n,radius=radius,depth=length,location=xyz(pos))
        o=bpy.context.object; o.rotation_euler=xyz(axis).to_track_quat('Z','Y').to_euler()
        return self.finish(o,name,parent,mat,bevel if self.lod==0 else 0,True)
    def sphere(self,name,pos,size,mat='dark',parent=None):
        bpy.ops.mesh.primitive_uv_sphere_add(segments=self.n,ring_count=8 if self.lod==0 else 4,location=xyz(pos))
        o=bpy.context.object; o.scale=(size[0],size[2],size[1])
        return self.finish(o,name,parent,mat,0,True)
    def bar(self,name,a,b,r=.008,mat='accent',parent=None):
        a=Vector(a);b=Vector(b)
        return self.cylinder(name,(a+b)/2,r,(b-a).length,mat,parent,tuple((b-a).normalized()))
    def torus(self,name,pos,major,minor,mat='dark',parent=None,axis=(0,0,1),minor_segments=None):
        bpy.ops.mesh.primitive_torus_add(major_segments=self.n,minor_segments=minor_segments or (8 if self.lod==0 else 4),major_radius=major,minor_radius=minor,location=xyz(pos))
        o=bpy.context.object; o.rotation_euler=xyz(axis).to_track_quat('Z','Y').to_euler()
        return self.finish(o,name,parent,mat,0,True)
    def anchor(self,name,pos,parent=None):
        return node(name,pos,parent or self.root)

def reset():
    bpy.ops.object.select_all(action='SELECT');bpy.ops.object.delete(use_global=False)

def build_model(mid,build):
    root=node('control_root');mats=materials();lods=[]
    for lod in (0,1):
        group=node('LOD'+str(lod),parent=root);lods.append(group)
        build(Parts(group,lod,mats), '_lod1' if lod else '')
    return root,lods

def descendants(o):
    return [o]+[c for child in o.children for c in descendants(child)]

def export(root,out):
    bpy.ops.object.select_all(action='DESELECT')
    for o in descendants(root):o.select_set(True)
    bpy.context.view_layer.objects.active=root
    bpy.ops.export_scene.gltf(filepath=str(out),export_format='GLB',use_selection=True,export_yup=True,
                              export_apply=True,export_cameras=False,export_lights=False)

def render(root,lods,out,size=640,view=(1.4,-2.6,1.7)):
    for o in descendants(lods[1]):o.hide_render=True
    scene=bpy.context.scene;scene.render.engine='CYCLES';scene.cycles.device='CPU'
    scene.cycles.samples=32;scene.cycles.use_denoising=True
    scene.render.threads_mode='FIXED';scene.render.threads=2
    scene.render.resolution_x=size;scene.render.resolution_y=size;scene.render.resolution_percentage=100
    scene.world.color=(.12,.12,.12);scene.view_settings.view_transform='AgX'
    scene.view_settings.look='AgX - Medium High Contrast';scene.view_settings.exposure=0
    bpy.context.view_layer.update()
    pts=[o.matrix_world@Vector(v) for o in descendants(lods[0]) if o.type=='MESH' for v in o.bound_box]
    low=Vector(tuple(min(p[i] for p in pts) for i in range(3)));high=Vector(tuple(max(p[i] for p in pts) for i in range(3)))
    centre=(low+high)/2; extent=max(high-low)
    floor=bpy.data.materials.new('studio');floor.use_nodes=True
    floor.node_tree.nodes.get('Principled BSDF').inputs['Base Color'].default_value=(.065,.09,.12,1)
    floor.node_tree.nodes.get('Principled BSDF').inputs['Roughness'].default_value=.8
    bpy.ops.mesh.primitive_plane_add(size=200,location=(0,0,low.z-.015));bpy.context.object.data.materials.append(floor)
    for name,pos,energy,area in [('key',(-1,-2,3),380,3),('fill',(2,0,1),220,2),('rim',(0,2,2),420,2)]:
        l=bpy.data.lights.new(name,'AREA');l.energy=energy*extent*extent*.25;l.shape='DISK';l.size=area*extent
        o=bpy.data.objects.new(name,l);scene.collection.objects.link(o);o.location=centre+Vector(pos)*extent
        o.rotation_euler=(centre-o.location).to_track_quat('-Z','Y').to_euler()
    c=bpy.data.cameras.new('review');c.type='ORTHO';c.ortho_scale=extent*1.65
    camera=bpy.data.objects.new('review',c);scene.collection.objects.link(camera)
    camera.location=centre+Vector(view)*extent
    camera.rotation_euler=(centre-camera.location).to_track_quat('-Z','Y').to_euler();scene.camera=camera
    scene.render.filepath=str(out);scene.render.image_settings.file_format='PNG'
    bpy.ops.render.render(write_still=True)

def main(mid,build):
    p=argparse.ArgumentParser();p.add_argument('--out',type=Path,required=True);p.add_argument('--render',type=Path)
    a=p.parse_args(sys.argv[sys.argv.index('--')+1:]);reset()
    print('Blender',bpy.app.version_string,'CPU cabinet build',mid,flush=True)
    root,lods=build_model(mid,build);a.out.parent.mkdir(parents=True,exist_ok=True);export(root,a.out)
    if a.render:a.render.parent.mkdir(parents=True,exist_ok=True);render(root,lods,a.render)
    print('Finished',mid,flush=True)
