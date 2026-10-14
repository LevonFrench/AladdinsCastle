# SPDX-License-Identifier: GPL-3.0-only
"""Author legacy cabinet TOML and its original light/dark vector choice cards.

This writes source data/art only; it does not launch an emulator or send input.
The existing legacy element/output/extractor format and holds tables are reused.
"""
from pathlib import Path
import argparse
import html
import json
import tomllib
import textwrap

ROOT=Path(__file__).resolve().parents[1]
SETS=ROOT/'data/controls/cabinets'
SETS.mkdir(parents=True,exist_ok=True)

def value(v):
    if isinstance(v,bool):return 'true' if v else 'false'
    if isinstance(v,str):return json.dumps(v,ensure_ascii=False)
    if isinstance(v,dict):return '{ '+', '.join(k+' = '+value(a) for k,a in v.items())+' }'
    if isinstance(v,list):return '['+', '.join(value(a) for a in v)+']'
    return str(v)

def write_utf8(path,text):
    """Encode Unicode source exactly once at the file boundary (no locale codec)."""
    path.parent.mkdir(parents=True,exist_ok=True)
    path.write_bytes(text.encode('utf-8'))

def element(e):
    return '\n[[element]]\n'+'\n'.join(k+' = '+value(v) for k,v in e.items())+'\n'

def place_instances(rows):
    """Shared model parts retain one root position per player, not duplicate rigs."""
    origins={}
    for e in rows:
        if not e.get('model'):continue
        # This GLB contains both players' pumps and pedals on one shared deck.
        shared=e['model']=='pump-handcar'
        key=(e['model'],'shared' if shared else e.get('player',0))
        if key not in origins:origins[key]=e.get('position',[0,.85,-.40])
        e['model_instance']=e['model']+'-shared' if shared else e['model']+'-p'+str(e.get('player',0)+1)
        e['position']=origins[key]
        node=e.get('node','')
        labels={'pivot_wheel':'label_steering','pivot_shift':'label_shifter','pivot_handbrake':'label_handbrake',
                'pivot_accelerator':'label_accelerator','pivot_brake':'label_brake','pivot_pedal':'label_pedal',
                'pivot_steering':'label_steering','pivot_throttle':'label_throttle','pivot_front_brake':'label_front_brake',
                'pivot_swing':'label_swing','pivot_edge':'label_edge'}
        if e['model']=='stick-flight':e['label_node']={'pivot_trigger':'label_trigger','button_event':'label_event'}.get(node,'label_stick')
        elif e['model']=='stick-arcade':e['label_node']='label_joystick'
        elif e['model'].startswith('panel-action-'):e['label_node']=node.replace('button_action','label_action')
        elif e['model'] in ('arc-pistol-twin','mnt-mg-heavy'):e['label_node']=node
        else:e['label_node']=labels.get(node,node.replace('button_view','label_view').replace('button_select','label_select').replace('pivot_pump','label_pump'))
        if e.get('grab',{}).get('required'):
            anchors={'wheel-three-spoke':['grab_left','grab_right'],'shifter-four-gate':['grab_knob'],
                     'shifter-sequential':['grab_knob'],'handbrake-lever':['grab_handle'],
                     'bars-motorcycle':['grip_left','grab_right'],'arc-pistol-twin':['grip'],'mnt-mg-heavy':['grip','grip_two'],
                     'stick-flight':['grab_handle'],'stick-arcade':['grab_knob']}
            e['grab']['anchors']=anchors.get(e['model'],[node.replace('pivot_pump','grab_handle')])
            if e['model']=='mnt-mg-heavy':
                e['grab']['anchors']=['grab_right','grab_left'] if e['source']=='both_hands' else ['grab_right']
    return rows

def base_element(eid,label,kind,source,node,output,**fields):
    return dict(id=eid,label=label,type=kind,source=source,node=node,output=output,**fields)

def common_buttons():
    return [base_element('start','Start','button','right_primary','',{'button':'Start1'},binding={'mode':'press'}),
            base_element('coin','Insert credit','button','right_secondary','',{'button':'Coin1'},binding={'mode':'press'})]

def racer(steer,shift,views=4,handbrake=False):
    modes={'grab-wheel':('both_hands','bar_roll'), 'tilt-wheel':('left_hand','grip_twist'),'thumbstick':('left_thumbstick','stick')}
    source,extractor=modes[steer]
    rows=[base_element('wheel','Steering','wheel',source,'pivot_wheel',{'axis':'Steering','range':[0,255],'centre':128,'invert':False},
        model='wheel-three-spoke',position=[0,.78,-.42],tilt_deg=25,radius_m=.176,range_deg=270,
        extractor=extractor,axes={'component':'x' if steer=='thumbstick' else 'wheel_normal','positive':'clockwise/right'},
        calibration={'centre':'before_launch','range':[-45,45] if steer=='tilt-wheel' else [-135,135]},grab={'required':steer!='thumbstick','button':'grip','radius_m':.06,'soft_lock':True,'preserve_current_angle':True,'one_hand_fallback':'angle_about_hub'},
        curve={'deadzone':.025,'gamma':1},spring={'return_speed':6},tracking_loss='freeze_then_neutral',
        haptics={'centre_detent':.2})]
    for eid,hand,x in [('accelerator','right',.07),('brake','left',-.07)]:
        rows.append(base_element(eid,eid.title(),'pedal',hand+'_trigger','pivot_'+eid,{'axis':eid.title(),'range':[0,255],'invert':False},
            model='pedals-dual',position=[0,.10,-.48],extractor='trigger',curve={'deadzone':.04,'gamma':1.4},axes={'positive':'squeeze'},grab={'required':False}))
    rows.append(base_element('shifter','Four-speed gear','shifter_h','right_hand' if shift=='lever' else 'right_thumbstick','pivot_shift',
        {'gear_1':'GearShift1','gear_2':'GearShift2','gear_3':'GearShift3','gear_4':'GearShift4','neutral':'GearShiftN'},
        model='shifter-four-gate',position=[.26,.55,-.25],extractor='lever_slide' if shift=='lever' else 'stick',
        axes={'plane':'xz','forward':'negative_z'},range_m={'x':[-.037,.037],'z':[-.054,.054]},
        grab={'required':True,'button':'right_grip','radius_m':.055,'soft_lock':True},
        gate={'forward_left':1,'back_left':2,'forward_right':3,'back_right':4,'neutral_band_m':.012,'snap':True,'latch_on_release':True},
        binding={'modifier':'right_grip','directions':['up','down','right','left'],'neutral':'stick_click'} if shift=='buttons' else {'mode':'hold'},
        haptics={'gear_detent':.25}))
    for i,direction in enumerate(('up','right','down','left'),1):
        rows.append(base_element('view-'+str(i),'View '+str(i),'button','right_thumbstick','button_view_'+str(i),
            {'button':'VR'+str(i)},model='panel-view-four',position=[0,.92,-.49],
            binding={'direction':direction,'threshold':.65,'modifier':'right_grip_released','mode':'press'},axes={'positive':direction}))
    return rows+common_buttons()

