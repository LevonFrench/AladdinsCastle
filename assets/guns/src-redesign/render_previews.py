# SPDX-License-Identifier: GPL-3.0-only
"""Render checker-contract previews from an already-built R10-R12 GLB.

Run from Blender 5.2 in background mode with factory startup and four CPU
threads. This script imports and renders an existing GLB; it never builds or
exports a model. When installed at ``assets/guns/src-redesign/render_previews.py``,
repository and model roots are inferred from that location. In a private lane,
the repository root is inferred from the lane path and the model root defaults
to its sibling ``models`` directory. The output directory is always explicit.
Example Blender arguments follow ``--``:

    --model game-energy-gun --repo-root <repository> --models-root <models> --out-dir <preview-output>
"""
from __future__ import annotations

import argparse
import importlib.util
import json
import math
import struct
import sys
import tomllib
from pathlib import Path

import bpy
from bpy_extras.object_utils import world_to_camera_view
from mathutils import Vector


LANE = Path(__file__).resolve().parent
MODELS = ('game-energy-gun', 'game-revolver-heavy', 'game-tool-cutter')


def blender_args():
    args = sys.argv[sys.argv.index('--') + 1:] if '--' in sys.argv else []
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--model', required=True, choices=MODELS)
    parser.add_argument('--repo-root', type=Path,
                        help='repository root; inferred from this script when omitted')
    parser.add_argument('--models-root', type=Path,
                        help='directory containing the built GLB files; inferred when omitted')
    parser.add_argument('--out-dir', required=True, type=Path)
    return parser.parse_args(args)


def find_repo_root(start):
    for candidate in (start, *start.parents):
        if (candidate / 'assets' / 'guns').is_dir() and (candidate / '.git').exists():
            return candidate.resolve()
    for candidate in (start, *start.parents):
        if (candidate / 'assets' / 'guns').is_dir():
            return candidate.resolve()
    raise FileNotFoundError(f'cannot infer repository root from {start}')


def default_models_root(repo_root):
    private_lanes = (repo_root / '.local' / 'lanes').resolve()
    try:
        LANE.resolve().relative_to(private_lanes)
        return LANE / 'models'
    except ValueError:
        return repo_root / 'assets' / 'guns'


def runtime_common(repo_root):
    source = repo_root / 'assets' / 'guns' / 'src' / 'common.py'
    if not source.is_file():
        raise FileNotFoundError(f'guns worktree common.py is missing: {source}')
    spec = importlib.util.spec_from_file_location('guns_runtime_common', source)
    if spec is None or spec.loader is None:
        raise ImportError(f'cannot load preview helpers from {source}')
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


def model_path(model, models_root, repo_root):
    candidates = (models_root / model / (model + '.glb'),
                  models_root / (model + '.glb'),
                  repo_root / 'assets' / 'guns' / (model + '.glb'))
    for path in candidates:
        if path.is_file():
            return path
    raise FileNotFoundError('built GLB was not found; expected one of: ' +
                            ', '.join(str(p) for p in candidates))


def metadata_path(model, repo_root):
    path = repo_root / 'data' / 'guns' / (model + '.toml')
    if path.is_file():
        return path
    raise FileNotFoundError(f'model TOML is missing from repository data: {path}')


def descendants(root):
    found = [root]
    for child in root.children:
        found.extend(descendants(child))
    return found


def named_object(name):
    obj = bpy.data.objects.get(name)
    if obj is not None:
        return obj
    matches = [item for item in bpy.context.scene.objects
               if item.name.split('.')[0] == name]
    if len(matches) == 1:
        return matches[0]
    raise KeyError(f'GLB is missing unique object {name!r}')


def lod_objects():
    groups = {lod: named_object('LOD' + str(lod)) for lod in (0, 1)}
    meshes = {lod: [obj for obj in descendants(groups[lod])
                    if obj.type == 'MESH'] for lod in (0, 1)}
    for lod in (0, 1):
        if not meshes[lod]:
            raise ValueError(f'LOD{lod} contains no imported mesh objects')
    return groups, meshes


