"""Per-game IR measurement scope, native opt-in and unchanged peer profiles."""
from copy import deepcopy
import configparser
from pathlib import Path
from tempfile import TemporaryDirectory
import unittest
from wii_shell_stick import ROOT,aim_calibration,generated_files,render_game_profile,render_profile,render_scripted_profile,scripted_bridge_profile

GAME='wii-resident-evil-umbrella-chronicles'
SYNTHETIC=dict(version='wii-aim/1',state='desktop-verified',total_yaw_degrees=31.5,total_pitch_degrees=24.25,
    vertical_offset_cm=7,emulated_distance_m=2,native='provisional-opt-in',
    evidence=dict(method='nine-point',samples=9,independent_review=True,scope='approximate-desktop-dsu',pointer_extent=.8,input_domain=[-.8,.8],target_rms_px=3,target_max_error_px=6))

def section(data):
    parser=configparser.ConfigParser(interpolation=None);parser.optionxform=str;parser.read_string(data.decode())
    return dict(parser['Wiimote1'])

class AimCalibrationTests(unittest.TestCase):
    def setUp(self):
        directory=ROOT/'.local/wii-aim-fixtures';directory.mkdir(parents=True,exist_ok=True)
        self.temp=TemporaryDirectory(dir=directory);self.addCleanup(self.temp.cleanup);self.root=Path(self.temp.name)
        for name in ('data/product.json','data/controls/wii/shell-and-stick.toml'):
            path=self.root/name;path.parent.mkdir(parents=True,exist_ok=True);path.write_bytes((ROOT/name).read_bytes())
        for path in (ROOT/'data/controls/wii/games').glob('*.toml'):
            dest=self.root/'data/controls/wii/games'/path.name;dest.parent.mkdir(parents=True,exist_ok=True)
            dest.write_text(path.read_text(encoding='utf-8').split('\n[aim_calibration]')[0],encoding='utf-8')

    def calibrated_fixture(self,state='desktop-verified'):
        path=self.root/'data/controls/wii/games'/f'{GAME}.toml'
        path.write_text(path.read_text(encoding='utf-8')+f'''\n[aim_calibration]
version="wii-aim/1"
state="{state}"
total_yaw_degrees=31.5
total_pitch_degrees=24.25
vertical_offset_cm=7
emulated_distance_m=2
native="provisional-opt-in"
[aim_calibration.evidence]
method="nine-point"
samples=9
independent_review=true
scope="approximate-desktop-dsu"
pointer_extent=0.8
input_domain=[-0.8,0.8]
target_rms_px=3
target_max_error_px=6
''',encoding='utf-8')

    def test_verified_measurement_changes_only_target_dsu_and_opt_in_native_files(self):
        before=generated_files(self.root);self.calibrated_fixture();after=generated_files(self.root)
        changed={name for name in before if before[name]!=after[name]}
        expected={f'games/{GAME}/{name}-{hand}.{ext}' for hand in ('right','left') for name,ext in [('flat-dolphin','ini'),('scripted-dsu','ini'),('scripted-dsu','json')]}
        self.assertEqual(changed,expected|{'generated-files.json'})
        self.assertEqual(set(after)-set(before),{f'games/{GAME}/dolphinxr-aim-provisional-{hand}.{ext}' for hand in ('right','left') for ext in ('ini','Dolphin.ini')})
        for hand in ('left','right'):
            ds=section(render_scripted_profile(GAME,hand,self.root))
            self.assertEqual([ds[k] for k in ('IR/Total Yaw','IR/Total Pitch','IR/Vertical Offset')],['31.5','24.25','7'])
            self.assertTrue(ds['Device'].startswith('DSUClient/0/'));self.assertIn('DSUClient/1/',ds['Nunchuk/Buttons/Z'])
            normal=section(render_game_profile(GAME,'dolphinxr',hand,self.root))
            provisional=section(render_game_profile(GAME,'dolphinxr',hand,self.root,provisional_native_aim=True))
            self.assertEqual(provisional['Source'],'3');self.assertNotIn('IR/Total Yaw',normal)
            for key,value in normal.items():self.assertEqual(provisional[key],value)

    def test_candidate_keeps_all_ini_bytes_and_has_no_native_opt_in_export(self):
        before=generated_files(self.root);self.calibrated_fixture('candidate');after=generated_files(self.root)
        for name,blob in before.items():
            if name.endswith('.ini'):self.assertEqual(after[name],blob,name)
        self.assertEqual(set(before),set(after))
        with self.assertRaisesRegex(ValueError,'independently verified'):render_game_profile(GAME,'dolphinxr',root=self.root,provisional_native_aim=True)

    def test_invalid_units_ranges_evidence_and_native_acceptance_refuse(self):
        for key,value in [('total_yaw_degrees',0),('total_pitch_degrees',361),('vertical_offset_cm',101),('total_yaw_degrees',float('nan')),('total_yaw_degrees',True),('emulated_distance_m',4),('native','headset-verified'),('state','accepted'),('version','unknown')]:
            data=deepcopy(SYNTHETIC);data[key]=value
            with self.subTest(key=key,value=value),self.assertRaises(ValueError):aim_calibration(data)
        for key,value in [('independent_review',False),('samples',8),('scope','native-headset'),('pointer_extent',1.1),('input_domain',[-1,1]),('target_rms_px',float('inf'))]:
            data=deepcopy(SYNTHETIC);data['evidence'][key]=value
            with self.subTest(key=key),self.assertRaises(ValueError):aim_calibration(data)

    def test_native_cannot_bypass_explicit_provisional_opt_in(self):
        with self.assertRaisesRegex(ValueError,'provisional opt-in'):render_profile('dolphinxr',root=self.root,calibration=SYNTHETIC)
        with self.assertRaisesRegex(ValueError,'native-only'):render_profile('flat-dolphin',root=self.root,calibration=SYNTHETIC,provisional_native_aim=True)
        candidate=deepcopy(SYNTHETIC);candidate['state']='candidate'
        with self.assertRaisesRegex(ValueError,'Candidate aim'):render_profile('flat-dolphin',root=self.root,calibration=candidate)

    def test_aim_does_not_change_hand_roles_buttons_or_motion_bindings(self):
        before={hand:section(render_scripted_profile(GAME,hand,self.root)) for hand in ('left','right')}
        self.calibrated_fixture()
        for hand in ('left','right'):
            after=section(render_scripted_profile(GAME,hand,self.root))
            for key,value in before[hand].items():
                if key not in ('IR/Total Yaw','IR/Total Pitch'):self.assertEqual(after[key],value)
            d=scripted_bridge_profile(GAME,hand,self.root)
            self.assertEqual(d['aim_calibration']['native'],'provisional-opt-in')
            self.assertEqual(d['device_slots'],{'remote':0,'nunchuk':1});self.assertFalse(d['recenter_supported'])

if __name__=='__main__':unittest.main()
