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
MATERIAL_RGBA = {'body':(.55,.55,.55,1), 'accent':(.32,.32,.34,1),
                 'dark':(.012,.012,.018,1), 'glass':(.025,.10,.16,1)}


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


def plan(model_id, parameters):
    """Pure-Python blockout plan, independent of bpy; all shapes authored here."""
    meta = metadata(model_id)
    length = parameters['length_mm'] / 1000
    mounted = meta['hold'] == 'mounted'
    parts, nodes = [], {}

    def node(name, at, parent=None):
        nodes[name] = {'at': at, 'parent': parent}

    def box(name, at, size, material='body', parent=None, bevel=.002, tilt=0):
        parts.append(dict(name=name, shape='box', at=at, size=size,
                          material=material, parent=parent, bevel=bevel, tilt=tilt))

    def cylinder(name, at, radius, depth, material='body', parent=None, axis='z'):
        parts.append(dict(name=name, shape='cylinder', at=at, radius=radius,
                          depth=depth, material=material, parent=parent, axis=axis,
                          bevel=.001, tilt=0))

    # The grip frame is the web-of-hand anchor. Markers never inherit visual recoil.
    muzzle_z = -length * (.88 if mounted else .79)
    rear_z = length + muzzle_z
    bore_y = length * (.12 if mounted else .15)
    node('muzzle', (0, bore_y, muzzle_z))
    node('fx_muzzle', (0, bore_y, muzzle_z))
    node('fx_laser', (0, bore_y, muzzle_z))
    node('sight_front', (0, bore_y + length*.11, muzzle_z + length*.12))
    node('sight_rear', (0, bore_y + length*.11, rear_z - length*.10))
    node('pivot_trigger', (length*.23, length*.04, length*.07) if mounted else (0, -length*.015, -length*.12), 'pivot_pitch' if mounted else None)
    if mounted:
        node('pivot_yaw', (0, -length*.16, -length*.36))
        node('pivot_pitch', (0, 0, -length*.36), 'pivot_yaw')
        node('grip_two', (-length*.23, 0, length*.07))
        body_parent = 'pivot_pitch'
        box('receiver', (0, bore_y, -length*.40), (length*.24, length*.25, length*.60), parent=body_parent, bevel=.010)
        cylinder('barrel_shroud', (0, bore_y, -length*.76), length*.105, length*.24, 'accent', body_parent)
        box('yoke_base', (0, -length*.21, -length*.36), (length*.45, length*.08, length*.18), 'dark')
        for side in (-1, 1):
            box('yoke_arm_'+str(side), (side*length*.19, -length*.08, -length*.36), (length*.055, length*.23, length*.10), 'dark', 'pivot_yaw')
            box('rear_handle_'+str(side), (side*length*.23, -length*.005, length*.07), (length*.065, length*.20, length*.14), 'dark', body_parent, bevel=.009)
        box('handle_bridge', (0, bore_y*.5, length*.05), (length*.50, length*.06, length*.09), parent=body_parent)
        box('top_handle', (0, bore_y+length*.17, -length*.37), (length*.09, length*.055, length*.29), 'accent', body_parent)
        # Rear end stops at target overall length.
        box('rear_cap', (0, bore_y, rear_z-length*.04), (length*.21, length*.19, length*.08), parent=body_parent)
    else:
        slim = model_id == 'con-pistol-slim'
        revolver = model_id in ('arc-pistol-twin', 'con-revolver-chunky')
        width = parameters['barrel_width_mm']/1000
        body_parent = None
        box('frame', (0, bore_y*.40, -length*.18), (width*.95, length*.15, length*.55), bevel=.004)
        box('hand_grip', (0, -length*.19, length*.035), (width*1.05, length*.42, length*.22), 'dark', bevel=.005,
            tilt=math.radians(parameters['grip_angle_deg']))
        if 'recoil_slide' in meta['features']:
            node('slide_recoil', (0, bore_y, -length*.24))
            node('limit_recoil', (0, bore_y, -length*.24+.015))
            body_parent = 'slide_recoil'
        elif 'recoil_kick' in meta['features']:
            node('visual_kick', (0, 0, 0))
            body_parent = 'visual_kick'
            # Everything visible follows the kick, not the muzzle anchor.
            for part in parts:
                part['parent'] = body_parent
            nodes['pivot_trigger']['parent'] = body_parent
        # Grip furniture belongs to the frame, not the blowback slide. Whole-body
        # visual kick still carries it; static aim/reference markers never do.
        frame_parent = 'visual_kick' if 'recoil_kick' in meta['features'] else None
        barrel_depth = length * (.68 if slim else .73)
        box('barrel_shell', (0, bore_y, muzzle_z+barrel_depth*.5), (width, length*(.16 if slim else .22), barrel_depth),
            parent=body_parent, bevel=.003 if slim else .005)
        box('rear_shell', (0, bore_y*.85, rear_z-length*.07), (width*.95, length*.17, length*.14), parent=body_parent, bevel=.004)
        if revolver:
            cylinder('fixed_cylinder', (0, bore_y*.70, -length*.10), length*.125, length*.20, 'accent', body_parent)
        # A genuinely open trigger guard, assembled from three independent rails.
        for side in (-1, 1):
            box('guard_post_'+str(side), (0, -length*.12, -length*(.14+side*.115)), (width*.35, length*.20, length*.035), parent=frame_parent, bevel=.002)
        box('guard_lower', (0, -length*.205, -length*.14), (width*.35, length*.035, length*.265), parent=frame_parent, bevel=.002)
        cylinder('cable_boss', (0, -length*.43, length*.09), length*.04, length*.06, 'accent', frame_parent, axis='y')
        for i in range(3):
            box('grip_rib_'+str(i), (0, -length*(.14+i*.065), length*.13), (width*1.08, length*.014, length*.08), 'accent', frame_parent, bevel=.001)
    # Trigger is a mesh below its real pivot, so rotation moves the visible part.
    trigger = nodes['pivot_trigger']['at']
    box('trigger', (trigger[0], trigger[1]-length*.045, trigger[2]-length*.01),
        (length*.028, length*.09, length*.035), 'accent', 'pivot_trigger', bevel=.001)
    cylinder('muzzle_lens', (0, bore_y, muzzle_z+length*.008), length*(.035 if mounted else .040), length*.015, 'glass', body_parent)
    for name in ('sight_front', 'sight_rear'):
        at = nodes[name]['at']
        # Native decoding reserves sight_* for static references, including
        # descendant meshes. Visible geometry may recoil; marker names do not.
        visible_name = 'front_sight_geometry' if name=='sight_front' else 'rear_sight_geometry'
        box(visible_name, at, (length*.025, length*.025, length*.032), 'dark', body_parent, bevel=.001)
    for button in meta.get('button', []):
        bid = button['id']
        if mounted:
            at = (length*.15, bore_y*.70, -length*.09)
        elif model_id == 'con-pistol-dpad':
            at = {'a': (-length*.13, bore_y*.05, -length*.14),
                  'b': (length*.13, bore_y*.05, -length*.14),
                  'c': (0, -length*.39, length*.06),
                  'select': (-length*.13, bore_y*.72, -length*.32),
                  'start': (-length*.13, bore_y*.72, -length*.46),
                  'dpad': (0, bore_y*.85, rear_z)}[bid]
        elif model_id == 'con-pistol-slim':
            at = ((-1 if bid == 'a' else 1)*length*.105, bore_y*.20, -length*.34)
        else:
            at = (-length*.15, bore_y*.7, -length*.17)
        name = button['node']
        node(name, at, 'pivot_pitch' if mounted else body_parent)
        if bid == 'dpad':
            box(name+'_vertical', at, (length*.035, length*.14, length*.025), 'accent', name, bevel=.001)
            box(name+'_horizontal', at, (length*.14, length*.035, length*.025), 'accent', name, bevel=.001)
        else:
            cylinder(name+'_cap', at, length*.033, length*.024, 'accent', name, axis='x' if at[0] else 'y')
    if mounted:
        # Anchor the right rear handle at the controller, not the yoke centre.
        for data in [*nodes.values(), *parts]:
            x,y,z = data['at']
            data['at'] = (x-length*.23,y,z)
    return {'id': model_id, 'length_m': length, 'nodes': nodes, 'parts': parts}


