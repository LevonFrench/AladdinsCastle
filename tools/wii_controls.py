#!/usr/bin/env python3
"""Compile canonical shell/stick policy into route-aware view data."""
from copy import deepcopy
import argparse
import json
from pathlib import Path
import re
import tomllib
from wii_shell_stick import render_profile,source,ROUTES,write_generated
ROOT=Path(__file__).resolve().parents[1]
ID=re.compile(r'^[a-z0-9]+(?:-[a-z0-9]+)*$')
def load(path):
    with Path(path).open('rb') as f:return tomllib.load(f)
def require(ok,message):
    if not ok:raise ValueError(message)

def compile_library(root=ROOT):
    root=Path(root);directory=root/'data/controls/wii';base=source(root);rows=[]
    model=load(root/'data/guns'/f'{base["shell"]}.toml');accessory=load(root/'data/guns'/f'{base["stick"]}.toml')
    for path in sorted((directory/'games').glob('*.toml')):
        profile=load(path);gid=profile['game_id'];game=load(root/'games'/gid/'game.toml')
        require(ID.fullmatch(gid) and path.stem==gid and game['hardware']=='nintendo-wii','Invalid Wii game identity')
        slots=min(game['players'],game['controls'].get('guns',1),2)
        overrides={a['id']:a for a in profile.get('action',[]) if a.get('confirmed') is True}
        layout=profile.get('layout',dict(state='question',name='',question='Which option-screen control layout is selected?'))
        require(layout.get('state') in ('question','confirmed'),'Invalid game layout evidence state')
        variants={}
        for route in ROUTES:
            elements=[]
            for slot in range(slots):
                for action in base['action']:
                    row=deepcopy(action);ident=action['id'];role=action['role'];verified=overrides.get(ident)
                    require(not verified or bool(verified.get('sources')),'Confirmed action needs screen evidence reference')
                    row.update(id=f'p{slot+1}-'+ident,slot=slot,player=slot,
                        label=verified['label'] if verified else 'Question: game action for '+action['part']+'?',
                        game_action_state='confirmed' if verified else 'question',model=base['shell'] if role=='remote' else base['stick'],target=action['ini'],
                        binding=dict(hand='slot',control=action['split'],role=role,mode='hold',expression_by_mode=dict(split=action['split'],docked=action['split'],two_remotes=action['dual']),physical_dsu_slot_by_mode=dict(one_gun=1 if role=='nunchuk' else 0,two_remotes=slot)),
                        input=dict(kind='wii',semantic=action['ini']),availability=dict(state='configured' if route=='dolphinxr' else 'planned',reason='Generated native profile; gameplay validation pending' if route=='dolphinxr' else 'Generated DSU profile; bridge parity pending'))
                    elements.append(row)
                for ident,role,node,part,target in [('aim','remote','muzzle','Shell aim','pointer'),('motion','remote','grip','Move the shell','accelerometer_gyro'),('nunchuk-motion','nunchuk','grip','Move the stick controller','nunchuk_acceleration')]:
                    elements.append(dict(id=f'p{slot+1}-'+ident,slot=slot,player=slot,part=part,label='Aim' if ident=='aim' else 'Question: motion action in this game?',game_action_state='confirmed' if ident=='aim' else 'question',role=role,node=node,model=base['shell'] if role=='remote' else base['stick'],target=target,
                        binding=dict(hand='slot',control='pointer' if ident=='aim' else 'remote_motion' if role=='remote' else 'nunchuk_motion',role=role,mode='hold',physical_dsu_slot_by_mode=dict(one_gun=1 if role=='nunchuk' else 0,two_remotes=slot)),input=dict(kind='wii',semantic=target),availability=dict(state='configured',reason='Physical motion; gameplay recognition remains unverified')))
            data=dict(version='0.1',schema=base['version'],family='wii',route=route,id=gid+'-shell-and-stick',title=game['title']+' — shell and stick',gun_model=base['shell'],assembly=dict(shell=base['shell'],accessory=base['stick'],default_hold='split',holds=base['holds']),
                element=elements,policy=dict(p1_hand='right',two_guns='on_join',supported_primary_hands=['right','left'],nunchuk_hand_by_mode=dict(one_gun='other',two_remotes='same'),native_two_remotes=False,native_hand_config='Android/QuestLeftHanded'),
                transport=dict(pointer='right_stick',nunchuk_stick='left_stick',motion_input=True,shake_binding=True,swing_binding=True,acceleration_units='g',gyro_units='degrees_per_second'),questions=[layout.get('question','Confirm the selected game layout')]+profile.get('questions',[]),sources=profile.get('sources',[]),layout=layout,
                game_profile=layout.get('name') or 'Question: selected game layout',custom_setup=False,instruction='Shell in firing hand; stick controller in other hand. Both stick clicks recenter. Game actions are questions until the option/instruction screens are recorded.',profiles=dict(native='data/controls/wii/generated/dolphinxr/split-right.ini',dsu='data/controls/wii/generated/flat-dolphin/split-right.ini'))
            variants[route]=data
        rows.append(dict(game_id=gid,model=base['shell'],model_provenance='Owner shell-and-stick Wii rule',shape_review=False,control_set=gid+'-shell-and-stick',layers=['data/controls/wii/shell-and-stick.toml',path.relative_to(root).as_posix()],data=variants['flat-dolphin'],route_variants=variants,model_metadata=model,accessory_metadata=accessory,two_gun_eligible=slots==2,separate_views=False,configured_slots=slots,declared_active_slots=0,node_validation='not-decoded',gaps=[dict(category='runtime-acceptance',id='wii-input',reason='Native/bridge/held-model/headset acceptance remain separate')]))
    require(len(rows)==5,'Tier 1 must contain exactly five Wii sets')
    return dict(version='0.1',baseline_only=True,games=rows)