def diagram():
    return '''<g transform="translate(15,0)">
    <rect x="40" y="150" width="388" height="365" rx="40" fill="url(#metal)" opacity=".18"/>
    <g stroke="var(--muted)" stroke-width="3"><rect x="97" y="139" width="278" height="50" rx="14" fill="var(--plate)"/>
    <g fill="var(--teal)"><rect x="110" y="150" width="46" height="26" rx="7"/><rect x="175" y="150" width="46" height="26" rx="7"/><rect x="240" y="150" width="46" height="26" rx="7"/><rect x="305" y="150" width="46" height="26" rx="7"/></g></g>
    <g stroke-linecap="round"><circle cx="217" cy="290" r="93" fill="none" stroke="var(--rubber)" stroke-width="24"/>
    <path d="M217 290L137 244M217 290L297 244M217 290V379" stroke="url(#metal)" stroke-width="19"/>
    <circle cx="217" cy="290" r="30" fill="var(--plate)" stroke="var(--teal)" stroke-width="5"/></g>
    <rect x="326" y="338" width="79" height="88" rx="13" fill="var(--plate)" stroke="var(--muted)" stroke-width="3"/>
    <path d="M346 351V411M383 351V411M346 382H383" stroke="var(--rubber)" stroke-width="10" fill="none"/>
    <path d="M365 382V328" stroke="url(#metal)" stroke-width="12"/><ellipse cx="365" cy="325" rx="20" ry="23" fill="var(--rubber)"/>
    <g stroke="var(--muted)" stroke-width="3"><path d="M126 444H214L225 521H113Z" fill="url(#metal)"/><path d="M253 444H314L324 521H243Z" fill="url(#metal)"/></g>
    <path d="M129 460H211M126 477H216M124 495H219M254 460H313M250 477H317M248 495H320" stroke="var(--rubber)" stroke-width="8"/>
    </g>'''

def svg(title,explanation,steer,shift,theme):
    dark=theme=='dark';bg='#101d2b' if dark else '#f6f8fb';fg='#edf5fc' if dark else '#15283d';muted='#8aa4bc' if dark else '#53718b'
    steering={'grab-wheel':'Grip the rim; turn with both hands.','tilt-wheel':'Hold left grip; tilt your left hand.','thumbstick':'Move the left thumbstick left/right.'}[steer]
    shifting='Grip the knob; move through the H gate.' if shift=='lever' else 'Hold right grip + stick: up 1, down 2, right 3, left 4.'
    texts=[('1','STEER',steering),('2','PEDALS','Right trigger = gas; left trigger = brake.'),('3','GEARS',shifting),('4','VIEWS','Release right grip; right stick selects views 1\u20134.'),('5','START / CREDIT','A = Start; B = insert credit.')]
    labels=''
    for idx,(n,heading,text) in enumerate(texts):
        y=218+idx*75;labels+=f'<circle cx="496" cy="{y-8}" r="14" fill="#31c8bb"/><text x="496" y="{y-2}" text-anchor="middle" fill="#102233" font-size="18" font-weight="700">{n}</text><text x="522" y="{y-8}" font-size="24" font-weight="700">{heading}</text>'
        for j,line in enumerate(textwrap.wrap(text,43)):labels+=f'<text x="522" y="{y+18+j*24}" font-size="22">{html.escape(line)}</text>'
    dots=''.join(f'<circle cx="{x}" cy="{y}" r="17" fill="#31c8bb"/><text x="{x}" y="{y+6}" text-anchor="middle" fill="#102233" font-size="19" font-weight="700">{n}</text>' for n,x,y in [(1,92,285),(2,325,508),(3,420,379),(4,380,155)])
    return f'''<svg xmlns="http://www.w3.org/2000/svg" width="1080" height="660" viewBox="0 0 1080 660">
    <defs><linearGradient id="metal"><stop stop-color="#64798e"/><stop offset=".4" stop-color="#d7e2eb"/><stop offset="1" stop-color="#748ca0"/></linearGradient></defs>
    <style>svg{{--muted:{muted};--plate:{'#244151' if dark else '#cfdee8'};--rubber:{'#111821' if dark else '#243748'};--teal:#31c8bb}}text{{font-family:Arial,sans-serif;fill:{fg}}}</style>
    <rect width="1080" height="660" rx="22" fill="{bg}"/><rect x="32" y="30" width="5" height="69" rx="2" fill="#31c8bb"/>
    <text x="54" y="57" font-size="30" font-weight="700">{html.escape(title)}</text><text x="54" y="92" font-size="20">{html.escape(explanation)}</text>
    {diagram()}{dots}{labels}
    <text x="50" y="602" font-size="18">Seated \u2022 grab deliberately \u2022 release to recenter \u2022 pedals need no grab</text>
    <text x="50" y="630" font-size="17" fill="{muted}">Choose before launch. Calibrate the wheel to your comfortable reach; no body lean is required.</text></svg>'''

