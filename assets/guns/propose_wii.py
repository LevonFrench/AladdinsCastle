"""Original proposal sheet from authored metadata and existing CPU gun previews."""
import argparse
import hashlib
import json
from pathlib import Path
import textwrap
import tomllib
ROOT=Path(__file__).resolve().parents[2]
def proposals():
    rows=[]
    for path in sorted((ROOT/'data/controls/wii/games').glob('*.toml')):
        data=tomllib.loads(path.read_text(encoding='utf-8'));proposal=data.get('gun_proposal')
        if not proposal:continue
        game=tomllib.loads((ROOT/'games'/data['game_id']/'game.toml').read_text(encoding='utf-8'))
        image=ROOT/'assets/guns/preview'/f'{proposal["model"]}.png'
        rows.append(dict(game_id=data['game_id'],title=game['title'],state=proposal['state'],model=proposal['model'],rationale=proposal['rationale'],default_hold=data['hold']['default'],hold_state=data['hold']['state'],preview=image.relative_to(ROOT).as_posix(),preview_sha256=hashlib.sha256(image.read_bytes()).hexdigest()))
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
    heading=ImageFont.load_default(size=24);body=ImageFont.load_default(size=18)
    for i,row in enumerate(rows):
        x,y=(i%3)*512,(i//3)*640
        draw.text((x+18,y+14),'PROPOSED · '+row['default_hold'].upper(),font=body,fill='#f29d38')
        for j,line in enumerate(textwrap.wrap(row['title'],34)):draw.text((x+18,y+44+j*27),line,font=heading,fill='#eef0f3')
        image=Image.open(ROOT/row['preview']).convert('RGB');image.thumbnail((380,380))
        sheet.paste(image,(x+66,y+128))
        draw.text((x+18,y+514),row['model'],font=body,fill='#a5c3df')
        for j,line in enumerate(textwrap.wrap(row['rationale'],52)):draw.text((x+18,y+542+j*21),line,font=body,fill='#c5cbd4')
    draw.text((1042,1392),'OWNER CHOICES PENDING',font=heading,fill='#f29d38')
    draw.multiline_text((1042,1445),'Existing original gun designs.\nProvisional holds per game.\nShell/stick stays the faithful\ncontrols-page illustration.\n\nExact game weapon, physical fit\nand headset acceptance remain\nopen for the next sitting.',font=body,spacing=9,fill='#c5cbd4')
    sheet.save(directory/'wii-proposal-sheet.png');manifest.write_text(payload,encoding='utf-8')
    print('Saved seven-game gun/hold proposal sheet; no new model render')
if __name__=='__main__':main()
