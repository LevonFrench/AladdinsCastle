"""arc-light-gun: the arcade light gun. GPL-3.0-only script; the built asset is CC BY 4.0.

Unlike our other guns this one is not an original design. It is the gun hotd2-vr draws for
Confidential Mission and The House of the Dead 2, "Namco Arcade Gun" by Martoscar
(https://sketchfab.com/3d-models/namco-arcade-gun-15fbd5b9add94a34b5c21746e3dd32be),
licensed CC BY 4.0 (https://creativecommons.org/licenses/by/4.0/). The owner asked for it.
The script reads hotd2-vr's baked copy (core/rend/vr/gun_model.h at the commit pinned in
setups/flycast-hotd2-vr/upstream-pin.json) and changes it (arc_light_gun_mesh.py):
the maker's name engraved on the right side is removed and the side refilled, the upper
housing is split off as a blowback slide (cut sections capped, a cover plate under its
front), the trigger becomes its own part, the original sights are replaced by our notch and
blade sights, it is moved into our gun frame, decimated into two LODs and given our four
neutral materials. No logo or lettering remains on it.

Build (owner-approved CPU Blender for gun models):
  blender --background --factory-startup --python-exit-code 1 --threads 4 --python arc-light-gun.py --
      --source-header <hotd2-vr>/core/rend/vr/gun_model.h --out <abs>/arc-light-gun.glb --preview-dir <abs>/preview
"""
from pathlib import Path
import sys

PARAMETERS = dict(length_mm=248)
# Original model's header as pinned (hotd2-vr f0c7a2f, core/rend/vr/gun_model.h).
SOURCE_SHA256 = '4d987d81d7dc5eb32193b6da122b6a035af5977ac6b0920ec70a21b82e95390f'
# Anchors in metres (glTF frame: origin at the web of the hand, -Z bore, +Y up). The build
# recomputes them from the mesh and stops if they differ.
ANCHORS = dict(
    muzzle=(0.0, 0.0061, -0.2151),          # centre of the front lens: the aim ray starts here
    sight_front=(0.0, 0.0532, -0.2075),
    sight_rear=(0.0, 0.0532, -0.02055),
    pivot_trigger=(0.0, -0.0047, -0.0715),
    slide_recoil=(0.0, 0.0345, -0.105),
    limit_recoil=(0.0, 0.0345, -0.090),
)
LOD_RATIO = {0: 0.43, 1: 0.085}             # collapse ratio for the large shells
SMALL = ('front_sight_geometry', 'rear_sight_geometry', 'slide_cut_cap', 'frame_cut_cap', 'frame_top_cover')


class _Plan:
    """Pure-Python plan for tools/check_gun_assets.py (no bpy, no source header)."""
    def __init__(self):
        self.nodes = {n: dict(at=tuple(a), parent=None) for n, a in ANCHORS.items()}
        for n in ('fx_muzzle', 'fx_laser'):
            self.nodes[n] = dict(at=ANCHORS['muzzle'], parent=None)
        self.parts = []
        self.sight_setup = dict(kind='notch-blade', notch_width_m=.006, notch_depth_m=.003, front_width_m=.003)

    def result(self, mid):
        return dict(id=mid, length_m=PARAMETERS['length_mm'] / 1000, nodes=self.nodes, parts=self.parts,
                    sights=self.sight_setup)


def make_design():
    return _Plan()


