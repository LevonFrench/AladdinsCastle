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

    def test_dsu_controls_are_keyed_to_the_flat_dolphin_route(self):
        for game in compile_library()['games']:
            with self.subTest(game=game['game_id']):
                self.assertEqual(game['data']['route'],'flat-dolphin')
                resolved=Resolver().resolve_game(game['game_id'])
                self.assertEqual(resolved['data']['route'],'flat-dolphin')
                self.assertNotEqual(resolved['data']['route'],'dolphinxr')

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
            self.assertEqual({k:v for k,v in x['binding'].items() if k!='physical_dsu_slot_by_mode'},
                             {k:v for k,v in y['binding'].items() if k!='physical_dsu_slot_by_mode'})
        a[0]['label']='Mutation'
        self.assertNotEqual(a[0]['label'],b[0]['label'])

    def test_physical_devices_follow_mode_without_changing_player_slots(self):
        game=resolve_game(ROOT,'wii-resident-evil-umbrella-chronicles')
        rows={r['id']:r for r in game['data']['element']}
        expected={'p1-fire':(0,0), 'p1-z':(1,0), 'p1-stick':(1,0),
                  'p2-fire':(0,1), 'p2-z':(1,1), 'p2-stick':(1,1)}
        for rid,(single,coop) in expected.items():
            with self.subTest(row=rid):
                self.assertEqual(rows[rid]['binding']['physical_dsu_slot_by_mode'],
                                 {'one_gun':single,'two_remotes':coop})
                self.assertEqual(rows[rid]['slot'],0 if rid.startswith('p1-') else 1)
        self.assertEqual(game['data']['policy']['nunchuk_hand_by_mode'],
                         {'one_gun':'other','two_remotes':'same'})
        self.assertNotIn('nunchuk_hand',game['data']['policy'])
        self.assertEqual(game['data']['policy']['supported_primary_hands'],['right'])

    def test_missing_producer_profile_and_laser_paths_are_planned(self):
        for game in compile_library()['games']:
            rows={r['id']:r for r in game['data']['element']}
            if game['data']['custom_setup']: continue
            for part in ('dpad','minus','one','two','laser'):
                with self.subTest(game=game['game_id'],part=part):
                    availability=rows['p1-'+part]['availability']
                    self.assertEqual(availability['state'],'planned')
                    self.assertTrue(availability['reason'])
            self.assertEqual(rows['p1-laser']['binding']['physical_dsu_slot_by_mode'],
                             {'one_gun':None,'two_remotes':None})
            self.assertEqual(rows['p1-fire']['availability']['state'],'configured')
            self.assertEqual(rows['p1-menu']['availability']['state'],'unavailable')
            self.assertIn('Binding pending',rows['p1-menu']['binding_label'])
            self.assertIn('right system button',rows['p1-menu']['availability']['reason'])

    def test_keyboard_motion_fallbacks_do_not_claim_player_two(self):
        game=resolve_game(ROOT,'wii-resident-evil-umbrella-chronicles')
        rows={r['id']:r for r in game['data']['element']}
        for part in ('reload-fallback','melee-fallback'):
            a,b=rows['p1-'+part],rows['p2-'+part]
            self.assertEqual(a['availability']['state'],'configured')
            self.assertEqual(b['availability']['state'],'unavailable')
            self.assertEqual(a['fallback_player'],0)
            self.assertEqual(b['fallback_player'],0)
            self.assertEqual(b['binding']['physical_dsu_slot_by_mode'],{'one_gun':0,'two_remotes':0})
            self.assertIn('player 1 only',b['binding_label'])
        self.assertEqual(rows['p2-motion']['availability']['state'],'configured')
        self.assertEqual(rows['p2-motion']['binding']['physical_dsu_slot_by_mode']['two_remotes'],1)

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
