"""Compile shell-stick/1 data. No application, device, install or input I/O."""
from copy import deepcopy
import configparser
import hashlib
import io
import json
from pathlib import Path
import re
import tomllib

ROOT=Path(__file__).resolve().parents[1]
MODES=('split','docked','two-remotes')
ROUTES=('dolphinxr','flat-dolphin')
TOKENS={'trigger','grip','click','primary','secondary','up','down','left','right'}
CANONICAL_LIVE_SOURCE_TESTED=True # Lead's 18 CPU/loopback checks and independent source review.

def source(root=ROOT):
    with (Path(root)/'data/controls/wii/shell-and-stick.toml').open('rb') as f: data=tomllib.load(f)
    if data['version']!='shell-stick/1': raise ValueError('Unknown shell control schema')
    product=json.loads((Path(root)/'data/product.json').read_text(encoding='utf-8'))
    if product['schema']!=1 or not re.fullmatch(r'[A-Za-z0-9_-]+',product['dsu_server_name']):raise ValueError('Invalid DSU product identity')
    data['dsu']['server']=product['dsu_server_name']
    data['dsu']['device']=data['dsu']['device'].replace('{server}',data['dsu']['server'])
    ids=set(); outputs=set()
    for a in data['action']:
        if a['id'] in ids or a['ini'] in outputs: raise ValueError('Duplicate shell action/output')
        ids.add(a['id']); outputs.add(a['ini'])
        for mode in ('split','dual'): terms(a[mode])
    return data

def terms(expression):
    result=[]
    for term in expression.split(' & '):
        name=term.lstrip('!')
        if name not in TOKENS or term not in (name,'!'+name): raise ValueError('Invalid binding token: '+term)
        result.append((name,term.startswith('!')))
    return result

def evaluate(expression, inputs):
    # Match Dolphin boolean AND/NOT for button/directional states.
    return min((1-float(inputs.get(n,0)) if neg else float(inputs.get(n,0))) for n,neg in terms(expression))

def hands(mode,primary,slot=0):
    if mode not in MODES or primary not in ('left','right') or slot not in (0,1): raise ValueError('Invalid hold/hand/player')
    firing=primary if slot==0 else ('left' if primary=='right' else 'right')
    return firing, firing if mode=='two-remotes' else ('left' if firing=='right' else 'right')

def expression(action,mode): return action['dual' if mode=='two-remotes' else 'split']

def dsu_action_supported(action,data):
    # Current Pad payload emits its 16-bit button mask and analog sticks.
    # PS/Home is a separate byte which this producer always writes as zero.
    return action['wire'] in data['dsu']['scripted_buttons'] or action['wire'] in ('Left X+','Left X-','Left Y+','Left Y-')

def native_expression(expr,hand,data):
    def token(n):
        if n in ('primary','secondary'): return '`'+data['native'][hand+'_'+n]+'`'
        value=data['native'][n]
        if n=='grip': return '(`'+hand.title()+' Button Squeeze` | `'+hand.title()+' Squeeze`)'
        return '`'+value.format(hand=hand.title())+'`'
    result=' & '.join(('!' if neg else '')+token(n) for n,neg in terms(expr))
    return '('+result+') & !(`Right Button Thumbstick` & `Left Button Thumbstick`)'

def render_profile(route='dolphinxr',mode='split',primary_hand='right',root=ROOT):
    if route not in ROUTES: raise ValueError('Unknown input route')
    if route=='dolphinxr' and mode=='two-remotes': raise ValueError('Native two-remotes unsupported: aim routing is global, not per slot')
    data=source(root); count=2 if mode=='two-remotes' else 1
    sections={}
    for slot in range(count):
        firing,other=hands(mode,primary_hand,slot)
        remote=0 if firing=='right' else 1
        stick=remote if mode=='two-remotes' else 1-remote
        profile={'Source':'3' if route=='dolphinxr' else '1','Device':data['native']['device'] if route=='dolphinxr' else data['dsu']['device'].format(slot=remote),
                 'Extension':'Nunchuk','Extension/Attach MotionPlus':'False','IMUIR/Enabled':'False'}
        def dsu(name,device):
            prefix='' if device==remote else data['dsu']['device'].format(slot=device)+':'
            return '`'+prefix+name+'`'
        for a in data['action']:
            hand=firing if a['role']=='remote' else other
            device=remote if a['role']=='remote' else stick
            profile[a['ini']]=native_expression(expression(a,mode),hand,data) if route=='dolphinxr' else dsu(a['wire'],device) if dsu_action_supported(a,data) else ''
        for direction in ('Up','Down','Left','Right','Forward','Backward'):
            for prefix,hand,device in (('',firing,remote),('Nunchuk/',other,stick)):
                name=data['native']['accel'].format(hand=hand.title(),direction=direction)
                profile[prefix+'IMUAccelerometer/'+direction]='`'+name+'`' if route=='dolphinxr' else dsu(data['dsu']['accel'].format(direction=direction),device)
        for direction in ('Pitch Up','Pitch Down','Roll Left','Roll Right','Yaw Left','Yaw Right'):
            profile['IMUGyroscope/'+direction]='`'+data['native']['gyro'].format(hand=firing.title(),direction=direction)+'`' if route=='dolphinxr' else dsu(data['dsu']['gyro'].format(direction=direction),remote)
        if route=='flat-dolphin':
            profile.update({'IR/Relative Input':'False','IR/Auto-Hide':'False','IR/Total Yaw':'77.319617','IR/Total Pitch':'48.455491'})
            for direction,suffix in {'Up':'Y+','Down':'Y-','Left':'X-','Right':'X+'}.items(): profile['IR/'+direction]=dsu('Right '+suffix,remote)
        sections['Wiimote'+str(slot+1)]=profile
    for i in range(count,4): sections['Wiimote'+str(i+1)]={'Source':'0'}
    sections['BalanceBoard']={'Source':'0'}
    config=configparser.ConfigParser(interpolation=None); config.optionxform=str; config.read_dict(sections)
    output=io.StringIO(); config.write(output)
    # Preserve canonical bytes: existing installer recipes pin these hashes.
    text=output.getvalue()
    if route=='flat-dolphin':text=text.replace('Buttons/Home = \n','Buttons/Home =\n')
    return text.encode('utf-8')

