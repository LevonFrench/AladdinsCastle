"""Pure-Python hand export checker. No Blender, graphics or input I/O."""
from pathlib import Path
import json
import tomllib
from check_gun_assets import check_asset,check_previews
ROOT=Path(__file__).resolve().parents[1]
def check(root=ROOT):
    results={}
    for hand in ('right','left'):
        mid='grip-hand-'+hand
        with (Path(root)/'assets/hands'/f'{mid}.toml').open('rb') as f:meta=tomllib.load(f)
        if meta['version']!='hand-assembly/1' or meta['quaternion_order']!='xyzw' or meta['units']!='metres':raise ValueError('Invalid hand conventions')
        results[hand]=check_asset(Path(root)/meta['model'],meta)
        check_previews(Path(root)/'assets/hands/preview',mid,meta)
    return results
if __name__=='__main__':print(json.dumps(check(),indent=2))
