#!/usr/bin/env python3
"""Resolve v0.1 gun control sets without game files, Blender or dependencies.

Default completeness fails on unresolved parts, unverified backend bindings or
missing assets. --allow-unmapped explicitly permits SOURCE-ONLY coverage.
The resolver never sends inputs and never changes game or user configuration.
"""
from __future__ import annotations
import argparse
from copy import deepcopy
import json
import math
from pathlib import Path
import re
import sys
import tomllib

import gun_models

ROOT = Path(__file__).resolve().parents[1]
ID = re.compile(r'^[a-z0-9]+(?:-[a-z0-9]+)*$')
HANDS = {'slot','left','right','either'}
CONTROLS = {'trigger','grip','primary','secondary','thumbstick_click',
            'offscreen_trigger','flick_up','pump','slide','menu_chord'}
MODES = {'hold','press','toggle'}
RUNTIME = {'laser_toggle','recenter','pause','hand_switch','join'}


def load(path):
    with Path(path).open('rb') as stream:
        return tomllib.load(stream)


def require(condition, message):
    if not condition:
        raise ValueError(message)


def integer(value):
    return type(value) is int and value >= 0


def text(value):
    return isinstance(value,str) and bool(value.strip())


def rows_by_id(rows, context):
    require(isinstance(rows,list),context+' must be an array')
    result = {}
    for row in rows:
        require(isinstance(row,dict) and text(row.get('id')) and ID.fullmatch(row['id']),context+': kebab-case id required')
        require(row['id'] not in result,context+': duplicate id '+row['id'])
        result[row['id']] = row
    return result


def merge(base, later):
    """Recursive/ID-array layering and !delete; unknown extensions survive."""
    if isinstance(base,dict) and isinstance(later,dict):
        result = deepcopy(base)
        for key,value in later.items():
            if value == '!delete':
                result.pop(key,None)
            elif key in result:
                result[key] = merge(result[key],value)
            else:
                result[key] = deepcopy(value)
        return result
    if isinstance(base,list) and isinstance(later,list) and later and all(
            isinstance(v,dict) and 'id' in v for v in [*base,*later]):
        existing = rows_by_id(base,'inherited array')
        update = rows_by_id(later,'override array')
        return [merge(row,update.pop(key)) if key in update else deepcopy(row)
                for key,row in existing.items()] + [deepcopy(v) for v in update.values()]
    return deepcopy(later)


def vocabulary(root=ROOT):
    header = (Path(root)/'libacvr/include/acvr.h').read_text(encoding='utf-8')
    return {kind:{name.lower() for name in re.findall(r'^#define ACVR_'+prefix+r'_([A-Z0-9_]+)\s+\d+u',header,re.M)}
            for kind,prefix in (('axis','AXIS'),('button','BUTTON'))} | {
                'gun':{'trigger','reload'},'runtime':RUNTIME}


def validate_binding(binding, context):
    require(isinstance(binding,dict),context+': binding table required')
    require(binding.get('hand') in HANDS,context+': invalid hand')
    require(binding.get('control') in CONTROLS,context+': invalid control')
    require(binding.get('mode') in MODES,context+': invalid mode')
    if 'threshold' in binding:
        value = binding['threshold']
        require(type(value) in (int,float) and math.isfinite(value) and 0<=value<=1,context+': threshold must be 0..1')
    if 'invert' in binding:
        require(type(binding['invert']) is bool,context+': invert must be boolean')