def bridge_profile(mode='split',primary_hand='right',root=ROOT):
    data=source(root); players=[]
    for slot in range(2 if mode=='two-remotes' else 1):
        firing,other=hands(mode,primary_hand,slot)
        players.append(dict(player=slot,remote_hand=firing,stick_hand=other,actions=[dict(id=a['id'],role=a['role'],wire=a['wire'],expression=expression(a,mode),hand=firing if a['role']=='remote' else other,supported=dsu_action_supported(a,data)) for a in data['action']]))
    return dict(version=data['version'],route='flat-dolphin',mode=mode,primary_hand=primary_hand,players=players,
                recenter_chord=data['recenter_chord'],recenter_suppresses_game_buttons=True,live_bridge_compatible=False,live_bridge_reason='Legacy Mapper does not consume the canonical action policy or reserved recenter chord',recenter_supported=False,
                motion=dict(physical=True,button_fallback=True,button_transport='Shake/Swing bindings',units=dict(accel='g',gyro='degrees_per_second')))

def game_policy(game_id,root=ROOT):
    if not re.fullmatch(r'[a-z0-9]+(?:-[a-z0-9]+)*',game_id): raise ValueError('Invalid game ID')
    with (Path(root)/'data/controls/wii/games'/f'{game_id}.toml').open('rb') as f: data=tomllib.load(f)
    if data['game_id']!=game_id: raise ValueError('Game policy ID mismatch')
    hold=data.get('hold',dict(default='split',state='provisional'))
    if hold['default'] not in ('split','docked'): raise ValueError('Invalid default hold')
    reload=data.get('reload',dict(choices=['shake','flick-down','offscreen','button'],selected='button',state='question',question='Does this game use reload?'))
    if reload['choices']!=['shake','flick-down','offscreen','button'] or reload['selected'] not in reload['choices']: raise ValueError('Invalid reload choices')
    if reload['state'] not in ('question','confirmed'): raise ValueError('Invalid reload evidence')
    return dict(hold=hold,reload=reload,gun_proposal=data.get('gun_proposal',{}))

def render_game_profile(game_id,route='dolphinxr',primary_hand='right',root=ROOT):
    return render_profile(route,game_policy(game_id,root)['hold']['default'],primary_hand,root)

def render_scripted_profile(game_id,primary_hand='right',root=ROOT):
    # Scripted bridge slots are roles: slot0 Remote, slot1 stick. Handedness
    # belongs to the producer descriptor, not a physical device-slot swap.
    if primary_hand not in ('left','right'): raise ValueError('Invalid primary hand')
    return render_profile('flat-dolphin',game_policy(game_id,root)['hold']['default'],'right',root)

def render_producer_profile(game_id,producer,primary_hand='right',root=ROOT):
    if producer=='native-openxr':return render_game_profile(game_id,'dolphinxr',primary_hand,root)
    if producer=='scripted-dsu':return render_scripted_profile(game_id,primary_hand,root)
    if producer=='canonical-live':
        if not CANONICAL_LIVE_SOURCE_TESTED:raise ValueError('Canonical live consumer has not passed source review')
        return render_scripted_profile(game_id,primary_hand,root)
    if producer=='legacy-steamvr':raise ValueError('Legacy live bridge is incompatible: button roles, modifier policy and recenter chord are not adopted')
    raise ValueError('Unknown input producer')

