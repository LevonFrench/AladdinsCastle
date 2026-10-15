"""Bakes a cabinet-control model (glTF binary from assets/controls) into a small file the patched
Supermodel draws in head-tracked mode: LOD0 only, every mesh in control_root space (metres,
+Y up, -Z forward), parts under the moving pivot node marked so the renderer can turn them.

  python bake_control.py <model.glb> <pivot node name> <out.acm>

File layout (little endian):
  'ACM1', u32 parts, f32 pivot origin[3]
  per part: u32 moving, f32 colour[3], u32 vertices, u32 indices,
            vertices * (f32 position[3], f32 normal[3]), indices * u32
"""
import json
import math
import struct
import sys


def quat_to_mat(q):
    x, y, z, w = q
    return [[1 - 2 * (y * y + z * z), 2 * (x * y - z * w), 2 * (x * z + y * w)],
            [2 * (x * y + z * w), 1 - 2 * (x * x + z * z), 2 * (y * z - x * w)],
            [2 * (x * z - y * w), 2 * (y * z + x * w), 1 - 2 * (x * x + y * y)]]


def local_matrix(node):
    if 'matrix' in node:
        m = node['matrix']
        return [[m[0], m[4], m[8], m[12]], [m[1], m[5], m[9], m[13]], [m[2], m[6], m[10], m[14]], [0, 0, 0, 1]]
    r = quat_to_mat(node.get('rotation', [0, 0, 0, 1]))
    s = node.get('scale', [1, 1, 1])
    t = node.get('translation', [0, 0, 0])
    return [[r[i][0] * s[0], r[i][1] * s[1], r[i][2] * s[2], t[i]] for i in range(3)] + [[0, 0, 0, 1]]


def mul(a, b):
    return [[sum(a[i][k] * b[k][j] for k in range(4)) for j in range(4)] for i in range(4)]


def main(src, pivot_name, out):
    data = open(src, 'rb').read()
    jlen = struct.unpack_from('<I', data, 12)[0]
    gltf = json.loads(data[20:20 + jlen])
    boff = 20 + jlen
    blen = struct.unpack_from('<I', data, boff)[0]
    binary = data[boff + 8:boff + 8 + blen]
    nodes = gltf['nodes']
    by_name = {n.get('name'): i for i, n in enumerate(nodes)}

    def accessor(index):
        acc = gltf['accessors'][index]
        view = gltf['bufferViews'][acc['bufferView']]
        start = view.get('byteOffset', 0) + acc.get('byteOffset', 0)
        comps = {'SCALAR': 1, 'VEC2': 2, 'VEC3': 3, 'VEC4': 4}[acc['type']]
        fmt = {5126: 'f', 5125: 'I', 5123: 'H', 5121: 'B'}[acc['componentType']]
        size = struct.calcsize('<' + fmt)
        stride = view.get('byteStride', size * comps)
        return [struct.unpack_from('<' + fmt * comps, binary, start + i * stride) for i in range(acc['count'])]

    parts = []

    def walk(i, parent, moving):
        node = nodes[i]
        name = node.get('name', '')
        if name.startswith('LOD') and name != 'LOD0':
            return
        world = mul(parent, local_matrix(node))
        moving = moving or name == pivot_name
        if 'mesh' in node:
            normal_m = [[world[r][c] for c in range(3)] for r in range(3)]
            for prim in gltf['meshes'][node['mesh']]['primitives']:
                pos = accessor(prim['attributes']['POSITION'])
                nrm = accessor(prim['attributes']['NORMAL'])
                idx = [v[0] for v in accessor(prim['indices'])]
                mat = gltf['materials'][prim['material']] if 'material' in prim else {}
                colour = mat.get('pbrMetallicRoughness', {}).get('baseColorFactor', [0.5, 0.5, 0.5, 1])[:3]
                verts = []
                for p, n in zip(pos, nrm):
                    wp = [sum(world[r][c] * p[c] for c in range(3)) + world[r][3] for r in range(3)]
                    wn = [sum(normal_m[r][c] * n[c] for c in range(3)) for r in range(3)]
                    length = math.sqrt(sum(v * v for v in wn)) or 1.0
                    verts.append(wp + [v / length for v in wn])
                parts.append((moving, colour, verts, idx))
        for child in node.get('children', []):
            walk(child, world, moving)

    identity = [[1 if r == c else 0 for c in range(4)] for r in range(4)]
    for root in gltf['scenes'][gltf.get('scene', 0)]['nodes']:
        walk(root, identity, False)

    # the pivot's origin in control_root space
    def world_of(target):
        def find(i, parent):
            world = mul(parent, local_matrix(nodes[i]))
            if i == target:
                return world
            for c in nodes[i].get('children', []):
                found = find(c, world)
                if found:
                    return found
            return None
        for root in gltf['scenes'][gltf.get('scene', 0)]['nodes']:
            found = find(root, identity)
            if found:
                return found
    pivot = world_of(by_name[pivot_name])
    origin = [pivot[r][3] for r in range(3)]

    with open(out, 'wb') as f:
        f.write(b'ACM1' + struct.pack('<I3f', len(parts), *origin))
        for moving, colour, verts, idx in parts:
            f.write(struct.pack('<I3fII', 1 if moving else 0, *colour, len(verts), len(idx)))
            for v in verts:
                f.write(struct.pack('<6f', *v))
            f.write(struct.pack('<%dI' % len(idx), *idx))
    moving_parts = sum(1 for p in parts if p[0])
    print(f'{out}: {len(parts)} parts ({moving_parts} turning), {sum(len(p[2]) for p in parts)} vertices, pivot at {origin}')


if __name__ == '__main__':
    main(sys.argv[1], sys.argv[2], sys.argv[3])
