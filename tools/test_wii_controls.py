"""Wii data/transport regressions; no emulator, game files or input emission."""
from copy import deepcopy
import json
import unittest
from wii_controls import ROOT,compile_library,resolve_game
from control_sets import Resolver


class WiiControlsTests(unittest.TestCase):
    def test_compiled_catalog_matches_sources(self):
        expected=compile_library()
        actual=json.loads((ROOT/'data/controls/wii/view-catalog.json').read_text())
        self.assertEqual(actual,expected)
        self.assertEqual(len(expected['games']),5)

    def test_pointer_stick_and_real_motion_are_separate(self):
        for game in compile_library()['games']:
            transport=game['data']['transport']
            self.assertEqual(transport['pointer'],'right_stick')
            self.assertEqual(transport['nunchuk_stick'],'left_stick')
            self.assertTrue(transport['motion_input'])
            self.assertFalse(transport['shake_binding'] or transport['swing_binding'])
            self.assertEqual(transport['acceleration_units'],'g')
            self.assertEqual(transport['gyro_units'],'degrees_per_second')

    def test_coop_is_generated_as_independent_mirrored_slots(self):
        game=resolve_game(ROOT,'wii-resident-evil-umbrella-chronicles')
        a=[e for e in game['data']['element'] if e['slot']==0]
        b=[e for e in game['data']['element'] if e['slot']==1]
        self.assertEqual(len(a),len(b))
        for x,y in zip(a,b):
            self.assertNotEqual(x['id'],y['id'])
            self.assertEqual(x['target'],y['target'])
            self.assertEqual(x['binding'],y['binding'])
        a[0]['label']='Mutation'
        self.assertNotEqual(a[0]['label'],b[0]['label'])

    def test_defaults_and_fallbacks_have_distinct_routes(self):
        game=resolve_game(ROOT,'wii-resident-evil-umbrella-chronicles')
        rows={e['id']:e for e in game['data']['element']}
        for action,control,target in [('fire','trigger','r2'),('interact','primary','cross'),
                ('z','grip','l2'),('c','secondary','circle'),('menu','menu','options'),
                ('laser','thumbstick_click','laser_toggle'),('reload-fallback','reload_fallback','inject_remote_shake'),
                ('melee-fallback','melee_fallback','inject_remote_swing')]:
            row=rows['p1-'+action]
            self.assertEqual((row['binding']['control'],row['target']),(control,target))
        self.assertEqual(rows['p1-nunchuk-motion']['role'],'nunchuk')
        self.assertEqual(rows['p1-motion']['role'],'remote')

    def test_custom_character_setup_is_not_a_generic_reload_set(self):
        game=resolve_game(ROOT,'wii-sin-and-punishment-star-successor')
        ids={r['id'] for r in game['data']['element']}
        self.assertTrue({'p1-move','p1-aim','p1-dodge','p1-melee','p1-lock-on','p1-charged-shot'}<=ids)
        self.assertNotIn('p1-reload',ids)
        self.assertTrue(game['data']['custom_setup'])
        self.assertEqual(game['configured_slots'],1)

    def test_baseline_resolver_routes_wii_without_arcade_coin(self):
        game=Resolver().resolve_game('wii-resident-evil-umbrella-chronicles')
        self.assertEqual(game['model'],'generic-pistol')
        self.assertEqual(game['data']['family'],'wii')
        self.assertFalse(any('coin' in e['id'] for e in game['data']['element']))
        mirrored=resolve_game(ROOT,game['game_id'],'left')
        self.assertEqual(mirrored['data']['policy']['p1_hand'],'left')
        with self.assertRaisesRegex(ValueError,'primary hand'): resolve_game(ROOT,game['game_id'],'neither')
        with self.assertRaisesRegex(ValueError,'unsupported layers'):
            Resolver().resolve_game(game['game_id'],user_override={'policy':{'p1_hand':'left'}})


if __name__=='__main__': unittest.main()
