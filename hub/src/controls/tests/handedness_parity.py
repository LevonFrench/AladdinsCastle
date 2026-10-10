"""Test-only handedness collision oracle; production uses the native resolver."""
import argparse
import json
from pathlib import Path
import sys

sys.dont_write_bytecode = True
parser = argparse.ArgumentParser()
parser.add_argument('--source-root',type=Path,required=True)
args = parser.parse_args()
sys.path.insert(0,str(args.source_root/'tools'))
from control_sets import Resolver

resolver=Resolver(args.source_root)
cases=[]
for primary in ('left','right'):
    for gid,control in (('timecris','trigger'),('hotd3','flick_up')):
        for explicit in ('left','right'):
            defaults={'policy':{'p1_hand':primary,'two_guns':'off'},'element':[
                {'id':'p1-coin','binding':{'hand':explicit,'control':control,'mode':'press'}}]}
            case={'game_id':gid,'defaults':defaults,'collides':explicit==primary}
            try:
                case['result']=resolver.resolve_game(gid,profile_defaults=defaults)
            except ValueError as error:
                case['error']=str(error)
            cases.append(case)
print(json.dumps(cases,ensure_ascii=False,default=str))
