"""Original proposal sheet from authored metadata and existing CPU gun previews."""
import argparse
import hashlib
import json
from pathlib import Path
import sys
import textwrap
import tomllib
ROOT=Path(__file__).resolve().parents[2]
sys.path.insert(0,str(ROOT/'assets/guns/src'))
from common import GAME_MODELS
def proposals():
    rows=[]
    assembly=json.loads((ROOT/'assets/hands/assembly.json').read_text())
    for path in sorted((ROOT/'data/controls/wii/games').glob('*.toml')):
        data=tomllib.loads(path.read_text(encoding='utf-8'));proposal=data.get('gun_proposal')
        if not proposal:continue
        game=tomllib.loads((ROOT/'games'/data['game_id']/'game.toml').read_text(encoding='utf-8'))
        prop_hold='support' if data['hold']['default']=='docked' and 'support_grip' in assembly['models'][proposal['model']] else 'primary'
        case=proposal['model']+'-right-'+prop_hold
        image=ROOT/'assets/hands/preview/grips'/f'{case}-assembly.png'
        rows.append(dict(game_id=data['game_id'],title=game['title'],state=proposal['state'],model=proposal['model'],rationale=proposal['rationale'],default_hold=data['hold']['default'],hold_state=data['hold']['state'],prop_hold=prop_hold,preview_case=case,reload=dict(data['reload']),preview=image.relative_to(ROOT).as_posix(),preview_sha256=hashlib.sha256(image.read_bytes()).hexdigest()))
    if len(rows)!=7:raise ValueError('Seven design-priority Wii proposals required')
    return rows
def main():
    parser=argparse.ArgumentParser();parser.add_argument('--write',action='store_true');args=parser.parse_args()
    rows=proposals();directory=ROOT/'assets/guns/preview';manifest=directory/'wii-proposal-sheet.json'
    payload=json.dumps(dict(version='gun-proposal/1',final_choices_require_owner=True,size=[1536,1920],games=rows),indent=2,ensure_ascii=False)+'\n'
    if not args.write:
        if manifest.read_text(encoding='utf-8')!=payload:raise ValueError('Proposal sheet metadata is stale')
        print('Seven provisional gun/hold proposals; owner confirmation pending');return
    from PIL import Image,ImageDraw,ImageFont
    sheet=Image.new('RGB',(1536,1920),'#101216');draw=ImageDraw.Draw(sheet)
    heading=ImageFont.load_default(size=24);body=ImageFont.load_default(size=18);small=ImageFont.load_default(size=14)
    coverage=json.loads((ROOT/'assets/hands/preview/grips/coverage.json').read_text())
    records={r['case']:r for r in coverage['images'] if r['view']=='assembly'}
    for i,row in enumerate(rows):
        x,y=(i%3)*512,(i//3)*640
        draw.text((x+18,y+14),'PROPOSED · '+('TWO HANDS' if row['prop_hold']=='support' else 'FIRING HAND'),font=body,fill='#f29d38')
        for j,line in enumerate(textwrap.wrap(row['title'],34)):draw.text((x+18,y+44+j*27),line,font=heading,fill='#eef0f3')
        image=Image.open(ROOT/row['preview']).convert('RGB');image.thumbnail((380,380))
        sheet.paste(image,(x+66,y+128))
        draw.text((x+18,y+514),row['model'],font=body,fill='#a5c3df')
        record=records[row['preview_case']]
        for node,label,position in [('muzzle','Aim / controller ray',(x+18,y+122)),('pivot_trigger','Trigger / B / game action?',(x+18,y+486))]:
            px,py=record['label_points'][node];target=(x+66+px*380/512,y+128+py*380/512)
            draw.line((position[0]+105,position[1]+8,*target),fill='#a5c3df',width=2)
            draw.ellipse((target[0]-3,target[1]-3,target[0]+3,target[1]+3),fill='#a5c3df')
            draw.text(position,label,font=small,fill='#eef0f3',stroke_width=2,stroke_fill='#101216')
        reload_label=row['reload']['selected']
        if reload_label!='button':reload_label+=' + button fallback'
        draw.text((x+18,y+542),'Reload: '+reload_label,font=body,fill='#c5cbd4')
        draw.text((x+18,y+567),'Reload disabled; game instruction pending',font=small,fill='#f29d38')
        for j,line in enumerate(textwrap.wrap(row['rationale'],62)[:2]):draw.text((x+18,y+591+j*18),line,font=small,fill='#c5cbd4')
    draw.text((530,1294),'SIGHT CHECKS · LOD0 / LOD1',font=body,fill='#a5c3df')
    for i,mid in enumerate(GAME_MODELS):
        sy=1330+i*110
        for j,line in enumerate(textwrap.wrap(mid,22)):draw.text((530,sy+30+j*18),line,font=small,fill='#c5cbd4')
        for lod in (0,1):
            sight=Image.open(directory/(mid+'-sight-lod'+str(lod)+'.png')).convert('RGB');sight.thumbnail((100,100));sheet.paste(sight,(750+lod*110,sy))
    draw.text((1042,1392),'OWNER CHOICES PENDING',font=heading,fill='#f29d38')
    draw.multiline_text((1042,1445),'Five original weapon props + hands.\nNeutral finish; player colour\non small accents only.\nCPU runtime diffuse lighting.\nMounted gun retains its older finish.\n\nBoth hand mounts and moving\nlabel anchors are in assembly.json.\nCrossbow/mounted support holds\nhave separate grip review images.\n\nShell/stick remains the faithful\ncontrols-page illustration.\nGun choice, fit, game actions and\nheadset appearance await review.',font=body,spacing=9,fill='#c5cbd4')
    sheet.save(directory/'wii-proposal-sheet.png');manifest.write_text(payload,encoding='utf-8')
    print('Saved seven-game weapon/hand/reload/label review sheet from CPU images')
if __name__=='__main__':main()
