"""Assemble original CPU renders and hash their inputs; no render or graphics API."""
import hashlib
import json
from pathlib import Path
ROOT=Path(__file__).resolve().parents[2]

def main():
    from PIL import Image,ImageDraw,ImageFont
    directory=ROOT/'assets/hands/preview';sheet=Image.new('RGB',(2048,1104),'#101216');draw=ImageDraw.Draw(sheet);font=ImageFont.load_default(size=18);records=[]
    for row,(model,hand) in enumerate([('arc-pistol-slide','right'),('con-stick-grip','left')]):
        for col,(state,view) in enumerate([('rest','side'),('rest','threequarter'),('stress','side'),('stress','threequarter')]):
            name=f'{model}-{hand}-{state}-{view}.png';path=directory/name
            draw.text((col*512+16,row*552+12),f'{hand.upper()} · {state} · {view}',font=font,fill='#c5cbd4')
            sheet.paste(Image.open(path).convert('RGB'),(col*512,row*552+40));records.append(dict(image=name,sha256=hashlib.sha256(path.read_bytes()).hexdigest()))
    path=directory/'hand-assembly-sheet.png';sheet.save(path)
    (directory/'hand-assembly-sheet.json').write_text(json.dumps(dict(schema='hand-review/1',sheet=path.name,size=list(sheet.size),sources=records,sha256=hashlib.sha256(path.read_bytes()).hexdigest(),acceptance='CPU fit review; owner physical acceptance pending'),indent=2)+'\n')
    print('Original CPU assembly sheet saved; source hashes recorded')
if __name__=='__main__':main()