def write_game(gid,choices):
    game=tomllib.loads((ROOT/'games'/gid/'game.toml').read_text(encoding='utf-8'))
    selected=choices[0][0]
    baseline=(SETS/(selected+'.toml')).read_text(encoding='utf-8')
    baseline=baseline.replace('id = '+value(selected),'id = '+value('model3-'+gid),1).replace('title = '+value(choices[0][1]),'title = '+value(game['title']+' cabinet'),1)
    baseline+='\n[hold]\ndefault = '+value(selected)+'\n'
    for sid,title,explanation in choices:
        baseline+='\n[holds.'+sid+']\n'+'\n'.join(k+' = '+value(v) for k,v in dict(title=title,description=explanation,control_set=sid,default=sid==selected,picture_light='assets/controls/cards/'+sid+'-light.svg',picture_dark='assets/controls/cards/'+sid+'-dark.svg').items())+'\n'
    target=SETS/'games'/(gid+'.toml');write_utf8(target,baseline)
    if game.get('controls',{}).get('type')!='gun':
        # Also publish the conventional legacy cabinet file for nongun games.
        # Existing v0.1 gun resolution must not absorb native ADC legacy rows.
        p=ROOT/'games'/gid/'setup/controls.toml';write_utf8(p,baseline)

def save_choice(sid,title,explanation,rows,art):
    text='# SPDX-License-Identifier: GPL-3.0-only\n# Existing versionless cabinet format from config-spec section 5.\n'
    text+='id = '+value(sid)+'\ntitle = '+value(title)+'\nseat = { height_m = 0.55, posture = "seated" }\n'+''.join(element(e) for e in place_instances(rows))
    write_utf8(SETS/(sid+'.toml'),text)
    for theme in ('light','dark'):write_utf8(ROOT/'assets/controls/cards'/(sid+'-'+theme+'.svg'),art(theme))
    return sid,title,explanation

def write_racers():
    choices=[]
    for steer in ('grab-wheel','tilt-wheel','thumbstick'):
        for shift in ('lever','buttons'):
            sid='cab-wheel-h-'+steer+'-'+shift
            title={'grab-wheel':'Grab the wheel','tilt-wheel':'Tilt one controller','thumbstick':'Thumbstick steering'}[steer]+' / '+('grabbed shifter' if shift=='lever' else 'gear buttons')
            explanation={'grab-wheel':'Turn a wheel held with both hands, with analog triggers for the pedals.','tilt-wheel':'Steer by tilting your left controller, with analog triggers for the pedals.','thumbstick':'Steer with your left thumbstick, with analog triggers for the pedals.'}[steer]
            rows=racer(steer,shift)
            text='# SPDX-License-Identifier: GPL-3.0-only\n# Existing versionless cabinet format from config-spec section 5.\n'
            text+='id = '+value(sid)+'\ntitle = '+value(title)+'\nseat = { height_m = 0.55, posture = "seated" }\n'
            text+=''.join(element(e) for e in place_instances(rows))
            write_utf8(SETS/(sid+'.toml'),text)
            for theme in ('light','dark'):write_utf8(ROOT/'assets/controls/cards'/(sid+'-'+theme+'.svg'),svg(title,explanation,steer,shift,theme))
            choices.append((sid,title,explanation))
    for gid in ('scud','daytona2'):write_game(gid,choices)

def family_card(title,explanation,picture,labels,theme):
    bg,fg,plate,rubber=('#102030','#edf6fc','#274659','#09151e') if theme=='dark' else ('#f5f8fc','#142f44','#c5dbe7','#253c4e')
    text=''
    for i,(heading,binding) in enumerate(labels):
        y=195+i*77
        text+=f'<circle cx="485" cy="{y}" r="15" fill="#31c8bb"/><text x="485" y="{y+7}" text-anchor="middle" font-size="20" fill="#102030">{i+1}</text><text x="515" y="{y-1}" font-size="24" font-weight="700">{html.escape(heading)}</text>'
        for j,line in enumerate(textwrap.wrap(binding,42)):text+=f'<text x="515" y="{y+27+j*25}" font-size="23">{html.escape(line)}</text>'
    height=max(740,230+len(labels)*77)
    return f'''<svg xmlns="http://www.w3.org/2000/svg" width="1080" height="{height}" viewBox="0 0 1080 {height}">
    <defs><linearGradient id="metal"><stop stop-color="#6b859c"/><stop offset=".45" stop-color="#d9e7ef"/><stop offset="1" stop-color="#758a9e"/></linearGradient></defs>
    <style>text{{font-family:Arial,sans-serif;fill:{fg}}}svg{{--plate:{plate};--rubber:{rubber};--muted:#7192aa;--teal:#31c8bb}}</style>
    <rect width="1080" height="{height}" rx="20" fill="{bg}"/><text x="35" y="52" font-size="31" font-weight="700">{html.escape(title)}</text>
    <text x="35" y="92" font-size="22">{html.escape(explanation)}</text>{picture}{text}
    <text x="35" y="{height-38}" font-size="23">Choose before launch • grip to grab • calibrate reach • pause cancels input</text></svg>'''

def bike_picture():
    return '''<g transform="translate(25,80)" stroke-linecap="round">
    <rect x="150" y="220" width="120" height="130" rx="42" fill="var(--plate)"/>
    <path d="M210 255V170M210 170L125 145L65 165M210 170L295 145L365 165" stroke="url(#metal)" stroke-width="19" fill="none"/>
    <path d="M38 171L100 153M329 155L397 177" stroke="var(--rubber)" stroke-width="30"/>
    <path d="M295 165L330 205L390 218" stroke="url(#metal)" stroke-width="12" fill="none"/>
    <path d="M150 367L176 407H234L270 367" fill="url(#metal)"/><path d="M183 378H244M188 392H237" stroke="var(--rubber)" stroke-width="8"/>
    <g fill="#31c8bb"><circle cx="210" cy="152" r="18"/><circle cx="387" cy="158" r="18"/><circle cx="354" cy="229" r="18"/><circle cx="274" cy="398" r="18"/></g>
    <g font-size="22" text-anchor="middle"><text x="210" y="159">1</text><text x="387" y="165">2</text><text x="354" y="236">3</text><text x="274" y="405">4</text></g></g>'''

