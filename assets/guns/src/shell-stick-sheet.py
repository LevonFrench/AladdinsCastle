"""Assemble authored CPU previews only, without a renderer. GPL-3.0-only."""
import argparse
from array import array
from pathlib import Path
import sys
def main():
    import bpy
    parser=argparse.ArgumentParser();parser.add_argument('--preview-dir',type=Path,required=True)
    args=parser.parse_args(sys.argv[sys.argv.index('--')+1:])
    width,height=1536,1024;pixels=array('f',[.04,.04,.04,1])*(width*height)
    for row,mid in enumerate(('con-remote-shell','con-stick-grip')):
        for column,suffix in enumerate(('-front','','-held')):
            source=bpy.data.images.load(str((args.preview_dir/(mid+suffix+'.png')).resolve()),check_existing=False)
            tile=array('f',[0])*(512*512*4);source.pixels.foreach_get(tile)
            for y in range(512):
                start=((512*(1-row)+y)*width+column*512)*4
                pixels[start:start+512*4]=tile[y*512*4:(y+1)*512*4]
    sheet=bpy.data.images.new('shell-stick-sheet',width=width,height=height)
    sheet.pixels.foreach_set(pixels);sheet.filepath_raw=str((args.preview_dir/'shell-stick-sheet.png').resolve());sheet.file_format='PNG';sheet.save()
    print('RESULT shell-stick CPU sheet: shell/stick rows; side/threequarter/held columns')
if __name__=='__main__':main()