def bounds(meshes):
    points = [obj.matrix_world @ Vector(corner)
              for obj in meshes for corner in obj.bound_box]
    lo = Vector(tuple(min(p[i] for p in points) for i in range(3)))
    hi = Vector(tuple(max(p[i] for p in points) for i in range(3)))
    return lo, hi


def semantic_objects():
    choices = {}
    for obj in bpy.context.scene.objects:
        if obj.type != 'EMPTY' or obj.name in ('LOD0', 'LOD1'):
            continue
        name = obj.get('semantic_node') or obj.name
        if '.' in name and name.rsplit('.', 1)[1].isdigit():
            name = name.rsplit('.', 1)[0]
        if name.endswith('_lod1'):
            name = name[:-5]
        priority = 1 if obj.name.endswith('_lod1') else 0
        previous = choices.get(name)
        if previous is None or priority < previous[0]:
            choices[name] = (priority, obj)
    if 'grip' not in choices:
        grip = bpy.data.objects.get('grip')
        if grip is not None:
            choices['grip'] = (0, grip)
    return {name: value[1] for name, value in choices.items()}


def required_anchors(meta):
    controller = meta.get('asset_kind') in ('controller', 'hand')
    required = ({'grip'} if controller else
                {'grip', 'muzzle', 'sight_front', 'sight_rear', 'pivot_trigger',
                 'fx_muzzle', 'fx_laser'})
    required |= {button['node'] for button in meta.get('button', [])}
    required |= {motion['node'] for motion in meta.get('motion', {}).values()}
    dock = meta.get('assembly', {}).get('dock_node')
    if dock:
        required.add(dock)
    if meta.get('hold') == 'mounted':
        required.add('grip_two')
    return required


def visibility(meshes, lod):
    for level in (0, 1):
        for obj in meshes[level]:
            obj.hide_render = level != lod


def set_view(camera, target, offset, length):
    camera.location = target + Vector(offset) * length
    camera.rotation_euler = (target - camera.location).to_track_quat('-Z', 'Y').to_euler()
    bpy.context.view_layer.update()


def write_png(scene, path):
    scene.render.filepath = str(path.resolve())
    bpy.ops.render.render(write_still=True)
    header = path.read_bytes()
    if (header[:8] != b'\x89PNG\r\n\x1a\n' or header[12:16] != b'IHDR' or
            struct.unpack_from('>II', header, 16) != (512, 512)):
        raise ValueError(f'render did not produce a 512x512 PNG: {path}')
    return {'file': path.name, 'width': 512, 'height': 512, 'bytes': len(header)}


def screen_anchors(scene, camera, anchors):
    projected = {}
    for name, obj in anchors.items():
        p = world_to_camera_view(scene, camera, obj.matrix_world.translation)
        xy = [round(float(p.x), 6), round(float(1.0 - p.y), 6)]
        if not all(math.isfinite(v) and 0.0 <= v <= 1.0 for v in xy):
            raise ValueError(f'anchor {name!r} projects outside the preview: {xy}')
        projected[name] = xy
    return projected


def configure_render(scene):
    scene.render.engine = 'CYCLES'
    scene.cycles.device = 'CPU'
    scene.cycles.samples = 64
    scene.cycles.use_denoising = True
    scene.cycles.denoiser = 'OPENIMAGEDENOISE'
    scene.cycles.denoising_use_gpu = False
    scene.cycles.preview_denoising_use_gpu = False
    scene.render.threads_mode = 'FIXED'
    scene.render.threads = 4
    scene.render.resolution_x = scene.render.resolution_y = 512
    scene.render.resolution_percentage = 100
    scene.render.image_settings.file_format = 'PNG'
    scene.render.image_settings.color_mode = 'RGBA'
    scene.render.film_transparent = False
    scene.view_settings.view_transform = 'Standard'
    scene.view_settings.look = 'None'
    scene.view_settings.exposure = 0
    scene.view_settings.gamma = 1
    scene.world.use_nodes = True
    background = scene.world.node_tree.nodes.get('Background')
    background.inputs['Color'].default_value = (.04, .04, .04, 1)
    background.inputs['Strength'].default_value = 1


