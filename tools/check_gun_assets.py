#!/usr/bin/env python3
"""Validate authored gun GLBs, motion targets and CPU callouts. Standard library only.

Missing assets fail by default. --source-only checks scripts/plans while the owner
has not approved Blender; it never counts as asset acceptance.
"""
from __future__ import annotations
import argparse
import json
import math
from pathlib import Path
import runpy
import struct
import sys
import tomllib

ROOT = Path(__file__).resolve().parents[1]
REQUIRED = {'grip','muzzle','sight_front','sight_rear','pivot_trigger','fx_muzzle','fx_laser','LOD0','LOD1'}
IDENTITY = (1,0,0,0, 0,1,0,0, 0,0,1,0, 0,0,0,1)
MOTION_DRIVES = {'trigger','recoil','pump','selector','yaw','pitch'}


def demand(condition, message):
    if not condition:
        raise ValueError(message)


def read_glb(path):
    blob = path.read_bytes()
    demand(len(blob) >= 20, 'truncated GLB header')
    magic, version, size = struct.unpack_from('<4sII',blob)
    demand(magic == b'glTF' and version == 2, 'not a glTF 2.0 binary')
    demand(size == len(blob), 'GLB declared length differs from file')
    chunks, offset = [], 12
    while offset < size:
        demand(offset+8 <= size, 'truncated chunk header')
        length, kind = struct.unpack_from('<II',blob,offset)
        offset += 8
        demand(length % 4 == 0 and offset+length <= size, 'invalid chunk length/alignment')
        chunks.append((kind,blob[offset:offset+length]))
        offset += length
    demand([c[0] for c in chunks] == [0x4e4f534a,0x004e4942], 'expected JSON then BIN, no other chunks')
    doc = json.loads(chunks[0][1].decode('utf-8'))
    demand(doc.get('asset',{}).get('version') == '2.0', 'asset.version must be 2.0')
    demand(len(doc.get('buffers',[])) == 1, 'expected one binary buffer')
    buffer = doc['buffers'][0]
    demand('uri' not in buffer, 'external/data-URI buffer forbidden')
    demand(isinstance(buffer.get('byteLength'),int) and 0 <= len(chunks[1][1])-buffer['byteLength'] <= 3,
           'binary buffer length mismatch')
    return doc, chunks[1][1][:buffer['byteLength']]


def accessor(doc, binary, index, color=False):
    demand(isinstance(index,int) and 0 <= index < len(doc.get('accessors',[])), 'invalid accessor index')
    acc = doc['accessors'][index]
    demand('sparse' not in acc, 'sparse accessors unsupported in authored tier 1')
    normalized = acc.get('normalized',False)
    demand(type(normalized) is bool, 'invalid accessor normalization')
    demand(color or not normalized, 'normalized accessors unsupported for this attribute')
    formats = {5121:('B',1),5123:('H',2),5125:('I',4),5126:('f',4)}
    sizes = {'SCALAR':1,'VEC3':3,'VEC4':4}
    demand(acc.get('componentType') in formats and acc.get('type') in sizes, 'unsupported accessor layout')
    vi = acc.get('bufferView')
    demand(isinstance(vi,int) and 0 <= vi < len(doc.get('bufferViews',[])), 'invalid bufferView index')
    view = doc['bufferViews'][vi]
    demand(view.get('buffer',0) == 0, 'bufferView references external buffer')
    code, width = formats[acc['componentType']]
    components = sizes[acc['type']]
    item_width = components*width
    stride = view.get('byteStride',item_width)
    offset, local, length, count = view.get('byteOffset',0), acc.get('byteOffset',0), view.get('byteLength'), acc.get('count')
    demand(all(isinstance(v,int) and v >= 0 for v in (offset,local,length,count,stride)), 'invalid accessor sizes')
    demand(count > 0 and stride >= item_width and stride % width == 0, 'invalid accessor stride/count')
    demand(offset+length <= len(binary) and local+(count-1)*stride+item_width <= length, 'accessor exceeds binary/view bounds')
    result = [struct.unpack_from('<'+code*components,binary,offset+local+i*stride) for i in range(count)]
    demand(all(math.isfinite(v) for row in result for v in row), 'non-finite accessor data')
    if color and acc['componentType'] != 5126:
        demand(normalized and acc['componentType'] in (5121,5123), 'vertex color integers must be normalized bytes/shorts')
        divisor = 255 if acc['componentType']==5121 else 65535
        result = [tuple(v/divisor for v in row) for row in result]
    return result