def write_step2():
    for family,ids in [('sequential-four',['lemans24']),('sequential-one',['dirtdvls','eca']),('rally',['srally2'])]:
        choices=[]
        for steer in ('grab-wheel','tilt-wheel','thumbstick'):
            for shift in ('lever','buttons'):
                sid='cab-'+family+'-'+steer+'-'+shift; title=family.replace('-',' ').title()+' / '+steer.replace('-',' ')+' / '+shift
                explanation='Steer comfortably, with trigger pedals and '+('a grabbed gear lever.' if shift=='lever' else 'thumbstick gear buttons.')
                rows=racer(steer,shift)
                if family.startswith('sequential'):
                    gear=next(e for e in rows if e['id']=='shifter');gear.update(type='shifter_seq',model='shifter-sequential',label='Shift up/down',output={'up':'GearShiftUp','down':'GearShiftDown'},range_m={'z':[-.055,.055]},gate={'spring_return':True,'pulse_on_entry':True},binding={'modifier':'right_grip','up':'thumbstick_up','down':'thumbstick_down','mode':'press'} if shift=='buttons' else {'mode':'hold'})
                if family!='sequential-four':
                    rows=[e for e in rows if not e['id'].startswith('view-')]
                    rows.append(base_element('view','Change view','button','right_thumbstick_click','button_view',{'button':'ViewChange'},model='panel-view-one',binding={'mode':'press','modifier':'right_grip_released'}))
                if family=='rally':
                    rows.append(base_element('handbrake','Handbrake','lever','right_hand' if shift=='lever' else 'left_primary','pivot_handbrake',{'button':'HandBrake','range':[0,1]},model='handbrake-lever',position=[.32,.53,-.20],extractor='lever_slide' if shift=='lever' else 'button',range_m=[0,.15],axes={'positive':'pull_up/back'},grab={'required':shift=='lever','button':'grip','soft_lock':True},binding={'threshold':.55,'mode':'hold'},fallback_binding={'hand':'left','control':'primary','mode':'hold'}))
                labels=[('STEERING',{'grab-wheel':'Grip rim and turn both hands.','tilt-wheel':'Grip left controller and tilt ±45°.','thumbstick':'Left thumbstick left / right.'}[steer]),('GAS / BRAKE','Right trigger gas; left trigger brake.'),('GEARS','Grip knob and move lever.' if shift=='lever' else 'Right grip + right stick selects gear.'),('VIEW','Right stick directions, grip released.' if family=='sequential-four' else 'Click the right thumbstick.'),('START / CREDIT','A starts; B inserts credit.')]
                picture=diagram()+''.join(f'<circle cx="{x}" cy="{y}" r="17" fill="#31c8bb"/><text x="{x}" y="{y+7}" text-anchor="middle" font-size="22">{n}</text>' for n,x,y in [(1,92,285),(2,325,508),(3,420,379),(4,380,155)])
                if family.startswith('sequential'):picture=picture.replace('M346 351V411M383 351V411M346 382H383','M365 351V411')
                if family!='sequential-four':picture=picture.replace('<g fill="var(--teal)">','<g fill="var(--teal)">').replace('x="175" y="150"','x="110" y="150"').replace('x="240" y="150"','x="110" y="150"').replace('x="305" y="150"','x="110" y="150"')
                if family=='rally':
                    labels.append(('HANDBRAKE','Grab and pull the lever; X also holds it.' if shift=='lever' else 'Hold X for the handbrake.'))
                    picture+='<path d="M390 545L430 457" stroke="url(#metal)" stroke-width="14"/><path d="M425 467L443 430" stroke="var(--rubber)" stroke-width="22"/><circle cx="446" cy="540" r="16" fill="#31c8bb"/><text x="446" y="548" text-anchor="middle" font-size="22">6</text>'
                choices.append(save_choice(sid,title,explanation,rows,lambda theme:family_card(title,explanation,picture,labels,theme)))
        for gid in ids:write_game(gid,choices)
    choices=[]
    for mode in ('grab-bars','thumbstick','body-lean','twist-throttle'):
        source={'grab-bars':'both_hands','twist-throttle':'both_hands','thumbstick':'left_thumbstick','body-lean':'head'}[mode]
        extractor={'grab-bars':'bar_yaw','twist-throttle':'bar_yaw','thumbstick':'stick','body-lean':'body_lean'}[mode]
        sid='cab-motorcycle-'+mode;title='Motorcycle / '+mode.replace('-',' ')
        explanation={'grab-bars':'Turn grabbed handlebars; triggers work the gas and front brake.','thumbstick':'Steer with a thumbstick while triggers work the gas and front brake.','body-lean':'Choose seated head lean to steer, with trigger gas and brakes.','twist-throttle':'Turn the bars and twist the right grip for throttle, like a motorcycle.'}[mode]
        rows=[base_element('bars','Steer/bank','handlebars',source,'pivot_steering',{'axis':'Steering','range':[0,255],'centre':128,'invert':False},model='bars-motorcycle',position=[0,.78,-.40],extractor=extractor,range_deg=60,calibration={'centre':'before_launch','range_m':[-.18,.18]},axes={'component':'x','positive':'right'},grab={'required':mode in ('grab-bars','twist-throttle'),'button':'grip','soft_lock':True},spring={'return_speed':6}),
              base_element('throttle','Throttle','lever','right_hand' if mode=='twist-throttle' else 'right_trigger','pivot_throttle',{'axis':'Accelerator','range':[0,255],'invert':False},model='bars-motorcycle',extractor='grip_twist' if mode=='twist-throttle' else 'trigger',range_deg=60,curve={'deadzone':.04,'gamma':1.4},axes={'positive':'twist_towards_rider' if mode=='twist-throttle' else 'squeeze'}),
              base_element('front-brake','Front brake','lever','right_trigger' if mode=='twist-throttle' else 'left_trigger','pivot_front_brake',{'axis':'Brake','range':[0,255],'invert':False},model='bars-motorcycle',extractor='trigger',curve={'deadzone':.04,'gamma':1.4}),
              base_element('rear-brake','Rear brake','pedal','left_trigger' if mode=='twist-throttle' else 'right_primary','pivot_pedal',{'axis':'RearBrake','range':[0,255],'invert':False},model='pedal-single',extractor='trigger' if mode=='twist-throttle' else 'button',position=[.12,.10,-.35],curve={'deadzone':.04,'gamma':1})]
        for eid,label,source,port in [('up','Shift up','right_thumbstick_up','GearShiftUp'),('down','Shift down','right_thumbstick_down','GearShiftDown'),('view','Change view','left_primary','ViewChange'),('music','Music select','left_secondary','MusicSelect'),('start','Start','right_thumbstick_click','Start1'),('coin','Insert credit','right_secondary','Coin1')]:rows.append(base_element(eid,label,'button',source,'',{'button':port},binding={'mode':'press'}))
        labels=[('STEERING',{'grab-bars':'Grip both bars; turn left / right.','twist-throttle':'Grip both bars; turn left / right.','thumbstick':'Left thumbstick left / right.','body-lean':'Seated lean left / right (optional).'}[mode]),('THROTTLE','Twist right grip towards you.' if mode=='twist-throttle' else 'Right trigger.'),('FRONT BRAKE','Right trigger.' if mode=='twist-throttle' else 'Left trigger.'),('REAR BRAKE','Left trigger.' if mode=='twist-throttle' else 'Hold A.'),('SHIFT / VIEW / MUSIC','Right stick up/down; X view; Y music.'),('START / CREDIT','Right stick click starts; B inserts credit.')]
        choices.append(save_choice(sid,title,explanation,rows,lambda theme:family_card(title,explanation,bike_picture(),labels,theme)))
    write_game('harley',choices)