def blender_run(model_id, parameters):
    import bpy
    from mathutils import Vector, Matrix
    from bpy_extras.object_utils import world_to_camera_view

    parser = argparse.ArgumentParser()
    parser.add_argument('--out', type=Path, required=True)
    parser.add_argument('--preview-dir', type=Path)
    parser.add_argument('--samples', type=int, default=16)
    args = parser.parse_args(sys.argv[sys.argv.index('--')+1:])
    if not bpy.app.background:
        raise RuntimeError('This builder is headless-only')
    if args.samples < 1 or args.samples > 64:
        raise ValueError('Preview samples must be in 1..64')
    scene = bpy.context.scene
    for obj in list(bpy.data.objects):
        bpy.data.objects.remove(obj, do_unlink=True)
    scene.unit_settings.system = 'METRIC'
    spec, meta = plan(model_id, parameters), metadata(model_id)
    def vec(g):
        return Vector((g[0], -g[2], g[1]))
    def empty(name, at=(0,0,0), parent=None):
        obj = bpy.data.objects.new(name, None)
        scene.collection.objects.link(obj)
        if parent:
            obj.parent = parent
            obj.location = vec(at) - parent.matrix_world.translation
        else:
            obj.location = vec(at)
        bpy.context.view_layer.update()
        return obj
    mats = {}
    for name, color in MATERIAL_RGBA.items():
        mat = bpy.data.materials.new(name)
        mat.diffuse_color = color
        bsdf = mat.node_tree.nodes.get('Principled BSDF')
        bsdf.inputs['Base Color'].default_value = color
        bsdf.inputs['Roughness'].default_value = .28 if name == 'glass' else .42
        mats[name] = mat
    grip = empty('grip')
    grip['asset_id'], grip['units'], grip['bore'] = model_id, 'metres', '-Z'
    # +Y glTF is Blender +Z, and -Z glTF is Blender +Y.
    fixed = {name: empty(name, data['at'], grip) for name, data in spec['nodes'].items()
             if name in ('muzzle','fx_muzzle','fx_laser','sight_front','sight_rear','grip_two','limit_recoil')}
    lod_objects, semantic = {}, {}
    for lod in (0, 1):
        group = empty('LOD'+str(lod), parent=grip)
        group['lod'], group['distance_m'] = lod, (0 if lod == 0 else 2)
        objects, pending = {}, {name:data for name,data in spec['nodes'].items() if name not in fixed}
        while pending:
            progressed = False
            for name, data in list(pending.items()):
                if data['parent'] and data['parent'] not in objects:
                    continue
                obj = empty(name if lod == 0 else name+'_lod1', data['at'], objects.get(data['parent'], group))
                obj['semantic_node'] = name
                if name == 'slide_recoil':
                    obj['limit_recoil'] = .015
                objects[name] = obj
                if lod == 0:
                    semantic[name] = obj
                del pending[name]
                progressed = True
            if not progressed:
                raise ValueError('Cyclic model plan')
        meshes = []
        for part in spec['parts']:
            vertices, faces = [], []
            if part['shape'] == 'box':
                x,y,z = (s/2 for s in part['size'])
                vertices = [(-x,-y,-z),(x,-y,-z),(x,y,-z),(-x,y,-z),
                            (-x,-y,z),(x,-y,z),(x,y,z),(-x,y,z)]
                faces = [(0,3,2,1),(4,5,6,7),(0,1,5,4),(1,2,6,5),(2,3,7,6),(3,0,4,7)]
            else:
                count = 24 if lod == 0 else 8
                for side in (-1,1):
                    for i in range(count):
                        a = i*2*math.pi/count
                        xyz = (part['radius']*math.cos(a),part['radius']*math.sin(a),side*part['depth']/2)
                        if part['axis'] == 'x':
                            xyz = (xyz[2],xyz[0],xyz[1])
                        elif part['axis'] == 'y':
                            xyz = (xyz[1],xyz[2],xyz[0])
                        vertices.append(xyz)
                faces = [tuple(reversed(range(count))),tuple(range(count,count*2))]
                faces += [(i,(i+1)%count,(i+1)%count+count,i+count) for i in range(count)]
            mesh = bpy.data.meshes.new(part['name']+'_lod'+str(lod))
            mesh.from_pydata([vec(v) for v in vertices], [], faces)
            mesh.validate()
            mesh.update()
            obj = bpy.data.objects.new(mesh.name, mesh)
            scene.collection.objects.link(obj)
            parent = objects.get(part['parent'], group)
            obj.parent = parent
            obj.location = vec(part['at']) - parent.matrix_world.translation
            if part['tilt']:
                obj.rotation_euler.x = part['tilt']
            obj.data.materials.append(mats[part['material']])
            if part['bevel']:
                modifier = obj.modifiers.new('rounded_edges', 'BEVEL')
                modifier.width = part['bevel']
                modifier.segments = 2 if lod == 0 else 1
                modifier.limit_method = 'ANGLE'
            meshes.append(obj)
        lod_objects[lod] = (group, meshes)
    bpy.context.view_layer.update()
    args.out.parent.mkdir(parents=True, exist_ok=True)
    # Export only the authored hierarchy, before adding preview cameras/lights.
    bpy.ops.object.select_all(action='DESELECT')
    for obj in list(bpy.data.objects):
        obj.select_set(True)
    bpy.ops.export_scene.gltf(filepath=str(args.out.resolve()), export_format='GLB',
                              use_selection=True, export_yup=True, export_apply=True,
                              export_extras=True, export_cameras=False, export_lights=False,
                              export_animations=False, export_texcoords=False)
    if args.preview_dir:
        args.preview_dir.mkdir(parents=True, exist_ok=True)
        for obj in lod_objects[1][1]:
            obj.hide_render = True
        tint = meta['tints'][meta['tints']['p1']]
        for name, color in preview_material_rgba(tint).items():
            mats[name].diffuse_color = color
            mats[name].node_tree.nodes['Principled BSDF'].inputs['Base Color'].default_value = color
        scene.render.engine = 'CYCLES'
        scene.cycles.device = 'CPU'
        scene.cycles.samples = args.samples
        scene.cycles.use_denoising = False
        scene.render.resolution_x = scene.render.resolution_y = 512
        scene.render.resolution_percentage = 100
        scene.render.image_settings.file_format = 'PNG'
        scene.render.film_transparent = False
        scene.view_settings.view_transform = 'Standard'
        scene.world.color = (.12,.12,.12)
        length = spec['length_m']
        target = vec((0,-length*.06,-length*.28))
        for name, location, energy, size in (
                ('key', (1.5, -1.2, 2.4), 130, 1.3),
                ('fill', (-1.7,-.7,1), 90, 1.5),
                ('rim', (.3,1.8,2), 160, 1.0)):
            light = bpy.data.lights.new(name, 'AREA')
            light.energy, light.shape, light.size = energy, 'DISK', size
            obj = bpy.data.objects.new(name, light)
            scene.collection.objects.link(obj)
            obj.location = target + Vector(location)*length*2
            obj.rotation_euler = (target-obj.location).to_track_quat('-Z','Y').to_euler()
        camera_data = bpy.data.cameras.new('preview_camera')
        camera = bpy.data.objects.new('preview_camera',camera_data)
        scene.collection.objects.link(camera)
        scene.camera = camera
        camera_data.type, camera_data.ortho_scale = 'ORTHO', length*1.38
        callouts = {'version':'0.1','model':model_id,'size':[512,512], 'views':{}}
        # Front is an explanatory left-side elevation, not an end-on barrel view.
        for view, offset in {'front':(-2.3,0,.12),'threequarter':(-1.9,2.3,1.3)}.items():
            camera.location = target+Vector(offset)*length
            camera.rotation_euler = (target-camera.location).to_track_quat('-Z','Y').to_euler()
            bpy.context.view_layer.update()
            filename = model_id+('-front' if view == 'front' else '')+'.png'
            scene.render.filepath = str((args.preview_dir/filename).resolve())
            bpy.ops.render.render(write_still=True)
            anchors = {}
            for name,obj in {**fixed, **semantic, 'grip':grip}.items():
                screen = world_to_camera_view(scene,camera,obj.matrix_world.translation)
                anchors[name] = [round(screen.x,6),round(1-screen.y,6)]
            callouts['views'][view] = {'image':filename,'nodes':anchors}
        (args.preview_dir/(model_id+'.json')).write_text(json.dumps(callouts,indent=2)+'\n',encoding='utf-8')
    print('RESULT '+json.dumps({'model':model_id,'blender':bpy.app.version_string,
                              'out':str(args.out.resolve()),'render_device':'CPU' if args.preview_dir else None}))


def run(model_id, parameters):
    blender_run(model_id, parameters)