def finite_number(value):
    if type(value) not in (int,float):
        return False
    try:
        return math.isfinite(value) and abs(value)<=3.4028234663852886e38
    except OverflowError:
        return False


def rgba(value,context):
    demand(isinstance(value,(list,tuple)) and len(value)==4 and
           all(finite_number(v) and 0<=v<=1 for v in value),context+' RGBA must be four finite numbers in 0..1')


def multiply(a,b):
    return tuple(sum(a[k*4+r]*b[c*4+k] for k in range(4)) for c in range(4) for r in range(4))


def transform(matrix, point, vector=False):
    p = tuple(point)+(0 if vector else 1,)
    return tuple(sum(matrix[c*4+r]*p[c] for c in range(4)) for r in range(3))


def local_matrix(node):
    if 'matrix' in node:
        demand(not any(k in node for k in ('translation','rotation','scale')), 'matrix and TRS both specified')
        m = node['matrix']
        demand(len(m) == 16 and all(math.isfinite(v) for v in m), 'invalid node matrix')
        demand(all(abs(m[i]-v)<1e-6 for i,v in ((3,0),(7,0),(11,0),(15,1))), 'non-affine node matrix')
        return tuple(m)
    t,s,q = node.get('translation',[0,0,0]),node.get('scale',[1,1,1]),node.get('rotation',[0,0,0,1])
    demand(len(t)==3 and len(s)==3 and len(q)==4 and all(math.isfinite(v) for v in (*t,*s,*q)), 'invalid node TRS')
    demand(abs(sum(v*v for v in q)-1)<1e-4, 'non-unit quaternion')
    x,y,z,w = q
    return ((1-2*y*y-2*z*z)*s[0],(2*x*y+2*z*w)*s[0],(2*x*z-2*y*w)*s[0],0,
            (2*x*y-2*z*w)*s[1],(1-2*x*x-2*z*z)*s[1],(2*y*z+2*x*w)*s[1],0,
            (2*x*z+2*y*w)*s[2],(2*y*z-2*x*w)*s[2],(1-2*x*x-2*y*y)*s[2],0,
            *t,1)


