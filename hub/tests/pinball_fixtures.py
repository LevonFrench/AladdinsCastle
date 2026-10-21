# SPDX-License-Identifier: GPL-3.0-only
"""Synthetic OLE/BIFF fixtures; no game content or network access."""
import os
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
    def helper_json(self, *arguments):
        completed=subprocess.run([HUB,*arguments],capture_output=True,text=True)
        self.assertEqual(completed.returncode,0,'Diagnostic helper failed: '+completed.stderr)
        value=json.loads(completed.stdout)
        self.assertIsInstance(value,dict,'Diagnostic helper must return a JSON object')
        return value
    def test_normal_plan_preserves_vpx_settings_and_library(self):
        with tempfile.TemporaryDirectory() as tmp:
            p=pathlib.Path(tmp); f=p/'ready table.vpx'
            table(f,'Ready (Synthetic 2000)','Test','1.0','Const VRRoom=1\nVRRoomCabinet.Visible=True')
            original=f.read_bytes()
            for mode in ('desktop','vr'):
                plan=self.helper_json('plan-pinball',str(f),HUB,str(p/'unused'),mode)
                self.assertIs(plan.get('launchAllowed'),True)
                self.assertEqual(plan['args'][:2],['-Play',f.as_posix()])
                if mode=='vr':
                    # The headset is asked for through a Hub-owned file under the owned root; planning creates nothing.
                    self.assertEqual(plan['args'][2],'-TableIni');owned=pathlib.Path(plan['args'][3])
                    self.assertEqual(owned.parent,p/'unused'/'user'/'pinball');self.assertRegex(owned.name,r'^[0-9a-f]{64}[.]ini$')
                    self.assertEqual(len(plan['args']),4);self.assertEqual(plan['tableSettingsSeed'],'')
                else:self.assertEqual(len(plan['args']),2)
                self.assertEqual(pathlib.Path(plan['cwd']),pathlib.Path(HUB).parent)
                self.assertNotIn('settings',plan)
                self.assertIn('normal settings',plan['help'])
                self.assertIn('scores',plan['help'])
                self.assertFalse((p/'unused').exists());self.assertEqual(f.read_bytes(),original)
            self.assertEqual({x.name for x in p.iterdir()},{'ready table.vpx'})
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
            vpm='\nLoadVPM "01560000","sys80.vbs",3.26\nController.GameName=cGameName'
            for name,script in [('database',''),('script',vr),('flat',''),('present',vr+'Const cGameName="synthetic_rom"'+vpm),('missing',vr+'Const cGameName="absent_rom"'+vpm),('dynamic',vr+'cGameName=ChooseRom()'),('em',vr+'Const cGameName="em_hiscore"\nExecuteGlobal GetTextFile("controller.vbs")')]:
                table(lib/(name+'.vpx'),name+' (Synthetic 2000)','Test Author','1.0',script)
            (roms/'synthetic_rom.zip').write_bytes(b'synthetic')
            (lib/'broken.vpx').write_bytes(b'not OLE'); (lib/'nested'/'deeper').mkdir(parents=True);table(lib/'nested'/'deeper'/'nested.vpx','nested','Test','1','')
            # A folder link to the library is never followed: no loop, no table listed twice.
            try: os.symlink(lib,lib/'loop',target_is_directory=True)
            except OSError: pass
            db=p/'db.json';db.write_text(json.dumps([dict(name='database',manufacturer='Synthetic',year=2000,tableFiles=[dict(authors=['Test Author'],version='1.0',features=['VR'],urls=[dict(url='https://invalid.example/download')])])]))
            before={x.name:x.read_bytes() for x in lib.glob('*.vpx')}
            result=json.loads(subprocess.check_output([HUB,'scan-pinball',str(lib),str(db),str(roms)]))
            self.assertEqual(result['total'],9); rows={x['title'].split(' (')[0]:x for x in result['tables']}
            self.assertIn('nested',rows) # Tables in subfolders are listed.
            self.assertEqual(rows['database']['evidence'],['VPS: VR']);self.assertEqual(rows['script']['evidence'],['script: VR room'])
            self.assertFalse(rows['flat']['vrReady']);self.assertEqual(rows['script']['romState'],'needs no ROM')
            self.assertEqual(rows['present']['romState'],'ready');self.assertEqual(rows['missing']['romState'],'ROM missing')
            self.assertEqual(rows['em']['romState'],'needs no ROM');self.assertNotIn('rom',rows['em'])
            self.assertEqual(rows['dynamic']['romState'],'unknown');self.assertNotIn('invalid.example',json.dumps(result))
            self.assertEqual(before,{x.name:x.read_bytes() for x in lib.glob('*.vpx')})
            plan=json.loads(subprocess.check_output([HUB,'plan-pinball',str(lib/'script.vpx'),HUB,str(p/'owned'),'desktop']))
            self.assertEqual(plan['args'],['-Play',(lib/'script.vpx').as_posix()]);self.assertTrue(plan['launchAllowed'])
            self.assertFalse((p/'owned').exists())

if __name__=='__main__':
    HUB=sys.argv.pop(1);unittest.main()
