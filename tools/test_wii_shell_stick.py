"""Canonical control policy/profile regressions: synthetic inputs, no emulator."""
import configparser
import itertools
import unittest
from wii_shell_stick import source,render_profile,bridge_profile,evaluate,expression,write_generated,MODES,ROOT

def profile(route,mode,hand):
    p=configparser.ConfigParser(interpolation=None);p.optionxform=str
    p.read_string(render_profile(route,mode,hand).decode());return p

class ShellProfilesTests(unittest.TestCase):
    def test_canonical_installer_seed_bytes_keep_their_pinned_hashes(self):
        import hashlib,tomllib
        found=[]
        def visit(value):
            if isinstance(value,dict):
                if str(value.get('source','')).startswith('data/controls/wii/generated/'):
                    found.append(value)
                for child in value.values():visit(child)
            elif isinstance(value,list):
                for child in value:visit(child)
        for path in (ROOT/'games').glob('*/install.toml'):
            visit(tomllib.loads(path.read_text(encoding='utf-8')))
        self.assertTrue(found)
        for step in found:self.assertEqual(hashlib.sha256((ROOT/step['source']).read_bytes()).hexdigest(),step['source_sha256'],step['source'])
    def test_game_default_profiles_and_scripted_roles_remain_one_source(self):
        from wii_shell_stick import game_policy,render_game_profile,render_scripted_profile,scripted_bridge_profile
        for path in (ROOT/'data/controls/wii/games').glob('*.toml'):
            policy=game_policy(path.stem)
            for hand in ('right','left'):
                for route in ('flat-dolphin','dolphinxr'):
                    self.assertEqual(render_game_profile(path.stem,route,hand),render_profile(route,policy['hold']['default'],hand))
                self.assertEqual(render_scripted_profile(path.stem,hand),render_profile('flat-dolphin',policy['hold']['default'],'right'))
                descriptor=scripted_bridge_profile(path.stem,hand)
                self.assertEqual(descriptor['device_slots'],{'remote':0,'nunchuk':1})
                self.assertEqual(descriptor['players'][0]['remote_hand'],hand)
                self.assertTrue(all(a['slot']==(0 if a['role']=='remote' else 1) for a in descriptor['players'][0]['actions']))
                self.assertFalse(descriptor['reload']['enabled'])
    def test_product_identity_generates_server_and_device_paths(self):
        import json
        from wii_shell_stick import generated_files
        name=json.loads((ROOT/'data/product.json').read_text())['dsu_server_name']
        self.assertEqual(source()['dsu']['server'],name)
        self.assertIn(('Entries = '+name+':127.0.0.1:26760').encode(),generated_files()['scripted-dsu/DSUClient.ini'])
        self.assertEqual(profile('flat-dolphin','split','right')['Wiimote1']['Device'],'DSUClient/0/'+name)
    def test_generated_bytes_are_current(self): write_generated(check=True)
    def test_native_device_and_every_button_survive_all_modes_and_swaps(self):
        for mode,hand in itertools.product(MODES,('right','left')):
            if mode=='two-remotes':
                with self.assertRaisesRegex(ValueError,'global'):render_profile('dolphinxr',mode,hand)
                continue
            p=profile('dolphinxr',mode,hand)
            count=2 if mode=='two-remotes' else 1
            for slot in range(count):
                r=p['Wiimote'+str(slot+1)]
                self.assertEqual(r['Source'],'3');self.assertEqual(r['Device'],'OpenXR/0/OpenXR Controller')
                for key in ('A','B','1','2','-','+','Home'):self.assertTrue(r['Buttons/'+key])
                for d in ('Up','Down','Left','Right'):self.assertTrue(r['D-Pad/'+d])
                for key in ('C','Z'):self.assertTrue(r['Nunchuk/Buttons/'+key])
                for a in source()['action']:self.assertIn('!(`Right Button Thumbstick` & `Left Button Thumbstick`)',r[a['ini']])
            self.assertEqual(p['Wiimote'+str(count+1)]['Source'],'0')
    def test_split_uses_correct_hands_trigger_z_and_grip_c(self):
        for hand in ('right','left'):
            r=profile('dolphinxr','split',hand)['Wiimote1'];other='Left' if hand=='right' else 'Right'
            self.assertIn(hand.title()+' Trigger',r['Buttons/B'])
            self.assertIn(other+' Trigger',r['Nunchuk/Buttons/Z'])
            self.assertIn(other+' Squeeze',r['Nunchuk/Buttons/C'])
            self.assertIn(other+' Thumbstick',r['Nunchuk/Stick/Up'])
    def test_modifiers_suppress_base_action(self):
        actions={a['id']:a for a in source()['action']}
        for modifier in (False,True):
            inputs={'primary':1,'grip':int(modifier)}
            self.assertEqual(evaluate(actions['interact']['split'],inputs),not modifier)
            self.assertEqual(evaluate(actions['menu']['split'],inputs),modifier)
        inputs={'primary':1,'click':1,'up':1}
        self.assertEqual(evaluate(actions['interact']['dual'],inputs),0)
        self.assertEqual(evaluate(actions['menu']['dual'],inputs),0)
        self.assertEqual(evaluate(actions['one']['dual'],inputs),1)
        self.assertEqual(evaluate(actions['dpad-up']['dual'],inputs),0)
    def test_motion_physical_and_button_fallback_coexist(self):
        for route in ('dolphinxr','flat-dolphin'):
            r=profile(route,'split','right')['Wiimote1']
            self.assertTrue(r['IMUAccelerometer/Up']);self.assertTrue(r['IMUGyroscope/Pitch Up'])
            self.assertTrue(r['Nunchuk/IMUAccelerometer/Up'])
            for k in ('Shake/X','Swing/Forward','Nunchuk/Shake/X','Nunchuk/Swing/Forward'):self.assertTrue(r[k])
    def test_bridge_and_emulator_profiles_share_targets_and_actions(self):
        for mode,hand in itertools.product(MODES,('right','left')):
            b=bridge_profile(mode,hand);p=profile('flat-dolphin',mode,hand)
            for slot,player in enumerate(b['players']):
                for action,definition in zip(player['actions'],source()['action']):
                    self.assertEqual(action['expression'],expression(definition,mode))
                    self.assertIn(action['wire'],p['Wiimote'+str(slot+1)][definition['ini']])
                if mode=='two-remotes':self.assertEqual(player['remote_hand'],player['stick_hand'])
            self.assertTrue(b['recenter_suppresses_game_buttons'])
            self.assertEqual(b['recenter_chord'],['right.click','left.click'])
    def test_invalid_selection_rejected(self):
        for kwargs in ({'route':'other'},{'mode':'unknown'},{'primary_hand':'bad'}):
            with self.assertRaises(ValueError):render_profile(**kwargs)
    def test_native_left_hand_requires_matching_global_aim_companion(self):
        from wii_shell_stick import generated_files
        outputs=generated_files()
        self.assertIn(b'QuestLeftHanded = True',outputs['dolphinxr/split-left.Dolphin.ini'])
        self.assertIn(b'QuestLeftHanded = False',outputs['dolphinxr/split-right.Dolphin.ini'])
        self.assertNotIn('dolphinxr/two-remotes-right.ini',outputs)

if __name__=='__main__':unittest.main()
