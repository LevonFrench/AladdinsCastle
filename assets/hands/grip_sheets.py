"""Compose the bounded CPU grip-review images; no renderer or runtime."""
import hashlib
import json
import math
from pathlib import Path
import textwrap
ROOT=Path(__file__).resolve().parents[2]

def main():
    from PIL import Image,ImageDraw,ImageFont
    directory=ROOT/'assets/hands/preview/grips';data=json.loads((directory/'coverage.json').read_text());cases=data['cases'];font=ImageFont.load_default(size=14)
    sheet=Image.new('RGB',(1024,math.ceil(len(cases)/4)*320),'#101216');draw=ImageDraw.Draw(sheet)
    for i,case in enumerate(cases):
        x,y=(i%4)*256,(i//4)*320
        for j,line in enumerate(textwrap.wrap(case['id'],30)):draw.text((x+8,y+5+j*17),line,font=font,fill='#c5cbd4')
        source=Image.open(directory/(case['id']+'-grip.png')).convert('RGB');source.thumbnail((256,256));sheet.paste(source,(x,y+64))
    target=directory/'grip-coverage-sheet.png';sheet.save(target)
    for mid in sorted({case['model'] for case in cases}):
        selected=[c for c in cases if c['model']==mid];family=Image.new('RGB',(1024,len(selected)*552),'#101216');text=ImageDraw.Draw(family)
        for row,case in enumerate(selected):
            text.text((16,row*552+12),case['id']+' · assembly / grip crop',font=font,fill='#c5cbd4')
            for col,view in enumerate(('assembly','grip')):family.paste(Image.open(directory/(case['id']+'-'+view+'.png')).convert('RGB'),(col*512,row*552+40))
        family.save(directory/(mid+'-review-sheet.png'))
    data['source_sha256']={item['image']:hashlib.sha256((directory/item['image']).read_bytes()).hexdigest() for item in data['images']}
    (directory/'coverage.json').write_text(json.dumps(data,indent=2)+'\n');print('CPU grip overview and model family sheets saved')
if __name__=='__main__':main()