def build():
    import argparse
    import hashlib
    import json
    import math
    import bpy
    import bmesh
    import numpy as np
    from mathutils import Vector
    from bpy_extras.object_utils import world_to_camera_view
    here = Path(__file__).resolve().parent
    sys.path.insert(0, str(here))
    import arc_light_gun_mesh as G
    from common import (RUNTIME_LIGHTING, MATERIAL_RGBA, metadata, preview_material_rgba,
                        runtime_preview_material)
    if bpy.app.version[:2] != (5, 2):
        raise RuntimeError('This build requires Blender 5.2')
    parser = argparse.ArgumentParser()
    parser.add_argument('--source-header', type=Path, required=True)
    parser.add_argument('--out', type=Path, required=True)
    parser.add_argument('--preview-dir', type=Path)
    parser.add_argument('--samples', type=int, default=64)
    args = parser.parse_args(sys.argv[sys.argv.index('--') + 1:])
    if not bpy.app.background or args.samples != 64:
        raise RuntimeError('Headless Blender and 64 samples required')
    raw = args.source_header.read_bytes()
    if hashlib.sha256(raw).hexdigest() != SOURCE_SHA256:
        raise RuntimeError('Source header differs from the pinned hotd2-vr gun_model.h')
    mesh = G.parse_header(raw.decode('utf-8'))
    clean, report = G.remove_lettering(mesh)
    parts, anchors = G.assemble(clean)
    for name, at in ANCHORS.items():
        if np.abs(np.array(at) - anchors[name]).max() > 6e-5:
            raise RuntimeError(f'anchor {name} moved: {anchors[name]} vs {at}')
    meta = metadata('arc-light-gun')
    scene = bpy.context.scene
    for obj in list(bpy.data.objects):
        bpy.data.objects.remove(obj, do_unlink=True)

    def vec(g):
        return Vector((g[0], -g[2], g[1]))

    def empty(name, at=(0, 0, 0), parent=None):
        obj = bpy.data.objects.new(name, None)
        scene.collection.objects.link(obj)
        obj.parent = parent
        obj.location = vec(at) - (parent.matrix_world.translation if parent else Vector())
        bpy.context.view_layer.update()
        return obj
    mats = {}
    for name, color in MATERIAL_RGBA.items():
        mat = bpy.data.materials.new(name)
        mat.diffuse_color = color
        bsdf = next(n for n in mat.node_tree.nodes if n.type == 'BSDF_PRINCIPLED')
        bsdf.inputs['Base Color'].default_value = color
        bsdf.inputs['Roughness'].default_value = .32 if name == 'glass' else .58
        mats[name] = mat
    grip = empty('grip')
    grip['asset_id'] = 'arc-light-gun'
    grip['units'] = 'metres'
    grip['asset_kind'] = 'gun'
    grip['bore'] = '-Z'
    grip['functional_sights'] = dict(kind='notch-blade', notch_width_m=.006, notch_depth_m=.003, front_width_m=.003)
    grip['licence'] = 'CC-BY-4.0'
    grip['attribution'] = ('"Namco Arcade Gun" by Martoscar, https://sketchfab.com/3d-models/'
                           'namco-arcade-gun-15fbd5b9add94a34b5c21746e3dd32be, CC BY 4.0; changed by AladdinsCastle '
                           '(lettering removed, slide and trigger split, sights replaced, re-oriented, decimated, recoloured)')
    fixed = {}
    for name in ('muzzle', 'fx_muzzle', 'fx_laser', 'sight_front', 'sight_rear', 'limit_recoil'):
        fixed[name] = empty(name, ANCHORS['muzzle' if name.startswith('fx_') else name], grip)
    lod_objects, semantic, counts = {}, {}, {}
    for lod in (0, 1):
        suffix = '_lod1' if lod else ''
        group = empty('LOD' + str(lod), parent=grip)
        group['lod'] = lod
        group['distance_m'] = 0 if lod == 0 else 2
        movers = {}
        for name in ('slide_recoil', 'pivot_trigger'):
            obj = empty(name + suffix, ANCHORS[name], group)
            obj['semantic_node'] = name
            if name == 'slide_recoil':
                obj['limit_recoil'] = .015
            movers[name] = obj
            if not lod:
                semantic[name] = obj
        objs = []
        counts[lod] = {}
        for pname, part in parts.items():
            if lod == 1 and part['material'] == 'dark' and not pname.endswith('sight_geometry'):
                continue        # screws are dropped at distance
            parent = movers.get(part['parent'], group)
            origin = np.array(ANCHORS[part['parent']]) if part['parent'] else np.zeros(3)
            verts = [vec(p) for p in (part['pos'] - origin)]
            me = bpy.data.meshes.new(pname + '_lod' + str(lod))
            me.from_pydata(verts, [], [tuple(int(i) for i in t) for t in part['tri']])
            me.validate()
            bm = bmesh.new()
            bm.from_mesh(me)
            bmesh.ops.remove_doubles(bm, verts=bm.verts, dist=2e-6)
            bm.to_mesh(me)
            bm.free()
            obj = bpy.data.objects.new(pname + '_lod' + str(lod), me)
            scene.collection.objects.link(obj)
            obj.parent = parent
            obj.location = Vector()
            obj.data.materials.append(mats[part['material']])
            if pname not in SMALL and len(part['tri']) > 300:
                mod = obj.modifiers.new('decimate', 'DECIMATE')
                mod.decimate_type = 'COLLAPSE'
                mod.ratio = LOD_RATIO[lod]
                mod.use_symmetry = True
                mod.symmetry_axis = 'X'
                dg = bpy.context.evaluated_depsgraph_get()
                reduced = bpy.data.meshes.new_from_object(obj.evaluated_get(dg))
                obj.modifiers.remove(mod)
                old = obj.data
                obj.data = reduced
                bpy.data.meshes.remove(old)
            bm = bmesh.new()
            bm.from_mesh(obj.data)
            bmesh.ops.triangulate(bm, faces=bm.faces[:])
            bm.to_mesh(obj.data)
            bm.free()
            obj.data.shade_smooth()
            obj.data.set_sharp_from_angle(angle=math.radians(35))
            objs.append(obj)
            counts[lod][pname] = len(obj.data.polygons)
        lod_objects[lod] = objs
    totals = {lod: sum(c.values()) for lod, c in counts.items()}
    if totals[0] > 8000 or totals[1] > 2000:
        raise RuntimeError(f'triangle budget exceeded: {totals}')
    bpy.context.view_layer.update()
    args.out.parent.mkdir(parents=True, exist_ok=True)
    bpy.ops.object.select_all(action='SELECT')
    bpy.ops.export_scene.gltf(filepath=str(args.out.resolve()), export_format='GLB', use_selection=True,
                              export_yup=True, export_apply=True, export_extras=True,
                              export_cameras=False, export_lights=False, export_animations=False, export_texcoords=False)
    if args.preview_dir:
        preview(args, scene, mats, lod_objects, fixed, semantic, grip, meta, RUNTIME_LIGHTING,
                preview_material_rgba, runtime_preview_material, world_to_camera_view, Vector, json, bpy)
    print('RESULT ' + json.dumps(dict(model='arc-light-gun', blender=bpy.app.version_string, render_device='CPU',
                                      samples=64, triangles=totals, lettering=report)), flush=True)