def resolve_game(root,gid,primary_hand='right',model=None,route='flat-dolphin'):
    require(primary_hand in ('left','right'),'Invalid primary hand');require(route in ROUTES,'Unknown Wii route')
    catalog=json.loads((Path(root)/'data/controls/wii/view-catalog.json').read_text(encoding='utf-8'))
    row=deepcopy(next(r for r in catalog['games'] if r['game_id']==gid))
    require(row.get('data',{}).get('route')=='flat-dolphin','Invalid Wii catalog baseline route')
    require(row['route_variants'].get('flat-dolphin')==row['data'],'Wii baseline route metadata mismatch')
    row['data']=deepcopy(row['route_variants'][route]);row['data']['policy']['p1_hand']=primary_hand
    if model:row['model']=model['id'];row['model_metadata']=model;row['data']['gun_model']=model['id']
    return row

def main():
    parser=argparse.ArgumentParser(description=__doc__);parser.add_argument('--write',action='store_true');parser.add_argument('--game');parser.add_argument('--json',action='store_true');parser.add_argument('--route',choices=ROUTES,default='flat-dolphin')
    args=parser.parse_args();catalog=compile_library();target=ROOT/'data/controls/wii/view-catalog.json';payload=json.dumps(catalog,indent=2,ensure_ascii=False)+'\n'
    if args.write:target.write_text(payload,encoding='utf-8')
    else:require(target.read_text(encoding='utf-8')==payload,'Wii view catalog is stale; run --write')
    write_generated(check=not args.write)
    selected=[resolve_game(ROOT,r['game_id'],route=args.route) for r in catalog['games'] if not args.game or r['game_id']==args.game];require(bool(selected),'Unknown Wii game')
    if args.json:print(json.dumps(dict(version='0.1',games=selected,errors=[]),indent=2,ensure_ascii=False))
    else:print('5 shell-and-stick Wii sets; native and DSU profiles match shared source; game actions remain questions')
    return 0
if __name__=='__main__':raise SystemExit(main())
