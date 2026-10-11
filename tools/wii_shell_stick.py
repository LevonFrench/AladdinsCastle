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

def source(root=ROOT):
    with (Path(root)/'data/controls/wii/shell-and-stick.toml').open('rb') as f: data=tomllib.load(f)
    if data['version']!='shell-stick/1': raise ValueError('Unknown shell control schema')
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
            profile[a['ini']]=native_expression(expression(a,mode),hand,data) if route=='dolphinxr' else dsu(a['wire'],device)
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
    return output.getvalue().encode('utf-8')

def bridge_profile(mode='split',primary_hand='right',root=ROOT):
    data=source(root); players=[]
    for slot in range(2 if mode=='two-remotes' else 1):
        firing,other=hands(mode,primary_hand,slot)
        players.append(dict(player=slot,remote_hand=firing,stick_hand=other,actions=[dict(id=a['id'],role=a['role'],wire=a['wire'],expression=expression(a,mode),hand=firing if a['role']=='remote' else other) for a in data['action']]))
    return dict(version=data['version'],route='flat-dolphin',mode=mode,primary_hand=primary_hand,players=players,
                recenter_chord=data['recenter_chord'],recenter_suppresses_game_buttons=True,
                motion=dict(physical=True,button_fallback=True,button_transport='Shake/Swing bindings',units=dict(accel='g',gyro='degrees_per_second')))

def generated_files(root=ROOT):
    result={}
    for mode in MODES:
        for hand in ('right','left'):
            for route in ROUTES: result[f'{route}/{mode}-{hand}.ini']=render_profile(route,mode,hand,root)
            result[f'flat-dolphin/{mode}-{hand}.json']=(json.dumps(bridge_profile(mode,hand,root),indent=2)+'\n').encode()
    hashes={name:hashlib.sha256(data).hexdigest() for name,data in result.items()}
    result['generated-files.json']=(json.dumps(dict(version='shell-stick/1',source='data/controls/wii/shell-and-stick.toml',files=hashes),indent=2)+'\n').encode()
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
    print('shell-stick/1: 12 controller settings, 6 DSU bridge profiles; generated bytes verified')