def validate_set(data, vocab, model, nodes=None,check_collisions=True):
    require(data.get('version')=='0.1','control set version must be 0.1')
    require(text(data.get('id')) and ID.fullmatch(data['id']) and text(data.get('title')),'control set id/title required')
    if 'gun_model' in data:
        require(data['gun_model']==model['id'],'control set gun_model differs from the resolved model')
    elements = rows_by_id(data.get('element'),'element')
    parts = rows_by_id(data.get('unmapped_part',[]),'unmapped_part')
    seen = {}
    fallback_seen = {}
    for eid,element in elements.items():
        require(all(text(element.get(k)) for k in ('label','part')) and isinstance(element.get('node'),str),eid+': label/part/node required')
        require(integer(element.get('slot')) and integer(element.get('player')),eid+': zero-based slot/player required')
        require(element['slot'] < 2 and element['player'] < 2,eid+': gun lane supports only slots/players 0 and 1')
        require(element['slot']==element['player'],eid+': this lane maps one gun slot to each corresponding player')
        inp = element.get('input',{})
        require(isinstance(inp,dict) and inp.get('kind') in vocab and inp.get('semantic') in vocab[inp['kind']],eid+': unknown ABI input semantic')
        validate_binding(element.get('binding'),eid)
        binding = element['binding']
        if binding['control'] in ('pump','slide'):
            require('fallback_binding' in element,eid+': two-gun fallback required')
        if 'fallback_binding' in element:
            validate_binding(element['fallback_binding'],eid+' fallback')
            require(element['fallback_binding']['control'] not in ('pump','slide'),eid+': fallback must work one-handed')
        node = element['node']
        if node and nodes is not None:
            require(node in nodes,eid+': missing built node '+node)
        if binding['control']=='offscreen_trigger':
            require(inp=={'kind':'gun','semantic':'reload'},eid+': offscreen trigger is only the composite reload edge')
        # A collision is about an actual physical action, including overlapping
        # either/slot/explicit hands. Different modes cannot excuse two actions.
        hand = binding['hand']
        hands = {('right' if element['slot']==0 else 'left')} if hand=='slot' else ({'left','right'} if hand=='either' else {hand})
        for physical in hands:
            key = (physical,binding['control'])
            old = seen.get(key)
            if old and check_collisions:
                require((old['input'],old['player'],old['slot'],old['binding']) == (inp,element['player'],element['slot'],binding),
                        eid+': controller collision with '+old['id'])
            seen[key] = element
        if 'fallback_binding' in element:
            require(element['fallback_binding']['hand']=='slot',eid+': two-gun fallback must be slot-local')
        if check_collisions and data.get('configured_slots',2)==2 and inp['semantic'] in ('trigger','reload','cover_pedal'):
            require(binding['hand']=='slot',eid+': two-gun fire/reload/pedal must be slot-local')
        active = element.get('fallback_binding',binding)
        active_hands = {('right' if element['slot']==0 else 'left')} if active['hand']=='slot' else ({'left','right'} if active['hand']=='either' else {active['hand']})
        for physical in active_hands:
            key = physical,active['control']
            old = fallback_seen.get(key)
            if old and check_collisions:
                require((old['input'],old['player'],old.get('fallback_binding',old['binding']))==(inp,element['player'],active),
                        eid+': fallback collision with '+old['id'])
            fallback_seen[key] = element
    # trigger/offscreen is one edge with an off-screen predicate, not a collision.
    for eid,part in parts.items():
        require(all(text(part.get(k)) for k in ('label','reason','requested_semantic')) and isinstance(part.get('node'),str),eid+': unresolved part needs label/node/reason/requested_semantic')
        if part['node'] and nodes is not None:
            require(part['node'] in nodes,eid+': unresolved part refers to missing built node')
        require(integer(part.get('slot')) and integer(part.get('player')),eid+': unresolved part slot/player required')
    covered = {e['node'] for e in [*elements.values(),*parts.values()]}
    require({b['node'] for b in model.get('button',[])}<=covered,'model button lacks element or explicit unresolved part')
    output = rows_by_id(data.get('output',[]),'output')
    for oid,row in output.items():
        require(row.get('kind') in ('solenoid','lamp','ffb') and all(integer(row.get(k)) for k in ('player','slot','channel')),oid+': invalid output kind/index')
        require(row['slot']<2 and row['player']<2,oid+': output slot/player out of range')
        require(row.get('motion') in model.get('motion',{}),oid+': unknown motion')
        require(type(row.get('amplitude')) in (int,float) and math.isfinite(row['amplitude']) and 0<=row['amplitude']<=1,oid+': amplitude must be 0..1')
        require(integer(row.get('duration_ms')) and row['duration_ms']>0,oid+': duration must be positive')
    return data


