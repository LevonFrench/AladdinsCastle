#!/usr/bin/env python3
"""System22 route-native control data and hand-label packets; never emits input.

The explicit system22-native/1 namespace keeps adopted-host actions separate
from the v0.1 arcade ABI. Existing TOML loading, keyed merge, catalog/model
selection and GLB node decoding are reused. No renderer or game files.
"""
from copy import deepcopy
import argparse,json,sys
from pathlib import Path
from control_sets import ROOT,Resolver,confined_path,integer,load,merge,require,rows_by_id,text,vocabulary

ROUTE='namco22-vr'
ROLES={'left','right','saved-primary','saved-off'}
CONTROLS={'trigger','grip','primary','secondary','thumbstick','thumbstick_click'} # menu buttons are off limits (contract v1 section 0)
NATIVE={'native-host':{'cover_mode','weapon_hand','gun_angle','gun_angle_reset','view_distance'},
        'native-cabinet':{'pedal_backward','trigger_left','trigger_right','motion_stop','trackball_x','trackball_y'}}

def physical_hand(role,primary):
    require(role in ROLES,'Unknown controller role')
    return primary if role=='saved-primary' else ('left' if primary=='right' else 'right') if role=='saved-off' else role

def touch_path(hand,control):
    token={'trigger':'trigger/value','grip':'squeeze/value','primary':('a' if hand=='right' else 'x')+'/click',
           'secondary':('b' if hand=='right' else 'y')+'/click','thumbstick':'thumbstick',
           'thumbstick_click':'thumbstick/click','pointer':'aim/pose'}[control]
    return '/user/hand/'+hand+'/input/'+token

def binding_label(hand,control,component=''):
    if control=='pointer':return 'Point gun with '+hand+' hand'
    if control=='primary':return 'A (right)' if hand=='right' else 'X (left)'
    if control=='secondary':return 'B (right)' if hand=='right' else 'Y (left)'
    return hand.capitalize()+' '+control.replace('_',' ')+(' '+('left/right' if component=='x' else 'forward/back') if component else '')

def validate(data,assignment,vocab,nodes=None):
    require(data.get('version')=='0.1' and data.get('schema')=='system22-native/1' and data.get('family')=='system22','Invalid route-native schema')
    require(data.get('route')==ROUTE,'Wrong System22 route')
    kind=assignment['control_type'];require(kind in ('gun','wheel','handlebars','trackball'),'Unknown cabinet control type')
    require(bool(assignment.get('gun_model'))==(kind=='gun'),'Gun model cannot be assigned to nongun controls')
    seen={};seen_paused={};pending_paused=[]
    for eid,e in rows_by_id(data.get('element',[]),'System22 element').items():
        require(all(text(e.get(k)) for k in ('label','part','source_action')) and isinstance(e.get('node'),str),eid+': incomplete part/action/source')
        require(e.get('slot')==0 and type(e.get('slot')) is int and e.get('player')==0 and type(e.get('player')) is int,eid+': one native cabinet/player only')
        inp=e.get('input',{});allowed=vocab|NATIVE;require(inp.get('kind') in allowed and inp.get('semantic') in allowed[inp['kind']],eid+': unknown input namespace/semantic')
        b=e.get('binding',{});require(b.get('role') in ROLES and b.get('control') in CONTROLS and b.get('mode') in ('hold','press','toggle'),eid+': invalid binding')
        require(b.get('hand')==physical_hand(b['role'],data['policy']['p1_hand']),eid+': physical hand differs from saved layout')
        component=b.get('component','');require(component in ('','x','y') and (not component or b['control']=='thumbstick'),eid+': invalid axis component')
        require(e.get('binding_evidence') in ('source','question') and e.get('when') in ('playing','focused','paused'),eid+': missing evidence/context')
        key=(b['hand'],b['control'],component)
        # Paused-only option rows may reuse a gameplay button; they never reuse the pause button itself.
        group=seen_paused if e['when']=='paused' else seen
        require(key not in group,eid+': controller collision with '+group.get(key,''));group[key]=eid
        if e['when']=='paused':pending_paused.append((eid,key))
        if e['node']:
            require(kind=='gun',eid+': nongun element cannot have a gun node')
            if nodes is not None:require(e['node'] in nodes,eid+': missing authored node '+e['node'])
        require(e.get('availability',{}).get('state')=='unavailable',eid+': source data cannot claim live availability')
    for eid,key in pending_paused:require(seen.get(key)!='pause',eid+': paused option cannot reuse the pause button')
    # Universal layer (contract v1 U1/U2): every game maps Start and Coin to the stick clicks, and nothing else may use them.
    by_sem={e['input']['semantic']:e for e in data['element'] if e['input']['kind']=='button'}
    for sem,hand in (('start','left'),('coin','right')):
        b=by_sem.get(sem,{}).get('binding',{});require(b.get('control')=='thumbstick_click' and b.get('hand')==hand and b.get('mode')=='press','Universal controls: '+sem+' must be the '+hand+' stick click')
    require(sorted(e['id'] for e in data['element'] if e['binding']['control']=='thumbstick_click')==sorted(e['id'] for e in by_sem.values() if e['input']['semantic'] in ('start','coin')),'Stick clicks belong to the universal layer only')
    if kind=='gun':
        require(assignment['gun_model']=='arc-pistol-slide','TC route must retain the assigned existing gun')
        require(not any(e['input']['semantic']=='reload' for e in data['element']),'Pinned TC1 has no separate offscreen reload input')
        require({e['binding']['hand'] for e in data['element'] if e['input']=={'kind':'gun','semantic':'trigger'}}=={'left','right'},'Both trigger handoff controls required')
        require({e['binding']['hand'] for e in data['element'] if e['input']['semantic']=='cover_pedal'}=={'left','right'},'Either grip cover controls required')
        require(data['policy']['two_guns']=='off' and data['policy']['native_players']==1,'Trigger handoff must not create P2')
        for row in data.get('label',[]):
            require(text(row.get('id')) and text(row.get('label')) and text(row.get('node')),'Invalid gun label')
            if nodes is not None:require(row['node'] in nodes,'Gun label references missing authored node')