def preview(args, scene, mats, lod_objects, fixed, semantic, grip, meta, RUNTIME_LIGHTING,
            preview_material_rgba, runtime_preview_material, world_to_camera_view, Vector, json, bpy):
    """Same CPU preview contract as common.blender_run, plus rest/recoil views of the slide."""
    model_id = 'arc-light-gun'
    args.preview_dir.mkdir(parents=True, exist_ok=True)
    for obj in lod_objects[1]:
        obj.hide_render = True
    for name, color in preview_material_rgba(meta['tints'][meta['tints']['p1']]).items():
        runtime_preview_material(mats[name], color)
    scene.render.engine = 'CYCLES'
    scene.cycles.device = 'CPU'
    scene.cycles.samples = 64
    scene.cycles.use_denoising = True
    scene.cycles.denoiser = 'OPENIMAGEDENOISE'
    scene.cycles.denoising_use_gpu = False
    scene.cycles.preview_denoising_use_gpu = False
    scene.render.resolution_x = scene.render.resolution_y = 512
    scene.render.resolution_percentage = 100
    scene.render.image_settings.file_format = 'PNG'
    scene.render.film_transparent = False
    scene.view_settings.view_transform = 'Standard'
    scene.view_settings.look = 'None'
    scene.view_settings.exposure = 0
    scene.view_settings.gamma = 1
    if scene.world is None:
        scene.world = bpy.data.worlds.new('World')
    scene.world.use_nodes = True
    background = next(n for n in scene.world.node_tree.nodes if n.type == 'BACKGROUND')
    background.inputs['Color'].default_value = (.04, .04, .04, 1)
    background.inputs['Strength'].default_value = 1
    points = [obj.matrix_world @ Vector(v) for obj in lod_objects[0] for v in obj.bound_box]
    lo = Vector(tuple(min(p[i] for p in points) for i in range(3)))
    hi = Vector(tuple(max(p[i] for p in points) for i in range(3)))
    target = (lo + hi) / 2
    length = 0.248
    camera_data = bpy.data.cameras.new('preview_camera')
    camera = bpy.data.objects.new('preview_camera', camera_data)
    scene.collection.objects.link(camera)
    scene.camera = camera
    camera_data.type = 'ORTHO'
    camera_data.ortho_scale = max(length, hi.z - lo.z, hi.x - lo.x) * 1.38
    callouts = dict(version='0.1', model=model_id, size=[512, 512], views={},
                    render=dict(device='CPU', samples=64, denoiser='OPENIMAGEDENOISE', denoising_use_gpu=False,
                                view_transform='Standard', exposure=0, lighting=RUNTIME_LIGHTING),
                    silhouette=model_id + '-silhouette.png')

    def shoot(filename):
        scene.render.filepath = str((args.preview_dir / filename).resolve())
        bpy.ops.render.render(write_still=True)
    for view, offset in {'front': (-3, 0, 0), 'threequarter': (-2.1, 2.2, 1.25), 'held': (-.65, -3, 1.5)}.items():
        camera.location = target + Vector(offset) * length
        camera.rotation_euler = (target - camera.location).to_track_quat('-Z', 'Y').to_euler()
        bpy.context.view_layer.update()
        filename = model_id + {'front': '-front', 'threequarter': '', 'held': '-held'}[view] + '.png'
        shoot(filename)
        anchors = {}
        for name, obj in {**fixed, **semantic, 'grip': grip}.items():
            screen = world_to_camera_view(scene, camera, obj.matrix_world.translation)
            anchors[name] = [round(screen.x, 6), round(1 - screen.y, 6)]
        callouts['views'][view] = dict(image=filename, nodes=anchors)
    # The slide at full recoil (15 mm back along +Z, Blender -Y) and the trigger pulled, three-quarter view.
    slide, trigger = semantic['slide_recoil'], semantic['pivot_trigger']
    camera.location = target + Vector((-2.1, 2.2, 1.25)) * length
    camera.rotation_euler = (target - camera.location).to_track_quat('-Z', 'Y').to_euler()
    rest_slide, rest_rot = slide.location.copy(), trigger.rotation_euler.copy()
    slide.location = rest_slide + Vector((0, -.015, 0))   # glTF +Z is Blender -Y
    trigger.rotation_euler = (rest_rot.x - 0.26, rest_rot.y, rest_rot.z)
    bpy.context.view_layer.update()
    shoot(model_id + '-recoil.png')
    camera.location = target + Vector((-3, 0, 0)) * length
    camera.rotation_euler = (target - camera.location).to_track_quat('-Z', 'Y').to_euler()
    bpy.context.view_layer.update()
    shoot(model_id + '-front-recoil.png')
    slide.location = rest_slide
    trigger.rotation_euler = rest_rot
    bpy.context.view_layer.update()
    callouts['asset_kind'] = 'gun'
    callouts['extra_views'] = dict(recoil=model_id + '-recoil.png', front_recoil=model_id + '-front-recoil.png')
    callouts['sight_reviews'] = {}
    camera_data.ortho_scale = .035
    for lod in (0, 1):
        for group, objects in lod_objects.items():
            for obj in objects:
                obj.hide_render = group != lod
        rear = fixed['sight_rear'].matrix_world.translation
        camera.location = rear + Vector((0, -.12, 0))     # glTF +Z (behind the gun) is Blender -Y
        aim = rear + Vector((0, .10, 0))
        camera.rotation_euler = (aim - camera.location).to_track_quat('-Z', 'Y').to_euler()
        bpy.context.view_layer.update()
        filename = model_id + '-sight-lod' + str(lod) + '.png'
        shoot(filename)
        callouts['sight_reviews']['LOD' + str(lod)] = filename
    for obj in lod_objects[0]:
        obj.hide_render = False
    for obj in lod_objects[1]:
        obj.hide_render = True
    black = bpy.data.materials.new('silhouette_black')
    black.use_nodes = True
    black.node_tree.nodes.clear()
    output = black.node_tree.nodes.new('ShaderNodeOutputMaterial')
    emission = black.node_tree.nodes.new('ShaderNodeEmission')
    emission.inputs['Color'].default_value = (0, 0, 0, 1)
    black.node_tree.links.new(emission.outputs[0], output.inputs['Surface'])
    for obj in lod_objects[0]:
        obj.data.materials[0] = black
    background.inputs['Color'].default_value = (1, 1, 1, 1)
    camera_data.ortho_scale = .82
    camera.location = target + Vector((-3, 0, 0)) * length
    camera.rotation_euler = (target - camera.location).to_track_quat('-Z', 'Y').to_euler()
    bpy.context.view_layer.update()
    shoot(callouts['silhouette'])
    callouts['silhouette_scale_m'] = .82
    (args.preview_dir / (model_id + '.json')).write_text(json.dumps(callouts, indent=2) + '\n', encoding='utf-8')


if __name__ == '__main__':
    build()
