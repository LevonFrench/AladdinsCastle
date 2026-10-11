"""Index existing gun/hand previews and authored questions; never render or launch."""
import argparse
import hashlib
import json
import os
import runpy
from pathlib import Path
import tomllib
from wii_controls import compile_library

ROOT=Path(__file__).resolve().parents[1]
OUTPUT='assets/hands/preview/confirmation-index'
LIMITS=[
    'Gun choices and holds are proposals; owner confirmation is pending.',
    'Existing CPU previews use the runtime lighting recipe, not an actual headset frame.',
    'Rigid mounting and visible proportions can be reviewed; comfort, calibration, pressure and fine intersections remain unaccepted.',
    'Game layouts, reload classifiers and confirm/back meanings require the game option/instruction screens.',
    'Canonical DSU transport source tests do not establish game input or tracked recentre acceptance.',
]

def artifact(root,name,kind):
    path=(Path(root)/name).resolve()
    if Path(name).is_absolute() or not path.is_relative_to(Path(root).resolve()):raise ValueError('Artifact escapes project')
    if not path.is_file():raise ValueError('Missing existing artifact: '+name)
    data=path.read_bytes();text=kind in ('source','metadata')
    # Match the repository's enforced LF text bytes on both Windows and Linux.
    if text:data=data.replace(b'\r\n',b'\n')
    return dict(kind=kind,hash_basis='repository-lf' if text else 'file-bytes',sha256=hashlib.sha256(data).hexdigest())

def compile_index(root=ROOT):
    root=Path(root);artifacts={}
    def add(name,kind):
        artifacts[name]=artifact(root,name,kind);return name
    def load(name):return json.loads((root/name).read_text(encoding='utf-8'))
    shared=[add(name,kind) for name,kind in [
        ('data/controls/wii/shell-and-stick.toml','source'),('assets/hands/assembly.json','metadata'),
        ('tools/hand_assembly.py','source'),('tools/gun_confirmation.py','source'),
        ('tools/wii_controls.py','source'),('tools/wii_shell_stick.py','source'),
        ('assets/guns/src/common.py','source'),('assets/hands/src/grip_hand.py','source'),
        ('assets/guns/runtime-lighting.json','source'),('assets/hands/review_grips.py','source'),
        ('assets/hands/grip_sheets.py','source'),('assets/guns/propose_wii.py','source'),
        ('assets/guns/preview/wii-proposal-sheet.json','metadata'),('assets/guns/preview/wii-proposal-sheet.png','preview'),
        ('assets/guns/preview/shell-stick-sheet.png','preview'),
        ('assets/controls/wii/umbrella-shell-stick-controls.png','software-controls-preview'),
        ('assets/hands/preview/grips/coverage.json','metadata'),('assets/hands/preview/grips/grip-coverage-sheet.png','preview'),
    ]]
    for hand in ('left','right'):
        shared.extend([add(f'assets/hands/grip-hand-{hand}.glb','mesh'),add(f'assets/hands/grip-hand-{hand}.toml','metadata')])
    assembly=load('assets/hands/assembly.json');coverage=load('assets/hands/preview/grips/coverage.json')
    if coverage['cases']!=runpy.run_path(str(root/'assets/hands/review_grips.py'))['cases'](root):
        raise ValueError('Reviewed grip poses disagree with current source')
    if coverage['lighting']!=load('assets/guns/runtime-lighting.json') or coverage['device']!='CPU':
        raise ValueError('Reviewed lighting/device evidence disagrees with the CPU recipe')
    for name,digest in coverage['source_sha256'].items():
        full=add('assets/hands/preview/grips/'+name,'preview')
        if artifacts[full]['sha256']!=digest:raise ValueError('Stale reviewed image hash: '+full)
    proposal_sheet=load('assets/guns/preview/wii-proposal-sheet.json')
    proposals={p['game_id']:p for p in proposal_sheet['games']}
    games=[];models={}
    for row in compile_library(root)['games']:
        gid=row['game_id'];data=row['data'];proposal=data['gun_proposal'];mid=proposal['model']
        if proposal['state']!='proposed' or data['assembly']['default_hold']!=proposals[gid]['default_hold'] or mid!=proposals[gid]['model']:
            raise ValueError('Proposal sheet disagrees with authored question: '+gid)
        policy_path=f'data/controls/wii/games/{gid}.toml';policy=tomllib.loads((root/policy_path).read_text(encoding='utf-8'))
        sources=[add(policy_path,'source'),add(f'games/{gid}/game.toml','source')]
        games.append(dict(game_id=gid,title=data['title'].removesuffix(' — shell and stick'),proposal=dict(proposal),
            hold=dict(choice=data['assembly']['default_hold'],state=policy['hold']['state'],question='Confirm the hold, firing hand and whether the stick controller stays split or docks.'),
            reload=data['reload'],layout=data['layout'],questions=data['questions'],sources=sources,
            parts=[dict(id=p['id'],part=p['part'],label=p['label'],state=p['game_action_state'],role=p['role'],model=p['model'],node=p['node']) for p in data['element'] if p['slot']==0],
            review_models=[mid,'con-remote-shell','con-stick-grip']))
        for model in (mid,'con-remote-shell','con-stick-grip'):
            if model in models:continue
            meta_path=f'data/guns/{model}.toml';meta=tomllib.loads((root/meta_path).read_text(encoding='utf-8'))
            preview=f'assets/guns/preview/{model}'
            refs=[add(meta_path,'source'),add(meta['source'],'source'),add(meta['model'],'mesh'),add(preview+'.json','metadata')]
            refs.extend(add(preview+suffix+'.png','preview') for suffix in ('','-front','-held','-silhouette'))
            nodes=load(preview+'.json')['views']['threequarter']['nodes']
            sights=[add(preview+f'-sight-lod{lod}.png','preview') for lod in (0,1)] if 'muzzle' in nodes else []
            sheet=add(f'assets/hands/preview/grips/{model}-review-sheet.png','preview')
            cases=[]
            for case in coverage['cases']:
                if case['model']!=model:continue
                images=[add('assets/hands/preview/grips/'+case['id']+'-'+view+'.png','preview') for view in ('assembly','grip')]
                cases.append(dict(id=case['id'],primary_hand=case['primary_hand'],hold=case['hold'],images=images))
            models[model]=dict(label=meta['name'],state='provisional',sources=refs,sight_checks=sights,
                review_sheet=sheet,review_cases=cases,mounts=assembly['models'][model]['primary_mount_by_hand'],
                support=assembly['models'][model].get('support_by_primary',{}))
    if {g['game_id'] for g in games}!=set(proposals):raise ValueError('Proposal sheet game coverage mismatch')
    return dict(schema='gun-confirmation/1',state='owner-confirmation-pending',limits=LIMITS,
        evidence=dict(source='authored policy and SHA256-bound existing artifacts',visual='CPU grip-family sheets; rigid mounting and lighting proxy only',runtime='no game/headset acceptance recorded'),
        shared_artifacts=shared,models=models,games=games,artifacts=dict(sorted(artifacts.items())))