def linear_rgb(hex_color):
    result = []
    for offset in (1, 3, 5):
        value = int(hex_color[offset:offset + 2], 16) / 255.0
        result.append(value / 12.92 if value <= .04045 else ((value + .055) / 1.055) ** 2.4)
    return tuple(result)


def imported_base_color(material):
    if material.use_nodes and material.node_tree:
        for node in material.node_tree.nodes:
            if node.type == 'BSDF_PRINCIPLED' and 'Base Color' in node.inputs:
                return tuple(float(v) for v in node.inputs['Base Color'].default_value)
    return tuple(float(v) for v in material.diffuse_color)


def apply_p1_materials(common, meta):
    palette = meta['tints'][meta['tints']['p1']]
    tints = {role: linear_rgb(palette[role]) for role in ('body', 'accent')}
    seen = set()
    for obj in bpy.context.scene.objects:
        if obj.type != 'MESH':
            continue
        for material in obj.data.materials:
            if material is None or material.as_pointer() in seen:
                continue
            seen.add(material.as_pointer())
            role = material.name.split('.')[0]
            if role not in ('body', 'accent', 'dark', 'glass'):
                raise ValueError(f'unsupported GLB material role {material.name!r}')
            color = imported_base_color(material)
            if role in tints:
                color = tuple(color[i] * tints[role][i] for i in range(3)) + (color[3],)
            material.use_nodes = True
            common.runtime_preview_material(material, color)


def silhouette_material():
    material = bpy.data.materials.new('preview_silhouette_black')
    material.use_nodes = True
    tree = material.node_tree
    tree.nodes.clear()
    output = tree.nodes.new('ShaderNodeOutputMaterial')
    emission = tree.nodes.new('ShaderNodeEmission')
    emission.inputs['Color'].default_value = (0, 0, 0, 1)
    tree.links.new(emission.outputs[0], output.inputs['Surface'])
    return material


