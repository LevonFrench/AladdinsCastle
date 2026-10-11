# SPDX-License-Identifier: GPL-3.0-only
"""Synthetic OLE/BIFF fixtures; no game content or network access."""
import json, struct, sys, tempfile, pathlib, subprocess, unittest
EOC, FREE = 0xfffffffe, 0xffffffff
def table(path, title, author, version, script, mini=False):
    streams = [('TableName', title.encode('utf-16le')), ('AuthorName', author.encode('utf-16le')),
               ('TableVersion', version.encode('utf-16le')),
               ('GameData', struct.pack('<I',4)+b'CODE'+struct.pack('<I',len(script))+script.encode()+struct.pack('<I',4)+b'ENDB')]
    # Ordinary streams are padded to >=4096; a tiny CFB file, not a playable table.
    data=[]; fat=[EOC]; entries=[]
    def entry(name, kind, child=FREE, right=FREE, start=EOC, size=0):
        b=bytearray(128); n=(name+'\0').encode('utf-16le'); b[:len(n)]=n
        struct.pack_into('<HBBIII',b,64,len(n),kind,1,FREE,right,child)
        struct.pack_into('<IQ',b,116,start,size); return b
    entries=[entry('Root Entry',5,child=1),entry('TableInfo',1,child=3,right=2),entry('GameStg',1,child=6)]
    for i,(name,content) in enumerate(streams):
        padded=content.ljust(4096,b'\0'); start=2+sum(len(x)//512 for x in data)
        entries.append(entry(name,2,right=i+4 if i<2 else FREE,start=start,size=len(padded)))
        data.append(padded)
    directory=b''.join(entries).ljust(1024,b'\0')
    # directory uses sectors 0,1; stream sectors start at 2; FAT last
    sectors=[directory[:512],directory[512:]]+[part[j:j+512] for part in data for j in range(0,len(part),512)]
    fat=[1,EOC]
    for part in data:
        start=len(fat); fat.extend([start+i+1 for i in range(7)]+[EOC])
    fatid=len(sectors); fat.append(0xfffffffd); sectors.append(struct.pack('<128I',*(fat+[FREE]*(128-len(fat)))))
    h=bytearray(512); h[:8]=bytes.fromhex('d0cf11e0a1b11ae1'); struct.pack_into('<HHHH',h,24,0x3e,3,0xfffe,9);struct.pack_into('<H',h,32,6)
    struct.pack_into('<IIIIIIIII',h,40,0,1,0,0,4096,EOC,0,EOC,0)
    struct.pack_into('<109I',h,76,fatid,*([FREE]*108)); raw=bytearray(h+b''.join(sectors))
    if mini:
        mini_data=b'';mini_fat=[]
        for i,(_,content) in enumerate(streams[:3]):
            start=len(mini_fat);count=(len(content)+63)//64
            mini_fat += [start+j+1 for j in range(count-1)]+[EOC]
            mini_data += content.ljust(count*64,b'\0')
            struct.pack_into('<IQ',raw,512+(i+3)*128+116,start,len(content))
        assert len(mini_data)<=512
        first=len(sectors);struct.pack_into('<IQ',raw,512+116,first,len(mini_data))
        struct.pack_into('<II',raw,60,first+1,1)
        struct.pack_into('<II',raw,(fatid+1)*512+first*4,EOC,EOC)
        raw += mini_data.ljust(512,b'\0')+struct.pack('<128I',*(mini_fat+[FREE]*(128-len(mini_fat))))
    path.write_bytes(raw)

class Checks(unittest.TestCase):
    def test_blocked_plan_explains_actual_isolation_gaps(self):
        with tempfile.TemporaryDirectory() as tmp:
            p=pathlib.Path(tmp); f=p/'ready.vpx'
            table(f,'Ready (Synthetic 2000)','Test','1.0','Const VRRoom=1\nVRRoomCabinet.Visible=True')
            original=f.read_bytes()
            row=json.loads(subprocess.check_output([HUB,'scan-pinball',str(p)]))['tables'][0]
            self.assertTrue(row['vrReady']);self.assertEqual(row['romState'],'needs no ROM')
            for mode in ('desktop','vr'):
                plan=json.loads(subprocess.check_output([HUB,'plan-pinball',str(f),HUB,str(p/'owned'),mode]))
                self.assertFalse(plan['launchAllowed'])
                blockers={x['id']:x['text'] for x in plan['isolationBlockers']}
                self.assertEqual(set(blockers),{'preferences','table-data','scripts-and-plugins'})
                self.assertIn('settings folder',blockers['preferences'])
                self.assertIn('beside your tables',blockers['table-data'])
                self.assertIn('scripts',blockers['scripts-and-plugins']);self.assertIn('plugins',blockers['scripts-and-plugins'])
                self.assertTrue(plan['help'].startswith(plan['reason']))
                for text in blockers.values():self.assertIn(text,plan['help'])
                self.assertNotIn('https://',plan['help'])
                self.assertFalse((p/'owned').exists());self.assertEqual(f.read_bytes(),original)
            self.assertEqual({x.name for x in p.iterdir()},{'ready.vpx'})
    def test_minifat_and_directory_cycle(self):
        with tempfile.TemporaryDirectory() as tmp:
            p=pathlib.Path(tmp);f=p/'mini.vpx';table(f,'Mini (Synthetic 2000)','Test','1.0','Const VRRoom=1\nVRRoomCabinet.Visible=True',mini=True)
            result=json.loads(subprocess.check_output([HUB,'scan-pinball',str(p)]))['tables'][0]
            self.assertEqual(result['title'],'Mini (Synthetic 2000)');self.assertTrue(result['vrReady'])
            raw=bytearray(f.read_bytes());fat=struct.unpack_from('<I',raw,76)[0];struct.pack_into('<I',raw,(fat+1)*512,0);f.write_bytes(raw)
            result=json.loads(subprocess.check_output([HUB,'scan-pinball',str(p)]))['tables'][0]
            self.assertFalse(result['vrReady']);self.assertIn('error',result)
    def test_conditions_and_comments_are_not_switches(self):
        with tempfile.TemporaryDirectory() as tmp:
            p=pathlib.Path(tmp)
            for i,script in enumerate(["' Const VRRoom=1\nVRRoomCabinet.Visible=True",'If VRRoom=1 Then\nVRRoomCabinet.Visible=True\nEnd If','Const VRRoom=1\nOrdinaryObject.Visible=True','Debug.Print "Then VRRoom=1"\nVRRoomCabinet.Visible=True']):table(p/(str(i)+'.vpx'),'Noise','Test','1.0',script)
            result=json.loads(subprocess.check_output([HUB,'scan-pinball',str(p)]))
            self.assertEqual(result['vrReady'],0)
    def test_dynamic_rom_cannot_borrow_a_literal(self):
        with tempfile.TemporaryDirectory() as tmp:
            p=pathlib.Path(tmp);table(p/'dynamic.vpx','Dynamic','Test','1.0','Const VRRoom=1\nVRRoomCabinet.Visible=True\ncGameName="synthetic"\ncGameName=ChooseRom()')
            result=json.loads(subprocess.check_output([HUB,'scan-pinball',str(p)]))['tables'][0]
            self.assertEqual(result['romState'],'unknown');self.assertNotIn('rom',result)
    def test_only_exact_local_media_is_associated(self):
        with tempfile.TemporaryDirectory() as tmp:
            p=pathlib.Path(tmp);lib=p/'tables';lib.mkdir();art=p/'media';art.mkdir()
            table(lib/'fixture.vpx','Fixture','Test','1.0','')
            (art/'fixture.png').write_bytes(b'synthetic media metadata; never decoded')
            (art/'fixture.directb2s').write_bytes(b'synthetic metadata; never decoded')
            result=json.loads(subprocess.check_output([HUB,'scan-pinball',str(lib),'','',str(art)]))['tables'][0]
            self.assertEqual(pathlib.Path(result['wheelPath']),art/'fixture.png')
            self.assertEqual(pathlib.Path(result['backglassPath']),art/'fixture.directb2s')
            (art/'fixture.png').rename(art/'unrelated.png')
            result=json.loads(subprocess.check_output([HUB,'scan-pinball',str(lib),'','',str(art)]))['tables'][0]
            self.assertNotIn('wheelPath',result)
    def test_classification_and_refusals(self):
        with tempfile.TemporaryDirectory() as tmp:
            p=pathlib.Path(tmp); lib=p/'tables';lib.mkdir();roms=p/'roms';roms.mkdir()
            vr='Const VRRoom = 1\nVRRoomCabinet.Visible = VRRoom\n'
            for name,script in [('database',''),('script',vr),('flat',''),('present',vr+'Const cGameName="synthetic_rom"'),('missing',vr+'Const cGameName="absent_rom"'),('dynamic',vr+'cGameName=ChooseRom()')]:
                table(lib/(name+'.vpx'),name+' (Synthetic 2000)','Test Author','1.0',script)
            (roms/'synthetic_rom.zip').write_bytes(b'synthetic')
            (lib/'broken.vpx').write_bytes(b'not OLE'); (lib/'nested').mkdir();table(lib/'nested'/'ignored.vpx','ignored','Test','1','')
            db=p/'db.json';db.write_text(json.dumps([dict(name='database',manufacturer='Synthetic',year=2000,tableFiles=[dict(authors=['Test Author'],version='1.0',features=['VR'],urls=[dict(url='https://invalid.example/download')])])]))
            before={x.name:x.read_bytes() for x in lib.glob('*.vpx')}
            result=json.loads(subprocess.check_output([HUB,'scan-pinball',str(lib),str(db),str(roms)]))
            self.assertEqual(result['total'],7); rows={x['title'].split(' (')[0]:x for x in result['tables']}
            self.assertEqual(rows['database']['evidence'],['VPS: VR']);self.assertEqual(rows['script']['evidence'],['script: VR room'])
            self.assertFalse(rows['flat']['vrReady']);self.assertEqual(rows['script']['romState'],'needs no ROM')
            self.assertEqual(rows['present']['romState'],'ready');self.assertEqual(rows['missing']['romState'],'ROM missing')
            self.assertEqual(rows['dynamic']['romState'],'unknown');self.assertNotIn('invalid.example',json.dumps(result))
            self.assertEqual(before,{x.name:x.read_bytes() for x in lib.glob('*.vpx')})
            plan=json.loads(subprocess.check_output([HUB,'plan-pinball',str(lib/'script.vpx'),HUB,str(p/'owned'),'desktop']))
            self.assertIn('-Ini',plan['args']);self.assertIn('-TableIni',plan['args']);self.assertFalse(plan['launchAllowed'])
            self.assertFalse((p/'owned').exists())

if __name__=='__main__':
    HUB=sys.argv.pop(1);unittest.main()
