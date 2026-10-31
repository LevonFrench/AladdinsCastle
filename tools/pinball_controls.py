"""Source-backed display metadata only. No input, table discovery or launch."""
from copy import deepcopy
import argparse
import json
from pathlib import Path
import tomllib
ROOT=Path(__file__).resolve().parents[1]

def compile_route(root=ROOT):
    path=Path(root)/'data/controls/pinball/vpx-openxr.toml'
    with path.open('rb') as f:source=tomllib.load(f)
    if source['version']!='pinball-controls/1' or source['id']!='vpx-openxr':raise ValueError('Invalid Pinball route')
    elements=[];ids=set()
    for action in source['action']:
        if action['id'] in ids or action['hand'] not in ('left','right'):raise ValueError('Invalid Pinball action')
        ids.add(action['id'])
        state=action.get('state','planned')
        if state not in ('planned','unavailable'):raise ValueError('Unreviewed runtime capability')
        elements.append(dict(id=action['id'],part=action['part'],label=action['label'],slot=0,player=0,node='',warning=action.get('warning',False),binding=dict(hand=action['hand'],control=action['control']),input=dict(kind='pinball',semantic=action['target']),availability=dict(state=state,reason=action.get('reason','Controller binding exists in source; reviewed host bridge and table acceptance pending')),threshold=action.get('threshold'),staged_threshold=action.get('staged_threshold'),scale=action.get('scale',1)))
    data=dict(version='0.1',schema=source['version'],family='pinball',route=source['id'],id=source['id'],title=source['title'],element=elements,policy=dict(p1_hand='right',supported_primary_hands=['right','left']),sources=source['sources'],source_commit=source['source_commit'])
    return dict(game_id='pinball-vpx-openxr',model='quest-controllers',model_provenance='Controller illustration only',control_set='vpx-openxr',layers=['data/controls/pinball/vpx-openxr.toml'],data=data,model_metadata={},configured_slots=1,two_gun_eligible=False,declared_active_slots=0,node_validation='not-applicable',gaps=[dict(category='runtime-acceptance',id='vpx-bridge',reason='Standard VPX has no hold-to-quit (U4) and no 150 ms chord window; U4 needs the patched AladdinsCastle VPX build (setups/vpx-vr), not yet installed or headset-tested; otherwise the way out is the left Menu button, the Hub panel (Exit game), or Esc; live headset acceptance pending')])

def resolve_route(route='vpx-openxr',root=ROOT):
    if route!='vpx-openxr':raise ValueError('Unknown Pinball route')
    return deepcopy(compile_route(root))

def main():
    parser=argparse.ArgumentParser();parser.add_argument('--write',action='store_true');args=parser.parse_args()
    payload=json.dumps(compile_route(),indent=2)+'\n';path=ROOT/'data/controls/pinball/view.json'
    if args.write:path.write_text(payload,encoding='utf-8')
    elif path.read_text(encoding='utf-8')!=payload:raise ValueError('Stale Pinball controls view')
    print('vpx-openxr display policy: universal layer U1-U4, U3p via VPX controller alignment, execution planned')
if __name__=='__main__':main()
