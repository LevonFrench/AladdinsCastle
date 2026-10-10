"""One explicitly owner-approved sequential Blender CPU build batch. GPL-3.0-only."""
import argparse
import json
from pathlib import Path
import subprocess
import sys

HERE = Path(__file__).resolve().parent


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--blender', type=Path, required=True)
    parser.add_argument('--owner-approved-cpu-batch', action='store_true', required=True)
    parser.add_argument('--model', action='append', choices=(
        'generic-pistol','arc-pistol-slide','arc-pistol-twin','con-pistol-slim',
        'con-pistol-dpad','con-revolver-chunky','mnt-mg-heavy'))
    parser.add_argument('--samples', type=int, default=16)
    args = parser.parse_args()
    if not args.blender.is_file():
        parser.error('Use the absolute path to an existing Blender executable; this tool never installs it')
    models = args.model or ['generic-pistol','arc-pistol-slide','arc-pistol-twin',
                           'con-pistol-slim','con-pistol-dpad','con-revolver-chunky','mnt-mg-heavy']
    for model in models:
        subprocess.run([str(args.blender.resolve()), '--background', '--factory-startup',
                        '--python-exit-code','1','--threads','4','--python',str(HERE/'src'/(model+'.py')),
                        '--','--out',str(HERE/(model+'.glb')),'--preview-dir',str(HERE/'preview'),
                        '--samples',str(args.samples)], check=True)
    # Sheet assembly uses Blender's image data API without rendering or a GPU.
    if not args.model:
        subprocess.run([str(args.blender.resolve()),'--background','--factory-startup',
                        '--python-exit-code','1','--python',str(HERE/'src/sheet.py'),
                        '--','--preview-dir',str(HERE/'preview')],check=True)
    return subprocess.call([sys.executable,str(HERE.parents[1]/'tools/check_gun_assets.py')])


if __name__ == '__main__':
    sys.exit(main())