def resolve_game(root,gid,primary_hand='right'):
    require(primary_hand in ('left','right'),'Primary hand must be left or right')
    root=Path(root);spec=load(root/'data/controls/system22/controls.toml')
    require(spec.get('version')=='0.1' and spec.get('schema')=='system22-native/1' and spec.get('route')==ROUTE,'Invalid System22 library')
    require(set(spec['order'])==set(spec['assignments']) and len(spec['order'])==len(set(spec['order'])),'D63 assignment order must cover each game once')
    require(gid in spec['assignments'],'Game not assigned to the System22 route')
    assignment=deepcopy(spec['assignments'][gid]);game=load(confined_path(root,'games/'+gid+'/game.toml'))
    require(game['id']==gid and game['hardware'] in ('namco-system-22','namco-super-system-22'),'Catalog is not the assigned System22 game')
    sid=assignment['control_set'];require(sid in spec['sets'],'Unknown rule control set')
    data=merge(spec['sets'][sid],assignment)
    data.update(version='0.1',schema='system22-native/1',family='system22',route=ROUTE,id='system22-'+sid,
                title=game['title']+' controls',policy={'p1_hand':primary_hand,'two_guns':'off','native_players':1,'hand_switch':'trigger' if assignment['control_type']=='gun' else 'off'})
    model=assignment['gun_model'];meta={};nodes=None;gaps=[]
    if model:
        base=Resolver(root).resolve_game(gid);require(base['model']==model,'System22 assignment disagrees with existing gun defaults')
        meta=base['model_metadata'];asset=confined_path(root,meta['model'])
        if asset.exists():
            from check_gun_assets import read_glb
            doc,_=read_glb(asset);nodes={n['name'] for n in doc['nodes']}
        else:gaps.append({'category':'asset','id':model,'reason':'Existing gun asset not built'})
        gaps.append({'category':'runtime-integration','id':'adopted-hand-model-labels','reason':data['attachment']['state']})
    else:
        require(game.get('genre')!='gun','Gun game cannot lose its gun model')
        gaps.append({'category':'control-model','id':assignment['control_type'],'reason':'Appropriate cabinet model not built; no gun substitute'})
    for e in data['element']:
        b=e['binding'];b['hand']=physical_hand(b['role'],primary_hand)
        e['availability']={'state':'unavailable','reason':'Source mapping only; adopted-route live/controller acceptance pending' if e['binding_evidence']=='source' else 'Controller binding is a question; VR cabinet bridge not implemented'}
        if e['binding_evidence']=='source':b['source_openxr_path']=touch_path(b['hand'],b['control'])
        gaps.append({'category':'route-binding','id':e['id'],'reason':e['availability']['reason']})
    validate(data,assignment,vocabulary(root),nodes)
    return {'game_id':gid,'model':model,'model_provenance':'existing gun defaults' if model else 'cabinet control type from pinned source',
            'shape_review':False,'control_set':data['id'],'control_type':assignment['control_type'],
            'layers':['data/controls/system22/controls.toml:sets.'+sid,'data/controls/system22/controls.toml:assignments.'+gid],
            'data':data,'model_metadata':meta,'two_gun_eligible':False,'separate_views':False,'configured_slots':1 if model else 0,
            'declared_active_slots':0,'node_validation':'source-built-references-checked' if nodes is not None else 'not-built' if model else 'not-applicable-no-control-model',
            'gaps':gaps}

