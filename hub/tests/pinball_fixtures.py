# SPDX-License-Identifier: GPL-3.0-only
"""Synthetic OLE/BIFF fixtures; no game content or network access."""
import os
import json, struct, sys, tempfile, pathlib, subprocess, unittest
EOC, FREE = 0xfffffffe, 0xffffffff
def table(path, title, author, version, script, mini=False, declared_script_size=None):
    script_bytes = script.encode('utf-8')
    code_size = len(script_bytes) if declared_script_size is None else declared_script_size
    streams = [('TableName', title.encode('utf-16le')), ('AuthorName', author.encode('utf-16le')),
               ('TableVersion', version.encode('utf-16le')),
               ('GameData', struct.pack('<I',4)+b'CODE'+struct.pack('<I',code_size)+script_bytes+struct.pack('<I',4)+b'ENDB')]
    def entry(name, kind, child=FREE, right=FREE, start=EOC, size=0):
        b=bytearray(128); n=(name+'\0').encode('utf-16le'); b[:len(n)]=n
        struct.pack_into('<HBBIII',b,64,len(n),kind,1,FREE,right,child)
        struct.pack_into('<IQ',b,116,start,size); return b
    entries=[entry('Root Entry',5,child=1),entry('TableInfo',1,child=3,right=2),entry('GameStg',1,child=6)]
    directory_sectors=(len(entries)+len(streams)+3)//4
    sectors=[None]*directory_sectors
    fat=[FREE]*(directory_sectors)
    for i in range(directory_sectors): fat[i]=i+1 if i+1<directory_sectors else EOC
    mini_data=bytearray();mini_fat=[]; mini_root_start=EOC
    regular=[]
    for i,(name,content) in enumerate(streams):
        if mini and i<3:
            start=len(mini_fat);count=(len(content)+63)//64
            mini_fat.extend([start+j+1 for j in range(count-1)]+[EOC])
            mini_data.extend(content.ljust(count*64,b'\0'))
            entries.append(entry(name,2,right=i+4 if i<2 else FREE,start=start,size=len(content)))
        else:
            padded=content.ljust(max(4096,((len(content)+511)//512)*512),b'\0')
            count=len(padded)//512;start=len(fat)
            fat.extend([start+j+1 for j in range(count-1)]+[EOC])
            regular.append((start,padded))
            entries.append(entry(name,2,right=i+4 if i<2 else FREE,start=start,size=len(padded)))
    if mini_data:
        mini_root_start=len(fat);count=(len(mini_data)+511)//512
        fat.extend([mini_root_start+j+1 for j in range(count-1)]+[EOC])
    mini_fat_start=EOC
    if mini_fat:
        mini_fat_bytes=struct.pack('<%dI'%len(mini_fat),*mini_fat)
        mini_fat_bytes=mini_fat_bytes.ljust(((len(mini_fat_bytes)+511)//512)*512,b'\0')
        mini_fat_start=len(fat);count=len(mini_fat_bytes)//512
        fat.extend([mini_fat_start+j+1 for j in range(count-1)]+[EOC])
    directory=b''.join(entries).ljust(directory_sectors*512,b'\0')
    for i in range(directory_sectors): sectors[i]=directory[i*512:(i+1)*512]
    for start,content in regular:
        for j in range(0,len(content),512): sectors.append(content[j:j+512])
    if mini_data:
        padded=bytes(mini_data).ljust(((len(mini_data)+511)//512)*512,b'\0')
        for j in range(0,len(padded),512): sectors.append(padded[j:j+512])
    if mini_fat:
        for j in range(0,len(mini_fat_bytes),512): sectors.append(mini_fat_bytes[j:j+512])
    # Add enough FAT and DIFAT sectors to describe every sector, including both tables.
    fat_sector_count=difat_sector_count=0
    while True:
        total=len(sectors)+fat_sector_count+difat_sector_count
        next_fat=(total+127)//128
        next_difat=max(0,(next_fat-109+126)//127)
        if (next_fat,next_difat)==(fat_sector_count,difat_sector_count): break
        fat_sector_count,difat_sector_count=next_fat,next_difat
    fatid=len(sectors)
    fat.extend([FREE]*(fat_sector_count*128-len(fat)))
    for i in range(fat_sector_count):
        sid=fatid+i;fat[sid]=0xfffffffd
        sectors.append(None)
    difatid=len(sectors)
    for i in range(difat_sector_count):
        fat[difatid+i]=0xfffffffc
        sectors.append(None)
    for i in range(directory_sectors):
        fat[i]=i+1 if i+1<directory_sectors else EOC
    # Directory, regular streams, mini stream and MiniFAT already have their
    # chain links in `fat`; FAT and DIFAT sectors follow those data sectors.
    for i in range(fat_sector_count):
        sectors[fatid+i]=struct.pack('<128I',*fat[i*128:(i+1)*128])
    extra_fat_ids=[fatid+i for i in range(109,fat_sector_count)]
    for i in range(difat_sector_count):
        chunk=extra_fat_ids[i*127:(i+1)*127]
        next_sector=difatid+i+1 if i+1<difat_sector_count else EOC
        sectors[difatid+i]=struct.pack('<128I',*(chunk+[FREE]*(127-len(chunk))+[next_sector]))
    h=bytearray(512); h[:8]=bytes.fromhex('d0cf11e0a1b11ae1'); struct.pack_into('<HHHH',h,24,0x3e,3,0xfffe,9);struct.pack_into('<H',h,32,6)
    struct.pack_into('<IIIIIIIII',h,40,0,fat_sector_count,0,0,4096,mini_root_start,((len(mini_fat)+127)//128) if mini_fat else 0, difatid if difat_sector_count else EOC,difat_sector_count)
    if mini_fat: struct.pack_into('<I',h,60,mini_fat_start)
    header_fat_ids=[fatid+i for i in range(min(109,fat_sector_count))]
    struct.pack_into('<109I',h,76,*(header_fat_ids+[FREE]*(109-len(header_fat_ids))))
    raw=bytearray(h+b''.join(sectors))
    if mini:
        root=bytearray(128);root[:len(('Root Entry\0').encode('utf-16le'))]=('Root Entry\0').encode('utf-16le')
        struct.pack_into('<HBBIII',root,64,len(('Root Entry\0').encode('utf-16le')),5,1,FREE,FREE,1)
        struct.pack_into('<IQ',root,116,mini_root_start,len(mini_data))
        raw[512:640]=root
    path.write_bytes(raw)

class Checks(unittest.TestCase):
    def helper_json(self, *arguments):
        completed=subprocess.run([HUB,*arguments],capture_output=True,text=True,timeout=60)
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
            result=json.loads(subprocess.check_output([HUB,'scan-pinball',str(p)],timeout=60))['tables'][0]
            self.assertEqual(result['title'],'Mini (Synthetic 2000)');self.assertTrue(result['vrReady'])
            raw=bytearray(f.read_bytes());fat=struct.unpack_from('<I',raw,76)[0];struct.pack_into('<I',raw,(fat+1)*512,0);f.write_bytes(raw)
            result=json.loads(subprocess.check_output([HUB,'scan-pinball',str(p)],timeout=60))['tables'][0]
            self.assertFalse(result['vrReady']);self.assertIn('error',result)
    def test_large_code_stream_uses_multiple_fat_sectors(self):
        with tempfile.TemporaryDirectory() as tmp:
            p=pathlib.Path(tmp);f=p/'large-script.vpx'
            table(f,'Large (Synthetic 2000)','Test','1.0','Rem '+('x'*(3*1024*1024)))
            result=json.loads(subprocess.check_output([HUB,'scan-pinball',str(p)],timeout=60))['tables'][0]
            self.assertNotIn('error',result)
            self.assertEqual(result['romState'],'needs no ROM')
    def test_declared_code_size_over_limit_is_rejected(self):
        with tempfile.TemporaryDirectory() as tmp:
            p=pathlib.Path(tmp);f=p/'bad-size.vpx'
            table(f,'Bad size (Synthetic 2000)','Test','1.0','synthetic',declared_script_size=8*1024*1024+1)
            result=json.loads(subprocess.check_output([HUB,'scan-pinball',str(p)],timeout=60))['tables'][0]
            self.assertIn('error',result)
    def test_conditions_and_comments_are_not_switches(self):
        with tempfile.TemporaryDirectory() as tmp:
            p=pathlib.Path(tmp)
            for i,script in enumerate(["' Const VRRoom=1\nVRRoomCabinet.Visible=True",'If VRRoom=1 Then\nVRRoomCabinet.Visible=True\nEnd If','Const VRRoom=1\nOrdinaryObject.Visible=True','Debug.Print "Then VRRoom=1"\nVRRoomCabinet.Visible=True']):table(p/(str(i)+'.vpx'),'Noise','Test','1.0',script)
            result=json.loads(subprocess.check_output([HUB,'scan-pinball',str(p)],timeout=60))
            self.assertEqual(result['vrReady'],0)
    def test_dynamic_rom_cannot_borrow_a_literal(self):
        with tempfile.TemporaryDirectory() as tmp:
            p=pathlib.Path(tmp);table(p/'dynamic.vpx','Dynamic','Test','1.0','Const VRRoom=1\nVRRoomCabinet.Visible=True\ncGameName="synthetic"\ncGameName=ChooseRom()')
            result=json.loads(subprocess.check_output([HUB,'scan-pinball',str(p)],timeout=60))['tables'][0]
            self.assertEqual(result['romState'],'unknown');self.assertNotIn('rom',result)
    def test_only_exact_local_media_is_associated(self):
        with tempfile.TemporaryDirectory() as tmp:
            p=pathlib.Path(tmp);lib=p/'tables';lib.mkdir();art=p/'media';art.mkdir()
            table(lib/'fixture.vpx','Fixture','Test','1.0','')
            (art/'fixture.png').write_bytes(b'synthetic media metadata; never decoded')
            (art/'fixture.directb2s').write_bytes(b'synthetic metadata; never decoded')
            result=json.loads(subprocess.check_output([HUB,'scan-pinball',str(lib),'','',str(art)],timeout=60))['tables'][0]
            self.assertEqual(pathlib.Path(result['wheelPath']),art/'fixture.png')
            self.assertEqual(pathlib.Path(result['backglassPath']),art/'fixture.directb2s')
            (art/'fixture.png').rename(art/'unrelated.png')
            result=json.loads(subprocess.check_output([HUB,'scan-pinball',str(lib),'','',str(art)],timeout=60))['tables'][0]
            self.assertNotIn('wheelPath',result)
    def test_classification_and_refusals(self):
        with tempfile.TemporaryDirectory() as tmp:
            p=pathlib.Path(tmp); lib=p/'tables';lib.mkdir();roms=p/'roms';roms.mkdir();pinmame=p/'VPinMAME';pinmame.mkdir()
            vr='Const VRRoom = 1\nVRRoomCabinet.Visible = VRRoom\n'
            vpm='\nLoadVPM "01560000","sys80.vbs",3.26\nController.GameName=cGameName'
            for name,script in [('database',''),('script',vr),('flat',''),('present',vr+'Const cGameName="synthetic_rom"'+vpm),('missing',vr+'Const cGameName="absent_rom"'+vpm),('dynamic',vr+'cGameName=ChooseRom()'),('em',vr+'Const cGameName="em_hiscore"\nExecuteGlobal GetTextFile("controller.vbs")')]:
                table(lib/(name+'.vpx'),name+' (Synthetic 2000)','Test Author','1.0',script)
            import zipfile
            with zipfile.ZipFile(roms/'synthetic_rom.zip','w') as archive:archive.writestr('fixture.rom',b'synthetic')
            (lib/'broken.vpx').write_bytes(b'not OLE'); (lib/'nested'/'deeper').mkdir(parents=True);table(lib/'nested'/'deeper'/'nested.vpx','nested','Test','1','')
            # A folder link to the library is never followed: no loop, no table listed twice.
            try: os.symlink(lib,lib/'loop',target_is_directory=True)
            except OSError: pass
            db=p/'db.json';db.write_text(json.dumps([dict(name='database',manufacturer='Synthetic',year=2000,tableFiles=[dict(authors=['Test Author'],version='1.0',features=['VR'],urls=[dict(url='https://invalid.example/download')])])]))
            before={x.name:x.read_bytes() for x in lib.glob('*.vpx')}
            result=json.loads(subprocess.check_output([HUB,'scan-pinball',str(lib),str(db),str(roms),'',str(pinmame)],timeout=60))
            self.assertEqual(result['total'],9); rows={x['title'].split(' (')[0]:x for x in result['tables']}
            self.assertIn('nested',rows) # Tables in subfolders are listed.
            self.assertEqual(rows['database']['evidence'],['VPS: VR']);self.assertEqual(rows['script']['evidence'],['script: VR room'])
            self.assertFalse(rows['flat']['vrReady']);self.assertEqual(rows['script']['romState'],'needs no ROM')
            self.assertEqual(rows['present']['romState'],'ready');self.assertEqual(rows['missing']['romState'],'ROM missing')
            self.assertEqual(rows['em']['romState'],'needs no ROM');self.assertNotIn('rom',rows['em'])
            self.assertEqual(rows['dynamic']['romState'],'unknown');self.assertNotIn('invalid.example',json.dumps(result))
            self.assertEqual(before,{x.name:x.read_bytes() for x in lib.glob('*.vpx')})
            plan=json.loads(subprocess.check_output([HUB,'plan-pinball',str(lib/'script.vpx'),HUB,str(p/'owned'),'desktop'],timeout=60))
            self.assertEqual(plan['args'],['-Play',(lib/'script.vpx').as_posix()]);self.assertTrue(plan['launchAllowed'])
            self.assertFalse((p/'owned').exists())

if __name__=='__main__':
    HUB=sys.argv.pop(1);unittest.main()