def check_asset(path, meta):
    doc, binary = read_glb(path)
    demand(not doc.get('images') and not doc.get('textures'), 'tier 1 must have no images/textures')
    demand(not doc.get('extensionsRequired'), 'required extensions unsupported for tier 1')
    nodes = doc.get('nodes',[])
    names = [n.get('name') for n in nodes]
    demand(all(isinstance(n,str) and n for n in names) and len(set(names)) == len(names), 'missing/duplicate node names')
    needed = REQUIRED | {b['node'] for b in meta.get('button',[])}
    if 'recoil_slide' in meta['features']:
        needed |= {'slide_recoil','limit_recoil'}
    if meta['hold'] == 'mounted':
        needed |= {'pivot_yaw','pivot_pitch','grip_two'}
    demand(needed <= set(names), 'missing nodes: '+', '.join(sorted(needed-set(names))))
    expected_buttons = {b['node'] for b in meta.get('button',[])}
    actual_buttons = {n for n in names if n.startswith('button_') and not n.endswith('_lod1') and not n.endswith(('_cap_lod0','_cap_lod1','_vertical_lod0','_vertical_lod1','_horizontal_lod0','_horizontal_lod1'))}
    demand(actual_buttons == expected_buttons, 'button nodes differ from metadata')
    root = names.index('grip')
    scenes = doc.get('scenes',[])
    demand(len(scenes)==1 and doc.get('scene',0)==0 and scenes[0].get('nodes')==[root], 'expected only one scene/root: grip')
    parents, worlds, lods = {}, {}, {}
    def visit(index, world, lod=None, stack=()):
        demand(isinstance(index,int) and 0 <= index < len(nodes), 'invalid child index')
        demand(index not in stack and index not in worlds, 'cycle or multiply-parented node')
        node = nodes[index]
        worlds[index] = multiply(world,local_matrix(node))
        if node['name'] in ('LOD0','LOD1'):
            demand(lod is None, 'nested LOD groups')
            lod = node['name']
        lods[index] = lod
        for child in node.get('children',[]):
            parents[child] = index
            visit(child,worlds[index],lod,stack+(index,))
    visit(root,IDENTITY)
    demand(len(worlds)==len(nodes), 'unreachable/extra root nodes')
    demand(all(abs(a-b)<1e-6 for a,b in zip(worlds[root],IDENTITY)), 'grip must be identity at origin')
    for marker in ('muzzle','fx_muzzle','fx_laser'):
        demand(parents.get(names.index(marker))==root, marker+' must not inherit visual recoil')
    muzzle = worlds[names.index('muzzle')]
    demand(transform(muzzle,(0,0,0))[2]<-0.01, 'muzzle must be in front of grip along -Z')
    direction = transform(muzzle,(0,0,-1),vector=True)
    demand(abs(direction[0])<1e-5 and abs(direction[1])<1e-5 and abs(direction[2]+1)<1e-5, 'muzzle bore must point -Z')
    materials = doc.get('materials',[])
    demand({m.get('name') for m in materials}=={'body','accent','dark','glass'} and len(materials)==4, 'expected four named materials')
    for material in materials:
        pbr = material.get('pbrMetallicRoughness',{})
        demand(isinstance(pbr,dict), 'material PBR must be a table')
        demand(not any(key in pbr for key in ('baseColorTexture','metallicRoughnessTexture')), 'material textures unsupported')
        rgba(pbr.get('baseColorFactor',[1,1,1,1]),'material')
    totals, positions = {'LOD0':0,'LOD1':0}, {'LOD0':[],'LOD1':[]}
    used_materials, used_meshes = set(),set()
    for index,node in enumerate(nodes):
        if 'mesh' not in node:
            continue
        lod = lods[index]
        demand(lod in totals, 'geometry outside a LOD group')
        mi = node['mesh']
        demand(isinstance(mi,int) and 0 <= mi < len(doc.get('meshes',[])), 'invalid mesh index')
        used_meshes.add(mi)
        for prim in doc['meshes'][mi].get('primitives',[]):
            demand(prim.get('mode',4)==4 and not prim.get('extensions'), 'expected uncompressed triangles')
            demand(not prim.get('targets'), 'morph targets unsupported for tier 1')
            pi = prim.get('attributes',{}).get('POSITION')
            points = accessor(doc,binary,pi)
            demand(doc['accessors'][pi]['type']=='VEC3' and doc['accessors'][pi]['componentType']==5126, 'POSITION must be float VEC3')
            if 'COLOR_0' in prim.get('attributes',{}):
                ci = prim['attributes']['COLOR_0']
                demand(type(ci) is int and 0<=ci<len(doc['accessors']), 'invalid vertex color accessor')
                ca = doc['accessors'][ci]
                demand(ca.get('type') in ('VEC3','VEC4') and ca.get('count')==len(points) and
                       (ca.get('componentType')==5126 or
                        ca.get('componentType') in (5121,5123) and ca.get('normalized') is True),
                       'vertex color layout/count must match POSITION')
                for value in accessor(doc,binary,ci,color=True):
                    rgba(value if len(value)==4 else (*value,1),'vertex color')
            if 'indices' in prim:
                ia = doc['accessors'][prim['indices']]
                demand(ia['type']=='SCALAR' and ia['componentType'] in (5121,5123,5125), 'indices must be unsigned integers')
                indices = [r[0] for r in accessor(doc,binary,prim['indices'])]
                demand(all(i<len(points) for i in indices), 'index outside POSITION array')
            else:
                indices = list(range(len(points)))
            demand(len(indices)%3==0, 'triangle count is not integral')
            totals[lod] += len(indices)//3
            positions[lod].extend(transform(worlds[index],points[i]) for i in set(indices))
            material = prim.get('material')
            demand(isinstance(material,int) and 0 <= material < len(materials), 'invalid material index')
            used_materials.add(material)
    demand(used_meshes==set(range(len(doc.get('meshes',[])))), 'unreferenced meshes')
    demand(used_materials==set(range(4)), 'all material slots must be used')
    bounds = {}
    for lod,budget in (('LOD0',8000),('LOD1',2000)):
        demand(0 < totals[lod] <= budget, f'{lod} triangle budget: {totals[lod]}/{budget}')
        lo = [min(p[i] for p in positions[lod]) for i in range(3)]
        hi = [max(p[i] for p in positions[lod]) for i in range(3)]
        length = (hi[2]-lo[2])*1000
        demand(abs(length-meta['length_mm']) <= meta['length_mm']*.1,
               f'{lod} bore length {length:.2f} mm differs from target {meta["length_mm"]} mm')
        bounds[lod] = round(length,3)
    # Native model validation protects every static reference and every ancestor,
    # including the LOD1 alias. Direct-child muzzle checks alone cannot prove this.
    static_ancestors = set()
    for index,name in enumerate(names):
        if name in ('grip','grip_two','muzzle') or name.startswith(('sight_','fx_')):
            ancestor = index
            while ancestor is not None:
                static_ancestors.add(ancestor)
                ancestor = parents.get(ancestor)
    motions = meta.get('motion',{})
    demand(isinstance(motions,dict) and len(motions)<=64, 'motion must be a table with at most 64 entries')
    driven = set()
    for mid,motion in motions.items():
        demand(isinstance(mid,str) and mid and isinstance(motion,dict), 'motion id/table required')
        demand(motion.get('node') in names, f'motion {mid}: missing node')
        demand(motion.get('kind') in ('rotate','slide'), f'motion {mid}: invalid kind')
        axis,limits = motion.get('axis',[]),motion.get('range',[])
        demand(isinstance(axis,list) and len(axis)==3 and all(finite_number(v) for v in axis) and abs(sum(v*v for v in axis)-1)<1e-5, f'motion {mid}: unit axis required')
        demand(isinstance(limits,list) and len(limits)==2 and all(finite_number(v) for v in limits) and limits[0]<limits[1], f'motion {mid}: invalid range')
        drive = motion.get('drive')
        demand(isinstance(drive,str) and (drive in MOTION_DRIVES or drive.startswith('button:') and len(drive)>7), f'motion {mid}: unsupported motion drive')
        if 'duration_ms' in motion:
            duration = motion['duration_ms']
            demand(type(duration) is int and 0<duration<=10000, f'motion {mid}: duration must be an integer in 1..10000 ms')
        demand(motion.get('lod_nodes')==[motion['node'],motion['node']+'_lod1'], f'motion {mid}: explicit LOD targets required')
        for target in motion['lod_nodes']:
            demand(target in names, f'motion {mid}: missing LOD target {target}')
            index = names.index(target)
            demand(index not in static_ancestors, f'motion {mid}: motion would move a static aim/reference anchor')
            demand(index not in driven, f'motion {mid}: node has multiple motion drivers')
            driven.add(index)
        canonical = nodes[names.index(motion['node'])]
        alternate = nodes[names.index(motion['node']+'_lod1')]
        demand(canonical.get('extras',{}).get('semantic_node')==motion['node'] and
               alternate.get('extras',{}).get('semantic_node')==motion['node'], f'motion {mid}: LOD semantic alias mismatch')
    if 'recoil_slide' in meta['features']:
        slide = nodes[names.index('slide_recoil')]
        demand(slide.get('extras',{}).get('limit_recoil')==.015, 'slide_recoil must declare 15 mm travel')
        start = transform(worlds[names.index('slide_recoil')],(0,0,0))
        stop = transform(worlds[names.index('limit_recoil')],(0,0,0))
        demand(all(abs(a-b)<1e-5 for a,b in zip(stop,(start[0],start[1],start[2]+.015))), 'limit_recoil stop must be 15 mm behind slide')
    return {'triangles':totals,'length_mm':bounds,'nodes':len(nodes)}


