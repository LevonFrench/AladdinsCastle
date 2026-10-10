"""Source-only parity oracle for a confined synthetic native-resolver fixture."""
import argparse
import json
from pathlib import Path
import sys

sys.dont_write_bytecode = True
parser = argparse.ArgumentParser()
parser.add_argument('--source-root',type=Path,required=True)
parser.add_argument('--fixture-root',type=Path,required=True)
args = parser.parse_args()
sys.path.insert(0,str(args.source_root/'tools'))
from control_sets import Resolver,load

root=args.fixture_root
def optional(relative):
    path=root/relative
    return load(path) if path.exists() else {}
gid='fixture-gun'
game_layers=[]
packs=[]
for pack in ('first','second'):
    game_layers.append(optional(f'packs/{pack}/games/{gid}/game.toml'))
    packs.append([optional(f'packs/{pack}/data/controls/gun-con-pistol-slim.toml'),
                  optional(f'packs/{pack}/games/{gid}/setup/controls.toml')])
game_layers.append(optional(f'user/overrides/games/{gid}/game.toml'))
user=[optional('user/overrides/data/controls/gun-con-pistol-slim.toml'),
      optional(f'user/overrides/games/{gid}/setup/controls.toml')]
result=Resolver(root).resolve_game(gid,game_overrides=game_layers,pack_overrides=packs,
    user_override=user,profile_defaults={'policy':{'p1_hand':'left','two_guns':'off'},'extension':{'caller':'kept'}},
    profile_model='generic-pistol',model_overrides=[
        optional('packs/first/data/guns/con-pistol-slim.toml'),
        optional('packs/second/data/guns/con-pistol-slim.toml'),
        optional('user/overrides/data/guns/con-pistol-slim.toml')])
print(json.dumps(result,ensure_ascii=False,default=str))