def scripted_bridge_profile(game_id,primary_hand='right',root=ROOT):
    policy=game_policy(game_id,root); mode=policy['hold']['default']
    descriptor=bridge_profile(mode,primary_hand,root)
    descriptor.update(schema='scripted-dsu/1',game_id=game_id,device_slots=dict(remote=0,nunchuk=1),server=dict(name=source(root)['dsu']['server'],host='127.0.0.1',port=26760),reload=policy['reload'])
    descriptor['live_bridge_compatible']=CANONICAL_LIVE_SOURCE_TESTED
    descriptor['live_bridge_reason']='Canonical per-game role-slot transport; game/headset validation pending' if CANONICAL_LIVE_SOURCE_TESTED else 'Canonical live consumer implementation and source review pending'
    descriptor['producer_policy']=dict(schema='canonical-dsu/1',expression_tokens=sorted(TOKENS),
        slots=dict(remote=0,nunchuk=1),unsupported_actions=['home'],offscreen_hide_supported=False,
        recenter=dict(chord=descriptor['recenter_chord'],suppress_game_buttons=True,shared_theatre_calibration=True),
        producers=dict(scripted_dsu=dict(supported=True,tracked_recenter=False),
            legacy_steamvr=dict(supported=False,reason='Legacy Mapper does not consume the canonical action policy or reserved recenter chord'),
            canonical_live=dict(supported=CANONICAL_LIVE_SOURCE_TESTED,state='source-tested' if CANONICAL_LIVE_SOURCE_TESTED else 'consumer-implementation-pending')))
    buttons=source(root)['dsu']['scripted_buttons']
    for player in descriptor['players']:
        for action in player['actions']:
            action['slot']=0 if action['role']=='remote' else 1
            wire=action['wire']
            if wire in buttons: action['script']=dict(supported=True,field='buttons',button=wire)
            elif wire.startswith('Left '):action['script']=dict(supported=True,field='stick',axis=0 if 'X' in wire else 1,value=-1 if wire.endswith('-') else 1)
            else:action['script']=dict(supported=False,reason='Not accepted by the bounded scripted DSU schema')
    descriptor['reload']['enabled']=False # choice/evidence only; classifier owned by host
    return descriptor

def generated_files(root=ROOT):
    result={}
    for mode in MODES:
        for hand in ('right','left'):
            for route in ROUTES:
                if route=='dolphinxr' and mode=='two-remotes': continue
                result[f'{route}/{mode}-{hand}.ini']=render_profile(route,mode,hand,root)
                if route=='dolphinxr':result[f'{route}/{mode}-{hand}.Dolphin.ini']=f'[Android]\nQuestLeftHanded = {"True" if hand=="left" else "False"}\n'.encode()
            result[f'flat-dolphin/{mode}-{hand}.json']=(json.dumps(bridge_profile(mode,hand,root),indent=2)+'\n').encode()
    for path in sorted((Path(root)/'data/controls/wii/games').glob('*.toml')):
        gid=path.stem;policy=game_policy(gid,root)
        for hand in ('right','left'):
            for route in ROUTES:
                result[f'games/{gid}/{route}-{hand}.ini']=render_game_profile(gid,route,hand,root)
            result[f'games/{gid}/dolphinxr-{hand}.Dolphin.ini']=f'[Android]\nQuestLeftHanded = {"True" if hand=="left" else "False"}\n'.encode()
            result[f'games/{gid}/scripted-dsu-{hand}.ini']=render_scripted_profile(gid,hand,root)
            result[f'games/{gid}/scripted-dsu-{hand}.json']=(json.dumps(scripted_bridge_profile(gid,hand,root),indent=2)+'\n').encode()
    result['scripted-dsu/DSUClient.ini']=f'[Server]\nEnabled = True\nEntries = {source(root)["dsu"]["server"]}:127.0.0.1:26760\n'.encode()
    hashes={name:hashlib.sha256(data).hexdigest() for name,data in result.items()}
    result['generated-files.json']=(json.dumps(dict(version='shell-stick/1',source='data/controls/wii/shell-and-stick.toml',files=hashes,
        capabilities=dict(dolphinxr=dict(split=True,docked=True,two_remotes=False,left_hand_requires='Android/QuestLeftHanded'),flat_dolphin=dict(split=True,docked=True,two_remotes=True,home=False,live_bridge_compatible=False,scripted_single_player=True,recenter=False),canonical_live=dict(single_player=CANONICAL_LIVE_SOURCE_TESTED,two_remotes=False,home=False,recenter=False))),indent=2)+'\n').encode()
    return result

def write_generated(root=ROOT,check=False):
    directory=Path(root)/'data/controls/wii/generated'
    for name,data in generated_files(root).items():
        path=directory/name
        if check:
            if not path.is_file() or path.read_bytes()!=data: raise ValueError('Stale generated Wii profile: '+name)
        else:
            path.parent.mkdir(parents=True,exist_ok=True); path.write_bytes(data)

if __name__=='__main__':
    import argparse
    parser=argparse.ArgumentParser(description=__doc__); parser.add_argument('--write',action='store_true')
    args=parser.parse_args(); write_generated(check=not args.write)
    print('shell-stick/1: native single-player profiles with aim-hand companion; native two-remotes rejected; generated bytes verified')