def validate_backend(backend,vocab):
    if backend is None:
        return
    require(isinstance(backend,dict) and all(integer(backend.get(k)) for k in ('guns','players')),
            'backend guns/players must be nonnegative integers')
    require(type(backend.get('shared_view')) is bool,'backend shared_view must be explicit boolean')
    require(isinstance(backend.get('runtime_actions',[]),list) and all(a in RUNTIME for a in backend.get('runtime_actions',[])),
            'invalid declared runtime actions')
    declarations = backend.get('control',[])
    require(isinstance(declarations,list),'backend control declarations must be array')
    seen = set()
    for row in declarations:
        require(isinstance(row,dict) and row.get('kind') in {'gun','axis','button'} and
                row.get('semantic') in vocab[row['kind']] and integer(row.get('player')) and row['player']<backend['players'],
                'invalid backend input declaration')
        key = row['kind'],row['semantic'],row['player']
        require(key not in seen,'duplicate backend input declaration')
        seen.add(key)


def classify(element, backend,active_slots=0):
    """Only actual backend/profile declarations can make an input available."""
    if backend is None:
        return {'state':'unverified','reason':'No backend/runtime input declaration supplied'}
    require(isinstance(backend,dict),'backend declaration must be a table')
    if element['slot']>=active_slots:
        return {'state':'unavailable','reason':'Backend gun/player/shared-view limits exclude this slot'}
    if element['input']['kind']=='runtime':
        supported = element['input']['semantic'] in backend.get('runtime_actions',[])
    else:
        supported = (element['slot'] < backend.get('guns',0) and element['player'] < backend.get('players',0) and
                     any(row.get('kind')==element['input']['kind'] and row.get('semantic')==element['input']['semantic'] and
                         row.get('player')==element['player'] for row in backend.get('control',[])))
    return {'state':'available' if supported else 'unavailable',
            'reason':'Declared by supplied backend/profile' if supported else 'Not declared by supplied backend/profile'}


