# SPDX-License-Identifier: GPL-3.0-only
"""Build named original controls and CPU previews in one background Blender job.

blender -b --factory-startup --threads 2 --python-exit-code 1 -P assets/controls/build.py
  -- --models wheel-three-spoke shifter-four-gate pedals-dual panel-view-four --renders <private-output>
"""
import argparse
import importlib.util
from pathlib import Path
import sys
sys.dont_write_bytecode=True

ROOT=Path(__file__).resolve().parents[2]
SRC=ROOT/'assets/controls/src'
sys.path.insert(0,str(SRC))
import common

def main():
    parser=argparse.ArgumentParser();parser.add_argument('--models',nargs='+',required=True)
    parser.add_argument('--renders',type=Path,required=True)
    args=parser.parse_args(sys.argv[sys.argv.index('--')+1:]);args.renders.mkdir(parents=True,exist_ok=True)
    for mid in args.models:
        source=SRC/(mid+'.py')
        if not source.is_file():raise ValueError('Missing original model source: '+mid)
        spec=importlib.util.spec_from_file_location(mid,source);module=importlib.util.module_from_spec(spec);spec.loader.exec_module(module)
        common.reset();root,lods=common.build_model(mid,module.build)
        common.export(root,ROOT/'assets/controls'/(mid+'.glb'))
        common.render(root,lods,args.renders/(mid+'.png'),view=(1.4,2.6,1.7) if mid=='bars-motorcycle' else (1.4,-2.6,1.7))
        print('Completed',mid,flush=True)

if __name__=='__main__':main()
