#!/usr/bin/env python3
"""Compile original Wii part/action mappings for the desktop view; no input I/O.

The bridge and Dolphin profiles have separate owners. Compiled rows describe
their agreed DSU transport, never claim game or headset acceptance.
"""
import argparse
from copy import deepcopy
import json
from pathlib import Path
import re
import tomllib
from wii_shell_stick import render_profile

ROOT=Path(__file__).resolve().parents[1]
ID=re.compile(r'^[a-z0-9]+(?:-[a-z0-9]+)*$')


def load(path):
    with Path(path).open('rb') as f: return tomllib.load(f)


def require(condition,message):
    if not condition: raise ValueError(message)


def compile_library(root=ROOT):
    root=Path(root); directory=root/'data/controls/wii'
    base=load(directory/'remote-nunchuk.toml')
    require(base['version']=='0.1','Wii version must be 0.1')
    require(base.get('route')=='flat-dolphin','DSU Wii mappings require the flat-dolphin route')
    require(base['transport']['pointer']=='right_stick' and base['transport']['nunchuk_stick']=='left_stick','DSU axes must remain separate')
    require(base['transport']['motion_input'] and not base['transport']['shake_binding'] and not base['transport']['swing_binding'], 'Motion Input must not be double-counted')
    rows=[]
    for path in sorted((directory/'games').glob('*.toml')):
        profile=load(path); gid=profile['game_id']
        require(ID.fullmatch(gid) and path.stem==gid,'Wii game id differs from filename')
        game=load(root/'games'/gid/'game.toml')
        require(game['hardware']=='nintendo-wii','Wii mapping applied to other hardware')
        custom=profile.get('custom_setup',False)
        elements=deepcopy(profile['element'] if custom else base['element'])
        overrides={row['id']:row for row in profile.get('action',[])}
        for element in elements:
            if element['id'] in overrides: element.update({k:v for k,v in overrides[element['id']].items() if k!='id'})
        require(not(set(overrides)-{e['id'] for e in elements}),'Action override does not name a part')
        ids=set(); physical={}
        for row in elements:
            require(ID.fullmatch(row['id']) and row['id'] not in ids,'Duplicate/invalid Wii element id'); ids.add(row['id'])
            require(all(isinstance(row.get(k),str) and row[k] for k in ('part','label','control','target')),'Wii row lacks part/action/control/target')
            require(row.get('role') in ('remote','nunchuk','local'),'Invalid Wii hand role')
            if row['role']!='local' and row['control'] not in ('remote_motion','nunchuk_motion','pointer'):
                require(row['control'] not in physical,'Physical button silently overloaded')
                physical[row['control']]=row['id']
        if not custom:
            require({'aim','fire','interact','z','c','stick','menu','motion','nunchuk-motion','reload-fallback','melee-fallback','laser'}<=ids,'Wii base parts incomplete')
        configured=1 if custom else min(game['players'],game['controls'].get('guns',1),2)
        generated=[]
        for slot in range(configured):
            for source in elements:
                row=deepcopy(source); row['id']=f'p{slot+1}-'+row['id']; row['slot']=row['player']=slot
                row['binding']=dict(hand='slot',control=row.pop('control'),mode=row.get('mode','press'),role=row['role'])
                # Logical player slots are not physical DSU devices. One gun
                # attaches its Nunchuk to the other device; co-op reuses each
                # player's own device for both Remote and Nunchuk.
                has_device=row['role']!='local' or row['binding']['control'] in ('reload_fallback','melee_fallback')
                fallback_player=row.get('fallback_player')
                if fallback_player is not None:
                    require(type(fallback_player) is int and fallback_player==0,'Current keyboard fallback producer only supports player 1')
                row['binding']['physical_dsu_slot_by_mode']=dict(
                    one_gun=(1 if row['role']=='nunchuk' else 0) if has_device else None,
                    two_remotes=(fallback_player if fallback_player is not None else slot) if has_device else None)
                row['input']=dict(kind='wii' if row['role']!='local' else 'runtime',semantic=row['target'])
                row['availability']=dict(state=row.pop('implementation_state','configured'),
                    reason=row.pop('implementation_reason','Authored mapping; emulator/profile and gameplay checks are separate'))
                require(row['availability']['state'] in ('configured','planned','unavailable'),'Invalid Wii implementation state')
                if row['binding']['control']=='owner_setup':
                    row['availability']=dict(state='planned',reason='Choose the game layout and binding with the owner')
                    row['player_reason']='Choose this binding together during setup.'
                if fallback_player is not None and slot!=fallback_player:
                    row['availability']=dict(state='unavailable',reason='Keyboard fallback producer only injects motion into physical DSU slot 0')
                    row['player_reason']='Keyboard fallback is only available for player 1. Use physical motion.'
                    row['binding_label']='Unavailable: keyboard fallback is player 1 only'
                row.setdefault('node',''); generated.append(row)
        model=load(root/'data/guns/generic-pistol.toml')
        data=dict(version='0.1',family='wii',route=base['route'],id=gid+'-controls',title=game['title']+' controls',gun_model='generic-pistol',
                  element=generated,policy=dict(p1_hand='right',two_guns='on_join',supported_primary_hands=['right'],
                      nunchuk_hand_by_mode=dict(one_gun='other',two_remotes='same'),custom_setup=custom),
                  transport=base['transport'],questions=profile.get('questions',[]),sources=profile.get('sources',[]),
                  custom_setup=custom,game_profile=profile.get('game_profile','remote-nunchuk'),
                  instruction=profile.get('instruction','Move the gun to aim. Use real motion; R and V are seated fallbacks.'))
        rows.append(dict(game_id=gid,model='generic-pistol',model_provenance='Wii initial world-gun default',shape_review=False,
                         control_set=data['id'],layers=['data/controls/wii/remote-nunchuk.toml',path.relative_to(root).as_posix()],
                         data=data,model_metadata=model,two_gun_eligible=configured==2,separate_views=False,
                         configured_slots=configured,declared_active_slots=0,node_validation='not-decoded',
                         gaps=[dict(category='runtime-acceptance',id='wii-input',reason='Dolphin and headset acceptance are separate from this mapping')]))
    require(len(rows)==5,'Tier 1 must contain exactly five Wii sets')
    return dict(version='0.1',baseline_only=True,games=rows)