def gun_picture(mounted):
    gun='''<path d="M85 225L270 205L330 231L309 276L233 280L227 382L180 390L168 287L99 283Z" fill="var(--plate)" stroke="url(#metal)" stroke-width="6"/>
    <path d="M172 286L242 283L248 331L186 341Z" fill="var(--rubber)"/><path d="M224 289L224 314" stroke="#31c8bb" stroke-width="10"/>
    <path d="M178 299L191 371L219 365L221 293" fill="var(--rubber)"/>
    <path d="M90 243L35 243" stroke="#31c8bb" stroke-width="3" stroke-dasharray="8 8"/>'''
    if mounted:gun='''<path d="M62 229L240 201L330 224L332 292L240 318L66 286Z" fill="var(--plate)" stroke="url(#metal)" stroke-width="6"/>
    <path d="M91 285L109 365L293 365L312 296" stroke="url(#metal)" stroke-width="23" fill="none"/>
    <path d="M193 365V444" stroke="url(#metal)" stroke-width="29"/><rect x="125" y="432" width="147" height="27" rx="8" fill="var(--rubber)"/>
    <path d="M329 241L368 241L366 344" stroke="var(--rubber)" stroke-width="26" fill="none"/>
    <path d="M99 245H206M103 267H210" stroke="var(--rubber)" stroke-width="10" stroke-dasharray="8 9"/>'''
    return '<g transform="translate(18,85)">'+gun+'''<circle cx="65" cy="220" r="17" fill="#31c8bb"/><circle cx="245" cy="314" r="17" fill="#31c8bb"/><circle cx="309" cy="303" r="17" fill="#31c8bb"/>
    <g font-size="22" text-anchor="middle"><text x="65" y="227">1</text><text x="245" y="321">2</text><text x="309" y="310">3</text></g></g>'''

def write_step3():
    for family,gids,model in [('pistol',['lostwsga'],'arc-pistol-twin'),('mounted',['lamachin','oceanhun'],'mnt-mg-heavy')]:
        choices=[]
        modes=('two-guns','two-hand','single-gun') if family=='pistol' else ('two-hand','single-gun','two-guns')
        for mode in modes:
            sid='cab-'+family+'-'+mode;title=family.title()+' / '+mode.replace('-',' ')
            explanation={'two-guns':'Right hand controls player 1; left hand controls player 2 independently.','two-hand':'Hold one gun with both hands for player 1, with the other player inactive.','single-gun':'Use one gun in the right hand; left hand is free.'}[mode]
            rows=[]
            for slot in range(2 if mode=='two-guns' else 1):
                hand='right' if slot==0 else 'left';suffix='' if slot==0 else '2'
                source='both_hands' if mode=='two-hand' else hand+'_hand'
                for axis,extractor,semantic in [('x','bar_yaw','yaw'),('y','bar_lift','pitch')]:
                    fields=dict(model=model,player=slot,slot=slot,position=[(-.24 if slot else .24) if mode=='two-guns' else 0,.75,-.40],grab={'required':family=='mounted' or mode=='two-hand','button':'grip','soft_lock':True},tracking_loss='freeze_then_neutral',axes={'component':axis,'positive':'right' if axis=='x' else 'up'})
                    if family=='pistol':fields.update(extractor='aim_ray',aim={'projection':'original_game_view','hit':'scene_depth_or_geometry','outside':'clamp_to_edge_no_Offscreen_port','anchor':'muzzle','pose':'aim_orientation_at_grip_position','firing_hand':hand},hold={'firing_hand':hand,'offhand':'support_firing_hand' if mode=='two-hand' else 'none','attach_node':'grip'})
                    else:fields.update(extractor=extractor if mode=='two-hand' else 'stick_tilt',calibration={'centre':'before_grab','range_deg':[-40,40] if axis=='x' else [-23,23]},range_m=[-.12,.12] if axis=='y' and mode=='two-hand' else [],mount={'anchor':'seat','yaw_limit_deg':40,'pitch_limit_deg':23,'one_controller_per_mount':mode=='two-guns'})
                    rows.append(base_element('p'+str(slot+1)+'-aim-'+axis,'Aim '+('horizontal' if axis=='x' else 'vertical'),'pistol' if family=='pistol' else 'mounted_gun',source,'muzzle' if family=='pistol' else 'pivot_'+semantic,{'axis':'AnalogGun'+axis.upper()+suffix,'range':[0,255],'centre':128,'invert':False},**fields))
                for eid,label,source,node,port in [('main-trigger','Main trigger',hand+'_trigger','pivot_trigger','AnalogTriggerLeft'+suffix),('second-trigger','Second trigger',('left_trigger' if mode=='two-hand' else hand+'_primary'),'button_special' if family=='mounted' else '', 'AnalogTriggerRight'+suffix),('start','Start',hand+'_thumbstick_click','','Start'+str(slot+1)),('coin','Insert credit',hand+'_secondary','','Coin'+str(slot+1))]:
                    rows.append(base_element('p'+str(slot+1)+'-'+eid,label,'button',source,node,{'button':port,'range':[0,1]},player=slot,slot=slot,model=model,binding={'mode':'hold' if 'trigger' in eid else 'press','threshold':.55},game_action_question='Confirm each trigger switch caption in the game instructions.' if 'trigger' in eid else ''))
            labels=[('AIM','Point your gun at the target.' if family=='pistol' else 'Grip the handles and aim within the mount.'),('MAIN TRIGGER','Firing-hand trigger for each active player.'),('SECOND TRIGGER','Other-hand trigger with two hands; A/X otherwise.'),('START / CREDIT','Each gun: stick click starts; B/Y inserts credit.'),('PLAYERS','Two independent gun slots.' if mode=='two-guns' else 'Player 1 only; player 2 stays inactive.')]
            choices.append(save_choice(sid,title,explanation,rows,lambda theme:family_card(title,explanation,gun_picture(family=='mounted'),labels,theme)))
        for gid in gids:write_game(gid,choices)

