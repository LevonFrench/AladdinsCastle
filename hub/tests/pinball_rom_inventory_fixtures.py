# SPDX-License-Identifier: GPL-3.0-only
"""Synthetic ROM inventory cases; no real tables, ROMs or downloads."""
import json, os, pathlib, subprocess, sys, tempfile, unittest, zipfile
from pinball_fixtures import table

HUB = sys.argv.pop(1)
SCRIPT = 'Const cGameName="{name}"\nLoadVPM "01560000","sys80.vbs",3.26\nController.GameName=cGameName'

def flat_zip(path):
    with zipfile.ZipFile(path,'w') as archive:archive.writestr('chip.rom',b'synthetic bytes')

def scan(root, roms, pinmame, name):
    tables=root/'tables';tables.mkdir(exist_ok=True)
    table(tables/'fixture.vpx','Fixture (Synthetic 2000)','Test','1',SCRIPT.format(name=name))
    return json.loads(subprocess.check_output([HUB,'scan-pinball',str(tables),'',str(roms),'',str(pinmame)]))['tables'][0]

class Checks(unittest.TestCase):
    def test_nested_zip_extracted_and_named_member_root(self):
        with tempfile.TemporaryDirectory() as tmp:
            root=pathlib.Path(tmp);roms=root/'roms';(roms/'nested').mkdir(parents=True);pinmame=root/'VPinMAME';pinmame.mkdir()
            flat_zip(roms/'nested'/'NESTED.ZIP')
            self.assertEqual(scan(root,roms,pinmame,'nested')['romState'],'ready')
            extracted=roms/'other'/'EXTRACTED';extracted.mkdir(parents=True)
            (extracted/'chip.rom').write_bytes(b'synthetic bytes')
            self.assertEqual(scan(root,roms,pinmame,'extracted')['romState'],'ready')
            self.assertEqual(scan(root,roms,pinmame,'other')['romState'],'unknown')
            with zipfile.ZipFile(roms/'renamed.zip','w') as archive:archive.writestr('actual/chip.rom',b'synthetic bytes')
            self.assertEqual(scan(root,roms,pinmame,'actual')['romState'],'unknown')
            with zipfile.ZipFile(roms/'random.zip','w') as archive:archive.writestr('unrelated.rom',b'synthetic bytes')
            self.assertEqual(scan(root,roms,pinmame,'unrelated')['romState'],'ROM missing')
            (roms/'unrelated'/'readme.txt').parent.mkdir()
            (roms/'unrelated'/'readme.txt').write_text('synthetic notes',encoding='utf-8')
            self.assertEqual(scan(root,roms,pinmame,'unrelated')['romState'],'ROM missing')

    def test_alias_chain_case_cycle_ambiguity_and_unsafe_target(self):
        with tempfile.TemporaryDirectory() as tmp:
            root=pathlib.Path(tmp);roms=root/'roms';roms.mkdir();pinmame=root/'VPinMAME';pinmame.mkdir()
            flat_zip(roms/'TARGET.zip')
            aliases=pinmame/'VPMAlias.txt'
            aliases.write_text('First,Second\nSECOND,TaRgEt\ncycle_a,cycle_b\ncycle_b,cycle_a\nambiguous,target\nambiguous,other\nunsafe,../outside\nlocal,absent\n',encoding='utf-8')
            flat_zip(roms/'local.zip')
            self.assertEqual(scan(root,roms,pinmame,'FIRST')['romState'],'ready')
            for name in ('cycle_a','ambiguous','unsafe','local'):
                self.assertEqual(scan(root,roms,pinmame,name)['romState'],'unknown')

    def test_empty_folder_and_links_do_not_prove_missing_or_present(self):
        with tempfile.TemporaryDirectory() as tmp:
            root=pathlib.Path(tmp);roms=root/'roms';roms.mkdir();pinmame=root/'VPinMAME';pinmame.mkdir()
            self.assertEqual(scan(root,roms,pinmame,'absent')['romState'],'unknown')
            elsewhere=root/'outside.zip';flat_zip(elsewhere)
            flat_zip(roms/'other.zip')
            try:os.symlink(elsewhere,roms/'linked.zip')
            except OSError:return
            row=scan(root,roms,pinmame,'linked')
            self.assertEqual(row['romState'],'unknown')
            self.assertIn('inspected',row['romReason'])


    def test_valid_flat_zip_and_unavailable_zip_metadata(self):
        with tempfile.TemporaryDirectory() as tmp:
            root=pathlib.Path(tmp);roms=root/'roms';roms.mkdir();pinmame=root/'VPinMAME';pinmame.mkdir()
            flat_zip(roms/'other.zip')
            self.assertEqual(scan(root,roms,pinmame,'absent')['romState'],'ROM missing')
            with zipfile.ZipFile(roms/'commented.zip','w') as archive:
                archive.writestr('chip.rom',b'synthetic bytes');archive.comment=b'synthetic archive comment'
            self.assertEqual(scan(root,roms,pinmame,'commented')['romState'],'ready')
            (roms/'broken.zip').write_bytes(b'not a ZIP')
            row=scan(root,roms,pinmame,'absent')
            self.assertEqual(row['romState'],'unknown')
            self.assertIn('inspected',row['romReason'])
            self.assertEqual(scan(root,roms,pinmame,'broken')['romState'],'unknown')

    def test_related_revision_or_clone_archive_is_not_proof_of_missing(self):
        with tempfile.TemporaryDirectory() as tmp:
            root=pathlib.Path(tmp);roms=root/'roms';roms.mkdir();pinmame=root/'VPinMAME';pinmame.mkdir()
            flat_zip(roms/'machine_l2.zip');flat_zip(roms/'fixture.zip')
            for name in ('machine_l1','fixtureb','fixture123'):
                row=scan(root,roms,pinmame,name)
                self.assertEqual(row['romState'],'unknown');self.assertIn('coverage',row['romReason'])
            self.assertEqual(scan(root,roms,pinmame,'unrelated_absent')['romState'],'ROM missing')

if __name__=='__main__':unittest.main()
