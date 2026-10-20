"""Remove the maker's lettering from hotd2-vr's arcade gun (core/rend/vr/gun_model.h). GPL-3.0-only.

    python clean_gun_model.py <hotd2-vr source folder>

The arcade gun ("Namco Arcade Gun" by Martoscar, CC BY 4.0) has the maker's name engraved
on the right side of its upper housing. This tool deletes the engraved letters, refills the
side face (assets/guns/src/arc_light_gun_mesh.py), and rebuilds the header with hotd2-vr's own
converter (hotd2-vr/gun_model/convert_gun.py), so the format and placement stay hotd2-vr's.
Everything else is kept as it was: vertex positions and triangles of the rest of the model are
unchanged, the muzzle is unchanged (xr_gun.h's static_assert still holds); 90 of the 41,052
normal components move by one step of 1/127 because the converter re-quantises them.
The header gets a line saying what was changed, as CC BY 4.0 asks. Needs numpy.
"""
import json
import struct
import subprocess
import sys
import tempfile
from pathlib import Path

import numpy as np

HERE = Path(__file__).resolve().parent
sys.path.insert(0, str(HERE.parents[2] / 'assets' / 'guns' / 'src'))
import arc_light_gun_mesh as G  # noqa: E402

GROUPS = {'hardsurfaces': 'hardsurfaces_2', 'softsurfaces': 'softsurfaces_1', 'screws': 'screws_3', 'lens': 'lens_4'}
NOTE = ("// Also changed (AladdinsCastle, 2026): the maker's lettering engraved on the right side removed\n"
        "// and the side face refilled, by setups/flycast-hotd2-vr/tools/clean_gun_model.py.\n")


def model_space_glb(mesh, grip_anchor, grip_target):
    """The cleaned mesh back in the model's own space, laid out the way convert_gun.py reads it."""
    blob, views, accessors, meshes = bytearray(), [], [], []
    nodes = [dict(name='root', children=[])]

    def add(array, target):
        while len(blob) % 4:
            blob.append(0)
        data = array.tobytes()
        views.append(dict(buffer=0, byteOffset=len(blob), byteLength=len(data), target=target))
        blob.extend(data)
        return len(views) - 1
    for name, first, count in mesh['parts']:
        tri = mesh['tri'][first:first + count]
        used = np.unique(tri)
        remap = -np.ones(len(mesh['pos']), np.int64)
        remap[used] = np.arange(len(used))
        g = mesh['pos'][used] * G.UNIT - grip_target           # inverse of convert_gun.to_gun
        p = np.stack([-g[:, 2], g[:, 1], g[:, 0]], 1) + grip_anchor
        n = mesh['nrm'][used] / 127.0
        n = np.stack([-n[:, 2], n[:, 1], n[:, 0]], 1)
        idx = remap[tri].astype(np.uint32).reshape(-1)
        pv, nv, iv = add(p.astype(np.float32), 34962), add(n.astype(np.float32), 34962), add(idx, 34963)
        accessors += [dict(bufferView=pv, componentType=5126, count=len(p), type='VEC3',
                           min=p.min(0).tolist(), max=p.max(0).tolist()),
                      dict(bufferView=nv, componentType=5126, count=len(p), type='VEC3'),
                      dict(bufferView=iv, componentType=5125, count=len(idx), type='SCALAR')]
        a = len(accessors) - 3
        meshes.append(dict(primitives=[dict(attributes=dict(POSITION=a, NORMAL=a + 1), indices=a + 2)]))
        nodes[0]['children'].append(len(nodes))
        nodes.append(dict(name=GROUPS[name], children=[len(nodes) + 1]))
        nodes.append(dict(name=GROUPS[name] + '_mesh', mesh=len(meshes) - 1))
    while len(blob) % 4:
        blob.append(0)
    doc = dict(asset=dict(version='2.0'), scene=0, scenes=[dict(nodes=[0])], nodes=nodes, meshes=meshes,
               accessors=accessors, bufferViews=views, buffers=[dict(byteLength=len(blob))])
    js = json.dumps(doc).encode()
    js += b' ' * ((4 - len(js) % 4) % 4)
    return (struct.pack('<4sII', b'glTF', 2, 28 + len(js) + len(blob)) + struct.pack('<II', len(js), 0x4E4F534A) + js
            + struct.pack('<II', len(blob), 0x004E4942) + bytes(blob))


def main():
    if len(sys.argv) != 2:
        sys.exit(__doc__)
    source = Path(sys.argv[1])
    header = source / 'core' / 'rend' / 'vr' / 'gun_model.h'
    converter = source / 'hotd2-vr' / 'gun_model' / 'convert_gun.py'
    text = header.read_text(encoding='utf-8')
    if 'lettering engraved on the right side removed' in text:
        print('already clean:', header)
        return
    clean, report = G.remove_lettering(G.parse_header(text))
    if report['letter_loops'] == 0:
        sys.exit('no lettering found; header left as it is')
    sys.path.insert(0, str(converter.parent))
    import convert_gun as C
    with tempfile.TemporaryDirectory() as tmp:
        glb, out = Path(tmp) / 'gun.glb', Path(tmp) / 'gun_model.h'
        glb.write_bytes(model_space_glb(clean, C.GRIP_ANCHOR, C.GRIP_TARGET))
        subprocess.run([sys.executable, str(converter), str(glb), str(out)], check=True, cwd=str(converter.parent))
        new = out.read_text(encoding='utf-8')
    anchor = "also under CC BY 4.0.\n"
    if anchor not in new:
        sys.exit('converter output has an unexpected header comment')
    new = new.replace(anchor, anchor + NOTE, 1)
    if G.parse_header(new)['pos'].tolist() != clean['pos'].tolist():
        sys.exit('round trip changed the vertex positions')
    header.write_text(new, encoding='utf-8', newline='\n')
    print(json.dumps(dict(header=str(header), **report)))


if __name__ == '__main__':
    main()