def resolve_game(root,gid,primary_hand='right',model=None):
    require(primary_hand in ('left','right'),'Invalid primary hand')
    catalog=json.loads((Path(root)/'data/controls/wii/view-catalog.json').read_text())
    row=deepcopy(next(r for r in catalog['games'] if r['game_id']==gid))
    row['data']['policy']['p1_hand']=primary_hand
    if model:
        row['model']=model['id']; row['model_metadata']=model; row['data']['gun_model']=model['id']
    return row


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--write',action='store_true'); parser.add_argument('--game'); parser.add_argument('--json',action='store_true')
    args=parser.parse_args(); catalog=compile_library()
    payload=json.dumps(catalog,indent=2,ensure_ascii=False)+'\n'; target=ROOT/'data/controls/wii/view-catalog.json'
    if args.write: target.write_text(payload,encoding='utf-8')
    else: require(target.read_text(encoding='utf-8')==payload,'Wii compiled view catalog is stale; run --write')
    selected=[r for r in catalog['games'] if not args.game or r['game_id']==args.game]
    require(bool(selected),'Unknown Wii game')
    if args.json: print(json.dumps(dict(version='0.1',games=selected,errors=[]),indent=2,ensure_ascii=False))
    else:
        for game in selected:
            print(game['game_id'])
            for row in game['data']['element']:
                if row['slot']==0: print('  '+row['part']+' -> '+row['label']+' -> '+row.get('binding_label',row['binding']['control'])+' ('+row['target']+'; '+row['availability']['state']+')')
        print('5 tier-1 Wii mappings; 0 data errors; gameplay acceptance is separate')
    return 0


if __name__=='__main__': raise SystemExit(main())