def main():
    args = blender_args()
    if bpy.app.version[:2] != (5, 2):
        raise RuntimeError(f'Blender 5.2 required, got {bpy.app.version_string}')
    if not bpy.app.background:
        raise RuntimeError('run this preview renderer in Blender background mode')

    repo_root = args.repo_root.resolve() if args.repo_root else find_repo_root(LANE)
    models_root = args.models_root.resolve() if args.models_root else default_models_root(repo_root)
    out_dir = args.out_dir.resolve()
    common = runtime_common(repo_root)
    model = model_path(args.model, models_root, repo_root)
    with metadata_path(args.model, repo_root).open('rb') as stream:
        meta = tomllib.load(stream)
    if meta['id'] != args.model:
        raise ValueError(f'TOML id mismatch: expected {args.model}, got {meta["id"]}')
    if meta.get('asset_kind', 'gun') in ('controller', 'hand'):
        raise ValueError('this renderer is scoped to the three R10-R12 guns')
    scene = bpy.context.scene
    for obj in list(bpy.data.objects):
        bpy.data.objects.remove(obj, do_unlink=True)
    bpy.ops.import_scene.gltf(filepath=str(model.resolve()))
    _groups, meshes = lod_objects()
    visibility(meshes, 0)
    bpy.context.view_layer.update()
    lo, hi = bounds(meshes[0])
    target = (lo + hi) / 2
    length = float(meta['length_mm']) / 1000.0
    if length <= 0:
        raise ValueError('TOML length_mm must be positive')

    apply_p1_materials(common, meta)
    configure_render(scene)
    camera_data = bpy.data.cameras.new('runtime_preview_camera')
    camera = bpy.data.objects.new('runtime_preview_camera', camera_data)
    scene.collection.objects.link(camera)
    scene.camera = camera
    camera_data.type = 'ORTHO'
    camera_data.ortho_scale = max(length, hi.z - lo.z, hi.x - lo.x) * 1.38

    out_dir.mkdir(parents=True, exist_ok=True)
    manifest = out_dir / (args.model + '.json')
    manifest.unlink(missing_ok=True)
    anchors = semantic_objects()
    required = required_anchors(meta)
    missing = sorted(required - set(anchors))
    if missing:
        raise ValueError('GLB lacks semantic callout anchors: ' + ', '.join(missing))
    receipts = {}
    callouts = dict(version='0.1', model=args.model, size=[512, 512], views={},
                    render=dict(device='CPU', samples=64,
                                denoiser='OPENIMAGEDENOISE', denoising_use_gpu=False,
                                view_transform='Standard', exposure=0,
                                lighting=common.RUNTIME_LIGHTING),
                    silhouette=args.model + '-silhouette.png',
                    asset_kind=meta.get('asset_kind', 'gun'), sight_reviews={})

    view_offsets = {'front': (-3, 0, 0), 'threequarter': (-2.1, 2.2, 1.25),
                    'held': (-.65, -3, 1.5)}
    view_names = {'front': args.model + '-front.png',
                  'threequarter': args.model + '.png',
                  'held': args.model + '-held.png'}
    for view, offset in view_offsets.items():
        visibility(meshes, 0)
        set_view(camera, target, offset, length)
        filename = view_names[view]
        receipts[filename] = write_png(scene, out_dir / filename)
        callouts['views'][view] = dict(image=filename,
                                       nodes=screen_anchors(scene, camera, anchors))

    rear = anchors['sight_rear'].matrix_world.translation.copy()
    front = anchors['sight_front'].matrix_world.translation.copy()
    bore = front - rear
    if bore.length < 1e-6:
        raise ValueError('front and rear sight anchors do not define a bore direction')
    bore.normalize()
    camera_data.ortho_scale = .035
    for lod in (0, 1):
        visibility(meshes, lod)
        # The front-minus-rear sight vector follows the parallel -Z bore after
        # glTF import (Blender +Y). Aim 1.5 mm below rear-sight height, through
        # the center of the 3 mm notch, with camera up aligned to Blender +Z.
        line = rear.copy()
        line.z -= .0015
        camera.location = line - bore * .12
        sight_target = line + bore * .10
        camera.rotation_euler = (sight_target - camera.location).to_track_quat('-Z', 'Z').to_euler()
        bpy.context.view_layer.update()
        filename = f'{args.model}-sight-lod{lod}.png'
        receipts[filename] = write_png(scene, out_dir / filename)
        callouts['sight_reviews']['LOD' + str(lod)] = filename

    visibility(meshes, 0)
    black = silhouette_material()
    for obj in meshes[0]:
        obj.data.materials.clear()
        obj.data.materials.append(black)
    background = scene.world.node_tree.nodes.get('Background')
    background.inputs['Color'].default_value = (1, 1, 1, 1)
    background.inputs['Strength'].default_value = 1
    camera_data.ortho_scale = .82
    set_view(camera, target, (-3, 0, 0), length)
    filename = callouts['silhouette']
    receipts[filename] = write_png(scene, out_dir / filename)
    callouts['silhouette_scale_m'] = .82

    manifest_tmp = manifest.with_suffix('.json.tmp')
    manifest_tmp.write_text(json.dumps(callouts, indent=2) + '\n', encoding='utf-8')
    manifest_tmp.replace(manifest)
    receipt = dict(model=args.model, blender=bpy.app.version_string,
                   render_device='CPU', denoising_device='CPU', samples=64,
                   model_glb=str(model), manifest=str(manifest), images=receipts)
    print('RESULT ' + json.dumps(receipt, sort_keys=True), flush=True)


if __name__ == '__main__':
    main()