def check_previews(directory, mid, meta):
    path = directory/(mid+'.json')
    data = json.loads(path.read_text(encoding='utf-8'))
    demand(data.get('version')=='0.1' and data.get('model')==mid and data.get('size')==[512,512], 'invalid callout manifest')
    required = {'grip','muzzle','sight_front','sight_rear','pivot_trigger','fx_muzzle','fx_laser'} | {b['node'] for b in meta.get('button',[])} | {m['node'] for m in meta.get('motion',{}).values()}
    if meta['hold']=='mounted':
        required.add('grip_two')
    demand(set(data.get('views',{}))=={'front','threequarter'}, 'both CPU preview views required')
    for view in data['views'].values():
        name = view.get('image','')
        demand(Path(name).name==name and name.endswith('.png'), 'preview path must be a PNG basename')
        png = (directory/name).read_bytes()
        demand(png[:8]==b'\x89PNG\r\n\x1a\n' and png[12:16]==b'IHDR' and struct.unpack_from('>II',png,16)==(512,512), 'expected 512x512 PNG')
        anchors = view.get('nodes',{})
        demand(required<=set(anchors), 'missing callout anchors')
        demand(all(isinstance(p,list) and len(p)==2 and all(isinstance(v,(float,int)) and math.isfinite(v) and 0<=v<=1 for v in p) for p in anchors.values()), 'callouts must be normalized finite image positions')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--source-only',action='store_true')
    parser.add_argument('--model',action='append')
    args = parser.parse_args()
    sys.path.insert(0,str(ROOT/'assets/guns/src'))
    from common import TIERS, plan
    errors = []
    for mid in args.model or TIERS:
        try:
            with (ROOT/'data/guns'/(mid+'.toml')).open('rb') as stream:
                meta = tomllib.load(stream)
            params = runpy.run_path(str(ROOT/'assets/guns/src'/(mid+'.py')))['PARAMETERS']
            demand(params['length_mm']==meta['length_mm'], 'source length differs from metadata')
            spec = plan(mid,params)
            demand({'muzzle','sight_front','sight_rear','pivot_trigger'}<=set(spec['nodes']), 'source lacks named anchors')
            demand({b['node'] for b in meta.get('button',[])}<=set(spec['nodes']), 'source lacks button anchors')
            for motion in meta.get('motion',{}).values():
                demand(motion['node'] in spec['nodes'], 'motion target absent from source plan')
            if args.source_only:
                print(f'{mid}: source plan only, {len(spec["parts"])} primitives; assets NOT verified')
            else:
                result = check_asset(ROOT/meta['model'],meta)
                check_previews(ROOT/'assets/guns/preview',mid,meta)
                print(mid+': '+json.dumps(result,sort_keys=True))
        except (OSError,ValueError,KeyError,IndexError,TypeError,struct.error) as exc:
            errors.append(f'{mid}: {exc}')
    for error in errors:
        print('error '+error)
    print(f'{len(errors)} errors'+('; SOURCE ONLY, not asset acceptance' if args.source_only else ''))
    return bool(errors)


if __name__ == '__main__':
    sys.exit(main())