def ski_picture():
    return '''<g transform="translate(25,90)"><path d="M95 400L325 400L353 484H65Z" fill="var(--plate)" stroke="url(#metal)" stroke-width="5"/>
    <path d="M116 378H190L180 468H98ZM235 378H308L328 468H249Z" fill="var(--rubber)"/>
    <path d="M130 408H181M256 408H311" stroke="url(#metal)" stroke-width="15"/>
    <path d="M66 438V160L105 135M354 438V160L318 135" stroke="url(#metal)" stroke-width="15" fill="none"/>
    <path d="M80 147L122 121M334 147L293 121" stroke="var(--rubber)" stroke-width="25"/>
    <rect x="149" y="180" width="127" height="32" rx="10" fill="var(--plate)"/><g fill="#31c8bb"><circle cx="172" cy="194" r="10"/><circle cx="212" cy="194" r="10"/><circle cx="252" cy="194" r="10"/></g>
    <g fill="#31c8bb"><circle cx="205" cy="474" r="18"/><circle cx="338" cy="477" r="18"/><circle cx="110" cy="99" r="18"/><circle cx="294" cy="99" r="18"/></g>
    <g font-size="22" text-anchor="middle"><text x="205" y="481">1</text><text x="338" y="484">2</text><text x="110" y="106">3</text><text x="294" y="106">3</text></g></g>'''

def pump_picture():
    return '''<g transform="translate(12,90)"><path d="M63 437H357L402 518H22Z" fill="var(--plate)" stroke="url(#metal)" stroke-width="6"/>
    <path d="M110 431V349M310 431V349" stroke="url(#metal)" stroke-width="19"/>
    <path d="M110 351L134 243M310 351L286 243" stroke="url(#metal)" stroke-width="20"/>
    <path d="M94 242H170M250 242H326" stroke="var(--rubber)" stroke-width="25"/>
    <rect x="74" y="453" width="84" height="46" rx="8" fill="url(#metal)"/><rect x="260" y="453" width="84" height="46" rx="8" fill="url(#metal)"/>
    <path d="M80 465H152M80 483H152M266 465H338M266 483H338" stroke="var(--rubber)" stroke-width="8"/>
    <g fill="#31c8bb"><circle cx="315" cy="213" r="18"/><circle cx="103" cy="213" r="18"/><circle cx="193" cy="480" r="18"/></g>
    <g font-size="22" text-anchor="middle"><text x="315" y="220">1</text><text x="103" y="220">2</text><text x="193" y="487">3</text></g></g>'''