class Resolver:
    def __init__(self,root=ROOT):
        self.root = Path(root)
        self.games = {p.parent.name:load(p) for p in sorted((self.root/'games').glob('*/game.toml'))}
        self.models = {p.stem:load(p) for p in sorted((self.root/'data/guns').glob('*.toml')) if p.stem!='defaults'}
        self.gun_defaults = load(self.root/'data/guns/defaults.toml')
        self.hardware = load(self.root/'data/vocab/hardware.toml')
        self.defaults = load(self.root/'data/controls/defaults.toml')
        self.sets = {p.stem:load(p) for p in sorted((self.root/'data/controls').glob('*.toml')) if p.stem!='defaults'}
        self.vocab = vocabulary(self.root)
        require(self.defaults.get('version')=='0.1','defaults version must be 0.1')
        require(self.defaults.get('fallback') in self.sets,'unknown control set fallback')
        rules = rows_by_id(self.defaults.get('rule'),'rule')
        for rid,rule in rules.items():
            require(rule.get('gun_model') in self.models and rule.get('control_set') in self.sets,rid+': unknown model/set')
        require({r['gun_model'] for r in rules.values()}==set(self.models),'every gun model needs an explicit control-set rule')
        for sid,data in self.sets.items():
            require(data.get('id')==sid,'control set id must equal filename')
        for model in self.models.values():
            for mid,motion in model.get('motion',{}).items():
                require(text(motion.get('node')) and motion.get('kind') in ('rotate','slide'),mid+': invalid motion node/kind')
                axis,limits = motion.get('axis'),motion.get('range')
                require(isinstance(axis,list) and len(axis)==3 and all(type(v) in (int,float) and math.isfinite(v) for v in axis) and
                        abs(sum(v*v for v in axis)-1)<1e-5,mid+': motion axis must be a unit vector')
                require(isinstance(limits,list) and len(limits)==2 and all(type(v) in (int,float) and math.isfinite(v) for v in limits) and
                        limits[0]<limits[1],mid+': invalid motion range')
                drive = motion.get('drive')
                require(drive in {'trigger','recoil','pump','selector','yaw','pitch'} or
                        (isinstance(drive,str) and drive.startswith('button:') and drive[7:] in {b['id'] for b in model.get('button',[])}),
                        mid+': invalid motion drive')

    def resolve_game(self,gid,backend=None,pack_overrides=(),user_override=None,proposed_override=None):
        game = self.games[gid]
        require(game.get('genre')=='gun','requested game is not a gun game')
        model_id,model_why,review = gun_models.resolve_model(game,self.games,self.models,self.gun_defaults,self.hardware)
        model = self.models[model_id]
        validate_backend(backend,self.vocab)
        matching = next((r for r in self.defaults['rule'] if r['gun_model']==model_id),None)
        sid = matching['control_set'] if matching else self.defaults['fallback']
        data = deepcopy(self.sets[sid])
        layers = [f'data/controls/{sid}.toml']
        game_path = self.root/'games'/gid/'setup/controls.toml'
        if game_path.exists():
            layer = load(game_path)
            require(layer.get('version')=='0.1','legacy/versionless gun override cannot be silently migrated')
            data = merge(data,layer)
            layers.append(f'games/{gid}/setup/controls.toml')
        if proposed_override is not None:
            require(proposed_override.get('version')=='0.1','proposed override version must be 0.1')
            data = merge(data,proposed_override)
            layers.append('EXPLICIT SOURCE-ONLY proposed game override')
        for index,layer in enumerate(pack_overrides):
            data = merge(data,layer)
            layers.append(f'pack {index+1}')
        if user_override is not None:
            data = merge(data,user_override)
            layers.append('user override')
        separate = gid in self.gun_defaults['two_guns']['separate_views']
        eligible = min(game.get('controls',{}).get('guns',1),game.get('players',1))>=2 and not separate
        two_policy = game.get('controls',{}).get('two_guns','on_join')
        active_slots = 2 if eligible and two_policy!='off' else 1
        data['configured_slots'] = active_slots
        # Validate all known fields before selecting slots; inactive malformed
        # rows must not disappear silently. Collision checks use selected slots.
        validate_set(data,self.vocab,model,check_collisions=False)
        # Each shared set already explicitly names p1 and p2; select applicable
        # slots without inferring player identity from array order.
        data['element'] = [e for e in data.get('element',[]) if e.get('slot',0)<active_slots]
        data['unmapped_part'] = [e for e in data.get('unmapped_part',[]) if e.get('slot',0)<active_slots]
        if 'output' in data:
            data['output'] = [e for e in data['output'] if e['slot']<active_slots and e['player']<active_slots]
        path = self.root/model['model']
        node_state, nodes = 'not-built',None
        if path.exists():
            from check_gun_assets import read_glb
            doc,_ = read_glb(path)
            nodes = {n.get('name') for n in doc.get('nodes',[])}
            node_state = 'built-references-checked'
        validate_set(data,self.vocab,model,nodes)
        declared_slots = min(active_slots,backend['guns'],backend['players'],2 if backend['shared_view'] else 1) if backend is not None else 0
        for element in data['element']:
            element['availability'] = classify(element,backend,declared_slots)
        gaps = [{'category':'unmapped-part','id':p['id'],'reason':p['reason'],'node':p['node'],
                 'requested_semantic':p['requested_semantic']} for p in data.get('unmapped_part',[])]
        for slot in range(active_slots):
            if not any(e['slot']==slot and e['input']=={'kind':'gun','semantic':'trigger'} for e in data['element']):
                gaps.append({'category':'game-mapping','id':f'p{slot+1}-trigger','reason':'Configured gun slot has no fire mapping'})
        for element in data['element']:
            if element['availability']['state']!='available':
                gaps.append({'category':'backend-binding','id':element['id'],'reason':element['availability']['reason']})
        if nodes is None:
            gaps.append({'category':'asset','id':model_id,'reason':'GLB not built; actual model nodes not checked'})
        needs_cover = 'cover-pedal' in game.get('controls',{}).get('pedals',[]) or gid in self.defaults.get('required_cover',[])
        if needs_cover and not any(e['input']=={'kind':'axis','semantic':'cover_pedal'} for e in data['element']):
            gaps.append({'category':'game-mapping','id':'cover-pedal','reason':'Catalog/lead requires cover pedal; game override not yet supplied'})
        if backend is not None:
            for declaration in backend.get('control',[]):
                if declaration.get('kind') in ('gun','axis','button') and declaration.get('player',0)<active_slots and not any(
                    e['input']=={k:declaration[k] for k in ('kind','semantic')} and e['player']==declaration['player'] for e in data['element']):
                    gaps.append({'category':'declared-input','id':f"p{declaration['player']+1}-{declaration['semantic']}",
                                 'reason':'Backend declares an input with no control element'})
        return {'game_id':gid,'model':model_id,'model_provenance':model_why,'shape_review':review,
                'control_set':sid,'layers':layers,'data':data,'two_gun_eligible':eligible,
                'separate_views':separate,'configured_slots':active_slots,'declared_active_slots':declared_slots,'node_validation':node_state,
                'gaps':gaps}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--table',action='store_true')
    parser.add_argument('--json',action='store_true')
    parser.add_argument('--game')
    parser.add_argument('--allow-unmapped',action='store_true')
    parser.add_argument('--backend-declarations',type=Path,help='Explicit offline TOML declarations keyed by [game.<id>]')
    parser.add_argument('--proposed-overrides',action='store_true',help='SOURCE ONLY: explicitly evaluate candidate fragments, never a default layer')
    args = parser.parse_args()
    if args.proposed_overrides and not args.allow_unmapped:
        parser.error('--proposed-overrides requires --allow-unmapped; candidates never satisfy readiness')
    errors,rows = [],[]
    try:
        resolver = Resolver()
        declarations = load(args.backend_declarations).get('game',{}) if args.backend_declarations else {}
        gids = [args.game] if args.game else [gid for gid,g in resolver.games.items() if g.get('genre')=='gun']
        for gid in gids:
            try:
                candidate = resolver.root/'data/controls/proposed-game-overrides'/(gid+'.toml')
                proposed = load(candidate) if args.proposed_overrides and candidate.exists() else None
                rows.append(resolver.resolve_game(gid,backend=declarations.get(gid),proposed_override=proposed))
            except (OSError,ValueError,KeyError,TypeError) as exc:
                errors.append(f'{gid}: {exc}')
    except (OSError,ValueError,KeyError,TypeError) as exc:
        errors.append(str(exc))
    gaps = sum(len(r['gaps']) for r in rows)
    if args.json:
        print(json.dumps({'acceptance':'source-only' if args.allow_unmapped else 'completeness',
                          'games':rows,'errors':errors,'gap_count':gaps},indent=2,ensure_ascii=False))
    else:
        if args.table:
            for row in rows:
                print(f"{row['game_id']:48} {row['model']:22} {row['control_set']:29} slots={row['configured_slots']} gaps={len(row['gaps'])} nodes={row['node_validation']}")
        print(f'{len(rows)} gun games resolve; {gaps} open mapping/backend/asset gaps; {len(errors)} errors')
        if args.allow_unmapped:
            print('SOURCE ONLY: unresolved bindings/parts/assets are not acceptance')
        for error in errors:
            print('error '+error)
    return bool(errors or (gaps and not args.allow_unmapped))


if __name__ == '__main__':
    sys.exit(main())
