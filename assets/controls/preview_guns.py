# SPDX-License-Identifier: GPL-3.0-only
"""CPU preview of this repository's original gun assets, never game assets."""
import argparse
from pathlib import Path
import sys
sys.dont_write_bytecode=True
import tomllib
import bpy
ROOT=Path(__file__).resolve().parents[2]
sys.path.insert(0,str(ROOT/'assets/controls/src'))
import common

def main():
    p=argparse.ArgumentParser();p.add_argument('--out',type=Path,required=True)
    args=p.parse_args(sys.argv[sys.argv.index('--')+1:]);args.out.mkdir(parents=True,exist_ok=True)
    for mid in ('arc-pistol-twin','mnt-mg-heavy'):
        common.reset();bpy.ops.import_scene.gltf(filepath=str(ROOT/'assets/guns'/(mid+'.glb')))
        root=bpy.data.objects.get('grip');lods=[bpy.data.objects['LOD0'],bpy.data.objects['LOD1']]
        meta=tomllib.loads((ROOT/'data/guns'/(mid+'.toml')).read_text(encoding='utf-8'))
        tint=meta['tints'][meta['tints']['p1']]
        for mat in bpy.data.materials:
            base=mat.name.split('.')[0]
            if base in ('body','accent'):
                c=tint[base];rgb=[int(c[i:i+2],16)/255 for i in (1,3,5)]
                linear=[v/12.92 if v<=.04045 else ((v+.055)/1.055)**2.4 for v in rgb]
                bsdf=mat.node_tree.nodes.get('Principled BSDF')
                if bsdf:bsdf.inputs['Base Color'].default_value=tuple(linear)+(1,)
        common.render(root,lods,args.out/(mid+'.png'))
        print('Rendered existing original',mid,flush=True)

if __name__=='__main__':main()
