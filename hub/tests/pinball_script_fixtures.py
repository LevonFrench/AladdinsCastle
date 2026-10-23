# SPDX-License-Identifier: GPL-3.0-only
"""Synthetic VBScript metadata cases; scripts are never executed."""
import json, pathlib, subprocess, sys, tempfile, unittest, zipfile
sys.dont_write_bytecode = True  # Shared fixtures must leave the source tree clean for packaging.
from pinball_fixtures import table
HUB=sys.argv.pop(1)

class ScriptChecks(unittest.TestCase):
    def rows(self, scripts, installed=('fixture_a','fixture_b')):
        with tempfile.TemporaryDirectory() as tmp:
            root=pathlib.Path(tmp);lib=root/'tables';lib.mkdir();roms=root/'roms';roms.mkdir();vpm=root/'VPinMAME';vpm.mkdir()
            for name in installed:
                with zipfile.ZipFile(roms/(name+'.zip'),'w') as archive:archive.writestr('fixture.rom',b'synthetic presence fixture')
            for name,script in scripts.items():table(lib/(name+'.vpx'),name,'Fixture','1',script)
            run=subprocess.run([HUB,'scan-pinball',str(lib),'',str(roms),'',str(vpm)],capture_output=True,text=True,timeout=30)
            self.assertEqual(run.returncode,0,run.stderr)
            return {pathlib.Path(r['path']).stem:r for r in json.loads(run.stdout)['tables']}

    def test_literal_declarations_and_controller_properties(self):
        rows=self.rows({
            'const_list':'Const cGameName="fixture_a", UseSolenoids=1, UseLamps=0\nLoadVPM "x", "fixture.vbs", 1\nWith Controller\n.GameName=cGameName\nEnd With',
            'game_name':'GameName="fixture_a"\nLoadVPM "x","fixture.vbs",1\nController.GameName=GameName',
            'property':'Set engine=CreateObject("VPinMAME.Controller")\nengine.GameName="fixture_b"',
            'with':'Set engine=CreateObject("VPinMAME.Controller")\nWith engine\n.GameName="fixture_a"\nEnd With',
            'continued':'Const cGameName = _\n "fixture_a"\nLoadVPM "x","fixture.vbs",1',
            'comment_continuation':'\' ignored underscore _\nLoadVPM "x","fixture.vbs",1\nController.GameName="fixture_a"',
            'missing':'LoadVPM "x","fixture.vbs",1\nController.GameName="fixture_absent"'})
        for name in ('const_list','game_name','property','with','continued','comment_continuation'):
            self.assertEqual(rows[name]['romState'],'ready',name)
        self.assertEqual(rows['missing']['romState'],'ROM missing')

    def test_dmd_and_backglass_names_are_not_pinmame_roms(self):
        rows=self.rows({
            'flex':'Const cGameName="fixture_absent"\nSet FlexDMD=CreateObject("FlexDMD.FlexDMD")\nFlexDMD.GameName=cGameName',
            'with_flex':'Const cGameName="fixture_absent"\nSet display=CreateObject("FlexDMD.FlexDMD")\nWith display\n.GameName=cGameName\nEnd With',
            'ultra':'Set UltraDMD=CreateObject("UltraDMD.DMDObject")\nGameName="fixture_absent"',
            'backglass':'Set Controller=CreateObject("B2S.Server")\nController.GameName="fixture_absent"',
            'noise':'Rem LoadVPM "x"\n\' Set engine=CreateObject("VPinMAME.Controller")\nDebug.Print "LoadVPM and Controller.GameName=fixture_absent"'})
        for name,row in rows.items():
            self.assertEqual(row['romState'],'needs no ROM',name);self.assertNotIn('rom',row)

    def test_only_pinmame_receiver_supplies_rom_when_flex_is_also_used(self):
        script='Set engine=CreateObject("VPinMAME.Controller")\nSet display=CreateObject("FlexDMD.FlexDMD")\nengine.GameName="fixture_a"\ndisplay.GameName="fixture_absent"'
        row=self.rows({'mixed':script})['mixed'];self.assertEqual(row['romState'],'ready');self.assertEqual(row['rom'],'fixture_a')

    def test_arrays_and_multiple_choices_do_not_borrow_a_rom(self):
        load='LoadVPM "x","fixture.vbs",1\n'
        rows=self.rows({
            'array':load+'names=Array("fixture_a","fixture_b")\nController.GameName=names(choice)',
            'index':load+'names=Array("fixture_a","fixture_absent")\nController.GameName=names(0)',
            'partial':load+'cGameName="fixture_a"\nIf choice Then cGameName="fixture_absent"\nController.GameName=cGameName',
            'dynamic':load+'cGameName="fixture_a"\ncGameName=ChooseRom()\nController.GameName=cGameName',
            'concat':load+'prefix="fixture_"\nController.GameName=prefix & "a"'})
        for name in ('array','index','concat'):self.assertEqual(rows[name]['romState'],'ready',name)
        for name in ('partial','dynamic'):self.assertEqual(rows[name]['romState'],'unknown',name)
        self.assertNotIn('rom',rows['dynamic'])

    def test_quoted_property_and_unreadable_script_do_not_block(self):
        rows=self.rows({
            'quoted':'LoadVPM "x","fixture.vbs",1\nIf x Then Debug.Print "Then Controller.GameName=fixture_absent"',
            'display_only':'LoadVPM "x","fixture.vbs",1\nConst cGameName="fixture_absent"\nSet display=CreateObject("FlexDMD.FlexDMD")\ndisplay.GameName=cGameName',
            'malformed':'LoadVPM "x","fixture.vbs",1\nController.GameName="unterminated',
            'blank':'\n'*100001})
        for name in ('quoted','display_only','malformed'):
            self.assertEqual(rows[name]['romState'],'unknown',name);self.assertNotIn('error',rows[name])
        self.assertEqual(rows['blank']['romState'],'needs no ROM')

    def test_conditional_controllers_and_indexed_mutations_stay_unknown(self):
        load='LoadVPM "x","fixture.vbs",1\n'
        rows=self.rows({
            'conditional':'Const cGameName="fixture_absent"\nIf False Then LoadVPM "x","fixture.vbs",1',
            'proxy':load+'If useB2S Then Set Controller=CreateObject("B2S.Server")\nController.GameName="fixture_absent"',
            'conditional_sink':load+'If False Then Controller.GameName="fixture_absent"',
            'conditional_name':load+'If False Then cGameName="fixture_absent"\nController.GameName=cGameName',
            'mixed_object':'Set engine=CreateObject("FlexDMD.FlexDMD")\nIf choice Then Set engine=CreateObject("VPinMAME.Controller")\nengine.GameName="fixture_absent"',
            'mutated':load+'names=Array("fixture_absent")\nnames(0)="fixture_a"\nController.GameName=names(choice)'})
        for name,row in rows.items():self.assertEqual(row['romState'],'unknown',name)

    def test_embedded_loader_definitions_do_not_imply_pinmame_use(self):
        helpers='Sub LoadVPM(version,file,revision)\nLoadVPinMAME\nEnd Sub\nSub LoadVPMALT()\nLoadController("VPMALT")\nEnd Sub\nSub LoadVPinMAME()\nSet Controller=CreateObject("VPinMAME.Controller")\nEnd Sub\nSub LoadController()\nSet Controller=CreateObject("B2S.Server")\nEnd Sub\n'
        rows=self.rows({
            'display':helpers+'Const cGameName="display-name"\nSet display=CreateObject("FlexDMD.FlexDMD")\ndisplay.GameName=cGameName',
            'vpm':helpers+'Const cGameName="fixture_a"\nLoadVPM "x","fixture.vbs",1\nWith Controller\n.GameName=cGameName\nEnd With',
            'score_name':'Const cGameName="A score name"',
            'direct_loader':helpers+'LoadController("VPM")\nController.GameName="fixture_a"',
            'dynamic_loader':helpers+'LoadController(kind)\nController.GameName="fixture_absent"',
            'unused':helpers+'Sub StartVPM()\nLoadVPM "x","fixture.vbs",1\nEnd Sub\nConst cGameName="fixture_absent"',
            'custom':'Sub LoadVPM()\nSet display=CreateObject("FlexDMD.FlexDMD")\nEnd Sub\nLoadVPM\nConst cGameName="fixture_absent"',
            'custom_noise':'Sub LoadVPM()\nDebug.Print "VPinMAME.Controller"\nEnd Sub\nLoadVPM\nConst cGameName="fixture_absent"',
            'unused_underscore':'Sub Start_VPM()\nLoadVPM "x","fixture.vbs",1\nEnd Sub\nConst cGameName="fixture_absent"',
            'object_method':'Sub StartVPM()\nLoadVPM "x","fixture.vbs",1\nEnd Sub\nobj.StartVPM\nConst cGameName="fixture_absent"',
            'conditional_helper':'Sub StartVPM()\nLoadVPM "x","fixture.vbs",1\nEnd Sub\nIf False Then StartVPM\nConst cGameName="fixture_absent"',
            'unused_property':'Property Get Engine()\nLoadVPM "x","fixture.vbs",1\nEnd Property\nConst cGameName="fixture_absent"',
            'conditional_block':'If choice Then\nLoadVPM "x","fixture.vbs",1\nEnd If\nConst cGameName="fixture_absent"',
            'callback':helpers+'Sub Table_Init()\nStartVPM\nEnd Sub\nSub StartVPM()\nLoadVPM "x","fixture.vbs",1\nController.GameName="fixture_a"\nEnd Sub',
            'unsafe_rom':'LoadVPM "x","fixture.vbs",1\nController.GameName="../fixture_a"'})
        for name in ('display','score_name'):self.assertEqual(rows[name]['romState'],'needs no ROM',name)
        self.assertEqual(rows['vpm']['romState'],'ready')
        self.assertEqual(rows['direct_loader']['romState'],'ready')
        self.assertEqual(rows['dynamic_loader']['romState'],'unknown')
        self.assertEqual(rows['unused']['romState'],'needs no ROM')
        self.assertEqual(rows['custom']['romState'],'unknown')
        self.assertEqual(rows['callback']['romState'],'unknown') # Helper call conditions are not evaluated.
        for name in ('custom_noise','conditional_helper','conditional_block'):self.assertEqual(rows[name]['romState'],'unknown',name)
        for name in ('unused_underscore','object_method','unused_property'):self.assertEqual(rows[name]['romState'],'needs no ROM',name)
        self.assertEqual(rows['unsafe_rom']['romState'],'unknown')

if __name__=='__main__':unittest.main()
