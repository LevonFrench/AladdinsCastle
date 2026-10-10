"""Assemble the seven CPU previews with image pixels; no renderer. GPL-3.0-only."""
import argparse
from array import array
from pathlib import Path
import sys


def main():
    import bpy
    from common import TIERS
    parser = argparse.ArgumentParser()
    parser.add_argument('--preview-dir', type=Path, required=True)
    args = parser.parse_args(sys.argv[sys.argv.index('--')+1:])
    width, height = 512*4, 512*2
    pixels = array('f', [0.06,0.06,0.07,1]) * (width*height)
    for index, mid in enumerate(TIERS):
        source = bpy.data.images.load(str((args.preview_dir/(mid+'.png')).resolve()),check_existing=False)
        tile = array('f',[0])*(512*512*4)
        source.pixels.foreach_get(tile)
        x,y = (index%4)*512,(1-index//4)*512
        for row in range(512):
            start = ((y+row)*width+x)*4
            pixels[start:start+512*4] = tile[row*512*4:(row+1)*512*4]
    image = bpy.data.images.new('tier1_sheet',width=width,height=height)
    image.pixels.foreach_set(pixels)
    image.filepath_raw = str((args.preview_dir/'sheet.png').resolve())
    image.file_format = 'PNG'
    image.save()
    print('RESULT tier1 CPU preview sheet saved')


if __name__ == '__main__':
    main()