def hand_labels(result,active_hand=None,lod=0):
    require(type(lod) is int and lod in (0,1),'LOD must be0 or1')
    data=result['data'];require(result['control_type']=='gun','Only a real assigned gun has gun-node labels')
    primary=data['policy']['p1_hand'];active=active_hand or primary;require(active in ('left','right'),'Active firing hand must be left or right')
    entries={e['id']:e for e in data['element']};labels=[];claimed=set()
    for row in data['label']:
        eid=row.get('element_by_hand',{}).get(active,row.get('element'))
        b=entries[eid]['binding'] if eid else {'hand':active,'control':'pointer','source_openxr_path':touch_path(active,'pointer')}
        if eid:claimed.add(eid)
        labels.append({'id':row['id'],'part':row.get('part',row['id']),'game_action':row['label'],'anchor':{'kind':'gun-node','node':row['node']+('_lod1' if lod==1 and row['node']=='pivot_trigger' else ''),'physical_hand':active},
                       'controller_hand':b['hand'],'controller_button':binding_label(b['hand'],b['control'],b.get('component','')),'source_openxr_path':b['source_openxr_path']})
    for eid,e in entries.items():
        if eid in claimed:continue
        b=e['binding'];labels.append({'id':eid,'part':e['part'],'game_action':e['label'],'anchor':{'kind':'controller','physical_hand':b['hand']},
                                    'controller_hand':b['hand'],'controller_button':binding_label(b['hand'],b['control'],b.get('component','')),
                                    'source_openxr_path':b.get('source_openxr_path',''),'when':e['when']})
    return {'schema':'system22-hand-labels/1','game_id':result['game_id'],'gun_model':result['model'],'player':0,'gun_slot':0,'saved_primary_hand':primary,
            'active_gun_hand':active,'lod':lod,'labels':labels,'attachment':deepcopy(data['attachment']),'acceptance':'source-only; no label draw consumer or measured grip transform claimed'}

def catalog(root=ROOT):
    spec=load(Path(root)/'data/controls/system22/controls.toml')
    return {'version':'0.1','schema':'system22-native/1','route':ROUTE,'source_only':True,'games':[resolve_game(root,g) for g in spec['order']],
            'deferred_tc2':spec['deferred_tc2']}

def main():
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('--game');p.add_argument('--primary-hand',choices=('left','right'),default='right')
    p.add_argument('--active-hand',choices=('left','right'));p.add_argument('--lod',type=int,choices=(0,1),default=0);p.add_argument('--hand-labels',action='store_true');p.add_argument('--table',action='store_true')
    p.add_argument('--export-view-catalog',type=Path);a=p.parse_args()
    try:
        if a.hand_labels:
            require(a.game is not None and not a.export_view_catalog,'Choose one gun game for a hand-label packet')
            value=hand_labels(resolve_game(ROOT,a.game,a.primary_hand),a.active_hand,a.lod)
        else:
            require(a.active_hand is None and a.lod==0,'Active hand/LOD only applies to hand labels')
            value=catalog() if not a.game else {'games':[resolve_game(ROOT,a.game,a.primary_hand)],'source_only':True}
        if a.export_view_catalog:
            require(a.game is None and a.primary_hand=='right','Compiled catalog is canonical baseline only')
            a.export_view_catalog.parent.mkdir(parents=True,exist_ok=True);a.export_view_catalog.write_text(json.dumps(value,indent=2,ensure_ascii=False)+'\n',encoding='utf-8')
        if a.table:
            for row in value['games']:print(f"{row['game_id']:12} {row['control_type']:12} {row['model'] or '(no gun)':20} {row['control_set']} gaps={len(row['gaps'])}")
            print('SOURCE ONLY: controller/model/runtime gaps remain; no input emitted')
        else:print(json.dumps(value,indent=2,ensure_ascii=False))
        return 0
    except (OSError,ValueError,KeyError,TypeError) as e:print(str(e),file=sys.stderr);return 1

if __name__=='__main__':raise SystemExit(main())