def markdown(index):
    def link(label,path):return '['+label+']('+os.path.relpath(path,'assets/hands/preview').replace('\\','/')+')'
    lines=['# Gun and hold confirmation index','','Owner confirmation pending. All model and hold choices remain proposals.','',
        link('Proposal overview','assets/guns/preview/wii-proposal-sheet.png')+' · '+link('Grip overview','assets/hands/preview/grips/grip-coverage-sheet.png')+' · '+link('Shell and stick','assets/guns/preview/shell-stick-sheet.png'),'',
        link('Umbrella software controls example','assets/controls/wii/umbrella-shell-stick-controls.png')+' (native route; game actions remain questions).','',
        'Full SHA256 values and all source/image references: '+link('machine-readable index',OUTPUT+'.json')+'.',
        'Source/metadata hashes use the repository LF bytes enforced by `.gitattributes`; meshes and images use their exact file bytes.','']
    lines.extend('- '+limit for limit in index['limits']);lines.append('')
    lines.extend(['| Game | Proposed model | Proposed hold | Reload question | Existing grip sheet |','| --- | --- | --- | --- | --- |'])
    for game in index['games']:
        mid=game['proposal']['model'];model=index['models'][mid]
        lines.append(f'| {game["title"]} | {model["label"]} (`{mid}`) | {game["hold"]["choice"]} | {game["reload"]["selected"]}; unconfirmed | '+link('Review',model['review_sheet'])+' |')
    for game in index['games']:
        lines.extend(['','## '+game['title'],'',game['proposal']['rationale'],'',
            '- [ ] Confirm the proposed gun or choose a different original model.',
            '- [ ] '+game['hold']['question'],
            '- [ ] Reload: '+game['reload']['question'],
            '- [ ] Layout: '+game['layout']['question']])
        lines.extend('- [ ] '+q for q in dict.fromkeys(game['questions']) if q!=game['layout']['question'])
        lines.extend(['','| Part | Game action evidence |','| --- | --- |'])
        lines.extend('| '+p['part']+' | '+p['label']+' |' for p in game['parts'])
        lines.extend(['','Source policy: '+link(game['game_id'],game['sources'][0])+'.'])
    lines.extend(['','## Artifact hashes','','Hashes describe these existing bytes. They do not establish owner acceptance.','',
        '| Artifact | SHA256 |','| --- | --- |'])
    lines.extend('| '+link(path,path)+' | `'+record['sha256']+'` |' for path,record in index['artifacts'].items())
    return '\n'.join(lines)+'\n'

def main():
    parser=argparse.ArgumentParser(description=__doc__);parser.add_argument('--write',action='store_true');args=parser.parse_args()
    index=compile_index();outputs={OUTPUT+'.json':json.dumps(index,indent=2,ensure_ascii=False)+'\n',OUTPUT+'.md':markdown(index)}
    for name,text in outputs.items():
        path=ROOT/name
        if args.write:path.write_text(text,encoding='utf-8')
        elif not path.is_file() or path.read_text(encoding='utf-8')!=text:raise ValueError('Stale confirmation index: '+name)
    print(f'{len(index["games"])} proposed game rows; {len(index["artifacts"])} existing artifact hashes; no renders')

if __name__=='__main__':main()