def write_step4():
    choices=[]
    for mode in ('thumbstick','body-controls'):
        sid='cab-ski-'+mode;title='Ski platform / '+mode.replace('-',' ')
        explanation='Steer and edge with your left thumbstick; triggers jab the poles.' if mode=='thumbstick' else 'Choose body lean and forward/back rock for the ski platform; triggers jab the poles.'
        rows=[]
        for axis,node,port in [('x','pivot_swing','SkiX'),('y','pivot_edge','SkiY')]:
            rows.append(base_element('ski-'+axis,'Swing left/right' if axis=='x' else 'Edge forward/back','lean_body' if mode=='body-controls' else 'stick','head' if mode=='body-controls' else 'left_thumbstick',node,{'axis':port,'range':[0,255],'centre':128,'invert':axis=='x' or mode=='thumbstick'},model='platform-ski',position=[0,0,-.20],extractor=('body_lean' if axis=='x' else 'body_rock') if mode=='body-controls' else 'stick',axes={'component':axis if mode=='thumbstick' else ('x' if axis=='x' else 'z'),'positive':'right' if axis=='x' else ('back' if mode=='body-controls' else 'forward')},calibration={'range_m':[-.18,.18],'centre':'before_launch'},curve={'deadzone':.06,'gamma':1},tracking_loss='neutral'))
        for i,hand in enumerate(('left','right'),1):rows.append(base_element('pole-'+hand,'Jab '+hand+' pole','button',hand+'_trigger','button_pole_'+str(i),{'button':'SkiPoll'+hand.title(),'range':[0,1]},model='platform-ski',binding={'threshold':.55,'mode':'hold'}))
        for i,source in enumerate(('right_primary','right_secondary','left_primary'),1):rows.append(base_element('select-'+str(i),'Select '+str(i),'button',source,'button_select_'+str(i),{'button':'SkiSelect'+str(i),'range':[0,1]},model='panel-select-three',binding={'mode':'press'}))
        for eid,label,source,port in [('start','Start','right_thumbstick_click','Start1'),('coin','Insert credit','left_secondary','Coin1')]:rows.append(base_element(eid,label,'button',source,'',{'button':port,'range':[0,1]},binding={'mode':'press'}))
        labels=[('SWING','Left stick left/right.' if mode=='thumbstick' else 'Lean left/right from calibrated centre.'),('EDGE','Left stick forward/back.' if mode=='thumbstick' else 'Rock forward/back from centre.'),('POLES','Left/right triggers jab matching poles.'),('SELECT 1 / 2 / 3','A / B / X, respectively.'),('START / CREDIT','Right stick click starts; Y inserts credit.')]
        choices.append(save_choice(sid,title,explanation,rows,lambda theme:family_card(title,explanation,ski_picture(),labels,theme)))
    write_game('skichamp',choices)
    choices=[]
    for mode in ('grab-pump','thumbsticks'):
        sid='cab-handcar-'+mode;title='Handcar / '+mode.replace('-',' ')
        explanation='Grab and pump the two handles; each controller belongs to one player.' if mode=='grab-pump' else 'Use each thumbstick up/down as one player’s pump; triggers operate the pedals.'
        rows=[]
        for slot,hand in enumerate(('right','left')):
            i=slot+1
            rows.append(base_element('pump-'+str(i),'Player '+str(i)+' pump','pump',hand+'_hand' if mode=='grab-pump' else hand+'_thumbstick','pivot_pump_'+str(i),{'axis':'MagicalLever'+str(i),'range':[0,255],'centre':128,'invert':False},model='pump-handcar',player=slot,extractor='bar_lift' if mode=='grab-pump' else 'stick',axes={'component':'y','positive':'up'},range_m=[-.16,.16],grab={'required':mode=='grab-pump','button':'grip','radius_m':.06,'soft_lock':True},calibration={'centre':'before_grab'},tracking_loss='neutral'))
            rows.append(base_element('pedal-'+str(i),'Player '+str(i)+' pedal','pedal',hand+'_trigger','button_pedal_'+str(i),{'button':'MagicalPedal'+str(i),'range':[0,1]},model='pump-handcar',player=slot,extractor='trigger',binding={'threshold':.55,'mode':'hold'}))
            for eid,label,source,port in [('start','Start',hand+'_primary','Start'+str(i)),('coin','Insert credit',hand+'_secondary','Coin'+str(i))]:rows.append(base_element('p'+str(i)+'-'+eid,label,'button',source,'',{'button':port,'range':[0,1]},player=slot,binding={'mode':'press'}))
        labels=[('PLAYER 1 PUMP','Grip right handle; move up/down.' if mode=='grab-pump' else 'Right thumbstick up/down.'),('PLAYER 2 PUMP','Grip left handle; move up/down.' if mode=='grab-pump' else 'Left thumbstick up/down.'),('PEDALS','Right/left trigger = P1/P2 pedal.'),('START / CREDIT','A/B = P1 start/credit; X/Y = P2.'),('HANDOFF','Two independent players; no invented wheel.')]
        choices.append(save_choice(sid,title,explanation,rows,lambda theme:family_card(title,explanation,pump_picture(),labels,theme)))
    write_game('magtruck',choices)

def flight_picture():
    return '''<g transform="translate(25,85)">
    <rect x="96" y="395" width="256" height="102" rx="24" fill="var(--plate)" stroke="url(#metal)" stroke-width="6"/>
    <ellipse cx="224" cy="402" rx="66" ry="22" fill="var(--rubber)"/>
    <path d="M224 405L224 307" stroke="url(#metal)" stroke-width="22"/>
    <path d="M179 311L197 189L251 181L278 212L263 325Z" fill="var(--rubber)" stroke="url(#metal)" stroke-width="4"/>
    <path d="M168 326H277" stroke="var(--plate)" stroke-width="20"/>
    <path d="M196 258L169 281L173 311" stroke="#31c8bb" stroke-width="13" fill="none"/>
    <ellipse cx="252" cy="194" rx="15" ry="10" fill="#31c8bb"/>
    <g fill="#31c8bb"><circle cx="286" cy="273" r="18"/><circle cx="151" cy="287" r="18"/><circle cx="289" cy="185" r="18"/></g>
    <g font-size="22" text-anchor="middle"><text x="286" y="280">1</text><text x="151" y="294">2</text><text x="289" y="192">3</text></g></g>'''

def arcade_picture(count):
    caps=''
    for i in range(count):
        x=188+i*52
        caps+=f'<circle cx="{x}" cy="322" r="22" fill="var(--rubber)"/><circle cx="{x}" cy="317" r="17" fill="#31c8bb"/><text x="{x}" y="{324}" font-size="20" text-anchor="middle">{i+2}</text>'
    return '''<g transform="translate(12,100)"><rect x="44" y="292" width="381" height="162" rx="25" fill="var(--plate)" stroke="url(#metal)" stroke-width="6"/>
    <ellipse cx="113" cy="351" rx="43" ry="13" fill="var(--rubber)"/><path d="M113 349V267" stroke="url(#metal)" stroke-width="15"/>
    <circle cx="113" cy="258" r="29" fill="#31c8bb"/><text x="113" y="266" font-size="23" text-anchor="middle">1</text>'''+caps+'''<path d="M191 374H394" stroke="url(#metal)" stroke-width="4"/>
    <text x="195" y="409" font-size="19">Separate Start: stick click</text></g>'''

def write_added_games():
    choices=[]
    for mode in ('grab-stick','thumbstick'):
        sid='cab-flight-'+mode;title='Flight stick / '+mode.replace('-',' ')
        explanation='Grip the flight stick and tilt it; the right trigger fires and A presses Event.' if mode=='grab-stick' else 'Use the left thumbstick to aim; the right trigger fires and A presses Event.'
        rows=[]
        for axis in ('x','y'):
            rows.append(base_element('stick-'+axis,'Aim '+('left/right' if axis=='x' else 'up/down'),'stick',
                'right_hand' if mode=='grab-stick' else 'left_thumbstick','pivot_stick_'+axis,
                {'axis':'AnalogJoy'+axis.upper(),'range':[0,255],'centre':128,'invert':axis=='y' and mode=='thumbstick'},
                model='stick-flight',position=[.15,.58,-.35],extractor='stick_tilt' if mode=='grab-stick' else 'stick',
                axes={'component':axis if mode=='thumbstick' else ('roll' if axis=='x' else 'pitch'),'positive':'right' if axis=='x' else ('up' if mode=='thumbstick' else 'back/down')},
                calibration={'centre':'before_grab' if mode=='grab-stick' else 'before_launch','range_deg':[-20,20]},
                curve={'deadzone':.05,'gamma':1},spring={'return_speed':6},tracking_loss='neutral',
                grab={'required':mode=='grab-stick','button':'right_grip','radius_m':.06,'soft_lock':True}))
        rows.extend([
            base_element('fire','Fire','button','right_trigger','pivot_trigger',{'button':'AnalogJoyTrigger','range':[0,1]},model='stick-flight',binding={'threshold':.55,'mode':'hold'}),
            base_element('event','Event','button','right_primary','button_event',{'button':'AnalogJoyEvent','range':[0,1]},model='stick-flight',binding={'mode':'hold'}),
            base_element('start','Start','button','right_thumbstick_click','',{'button':'Start1','range':[0,1]},binding={'mode':'press'}),
            base_element('coin','Insert credit','button','right_secondary','',{'button':'Coin1','range':[0,1]},binding={'mode':'press'})])
        labels=[('AIM','Grip and tilt right hand +/-20 degrees.' if mode=='grab-stick' else 'Left thumbstick: left/right and up/down.'),('TRIGGER / FIRE','Right trigger = AnalogJoyTrigger.'),('EVENT BUTTON','A = AnalogJoyEvent.'),('START / CREDIT','Right stick click starts; B inserts credit.')]
        choices.append(save_choice(sid,title,explanation,rows,lambda theme:family_card(title,explanation,flight_picture(),labels,theme)))
    write_game('swtrilgy',choices)
    for gid,actions in [('vf3',[('Punch','right_trigger'),('Kick','right_primary'),('Guard','left_trigger'),('Escape','right_secondary')]),
                        ('fvipers2',[('Punch','right_trigger'),('Kick','right_primary'),('Guard','left_trigger')]),
                        ('spikeout',[('Beat','right_trigger'),('Charge','right_primary'),('Jump','right_secondary'),('Shift','left_trigger')]),
                        ('vs2',[('ShortPass','right_trigger'),('LongPass','right_primary'),('Shoot','right_secondary')])]:
        choices=[]
        game=tomllib.loads((ROOT/'games'/gid/'game.toml').read_text(encoding='utf-8'))
        for mode in ('thumbstick','grab-stick'):
            sid='cab-arcade-'+gid+'-'+mode;title=game['title']+' / '+mode.replace('-',' ')
            explanation='Move with the left thumbstick; use separate buttons for every action.' if mode=='thumbstick' else 'Grip and tilt the arcade stick with your left hand; use separate action buttons.'
            rows=[]
            for direction,axis,sign in [('Up','y',1),('Down','y',-1),('Left','x',-1),('Right','x',1)]:
                rows.append(base_element('joy-'+direction.lower(),'Move '+direction.lower(),'stick','left_thumbstick' if mode=='thumbstick' else 'left_hand','pivot_stick_'+axis,
                    {'button':'Joy'+direction,'range':[0,1]},model='stick-arcade',position=[-.18,.63,-.36],extractor='stick' if mode=='thumbstick' else 'stick_tilt',
                    axes={'component':axis if mode=='thumbstick' else ('roll' if axis=='x' else 'pitch'),'positive':'right' if axis=='x' else 'forward/up'},
                    binding={'direction':direction.lower(),'threshold':.55,'release_threshold':.35,'mode':'hold'},
                    calibration={'centre':'before_launch','range_deg':[-15,15]},curve={'deadzone':.08,'gamma':1},
                    direction={'sign':sign,'opposites':'neutral','diagonals':'allow'},
                    grab={'required':mode=='grab-stick','button':'left_grip','radius_m':.06,'soft_lock':True,'anchors':['grab_knob']},tracking_loss='neutral'))
            panel='panel-action-four' if len(actions)==4 else 'panel-action-three'
            button_names={'right_trigger':'Right trigger','left_trigger':'Left trigger','right_primary':'A','right_secondary':'B'}
            labels=[('MOVE','Left thumbstick in eight directions.' if mode=='thumbstick' else 'Grip the left stick; tilt in eight directions.')]
            for i,(port,source) in enumerate(actions,1):
                rows.append(base_element('action-'+str(i),port,'button',source,'button_action_'+str(i),{'button':port,'range':[0,1]},
                    model=panel,position=[.09,.63,-.36],binding={'mode':'hold','threshold':.55}))
                labels.append((port.upper(),button_names[source]+'.'))
            rows.extend([base_element('start','Start / first-person view' if gid=='vf3' else 'Start','button','right_thumbstick_click','',{'button':'Start1','range':[0,1]},binding={'mode':'press'}),
                         base_element('coin','Insert credit','button','left_secondary','',{'button':'Coin1','range':[0,1]},binding={'mode':'press'})])
            labels.append(('START / CREDIT','Right stick click = Start; Y = credit. Start during a fight switches to first-person view; it never shares an attack button.' if gid=='vf3' else 'Right stick click starts; Y inserts credit.'))
            choices.append(save_choice(sid,title,explanation,rows,lambda theme:family_card(title,explanation,arcade_picture(len(actions)),labels,theme)))
        write_game(gid,choices)

if __name__=='__main__':
    parser=argparse.ArgumentParser();parser.add_argument('--step',type=int,choices=[1,2,3,4,5],default=1);args=parser.parse_args()
    if args.step==1:write_racers()
    elif args.step==2:write_step2()
    elif args.step==3:write_step3()
    elif args.step==4:write_step4()
    else:write_added_games()
