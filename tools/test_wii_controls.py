"""Shared Wii route/assembly regressions; no games, backend or input emission."""
from copy import deepcopy
import json
import unittest
from wii_controls import ROOT,compile_library,resolve_game
from control_sets import Resolver

class WiiControlsTests(unittest.TestCase):
    def test_compiled_catalog_matches_sources(self):
        self.assertEqual(json.loads((ROOT/'data/controls/wii/view-catalog.json').read_text(encoding='utf-8')),compile_library())
        from pathlib import Path
        self.assertEqual({r['game_id'] for r in compile_library()['games']},{p.stem for p in (ROOT/'data/controls/wii/games').glob('*.toml')})
    def test_each_game_has_both_route_variants_from_same_actions(self):
        for game in compile_library()['games']:
            self.assertEqual(set(game['route_variants']),{'flat-dolphin','dolphinxr'})
            for route in game['route_variants']:
                row=resolve_game(ROOT,game['game_id'],route=route)
                self.assertEqual(row['data']['route'],route);self.assertEqual(row['model'],'con-remote-shell')
                self.assertEqual(row['data']['assembly']['accessory'],'con-stick-grip')
                ids={e['id'] for e in row['data']['element']}
                self.assertTrue({'p1-home','p1-fire','p1-c','p1-z','p1-remote-shake','p1-stick-shake'}<=ids)
    def test_unknown_layouts_and_actions_are_questions(self):
        for game in compile_library()['games']:
            data=game['data'];self.assertEqual(data['layout']['state'],'question')
            for row in data['element']:
                if row['id'].endswith('-aim'):continue
                self.assertEqual(row['game_action_state'],'question');self.assertIn('Question:',row['label'])
    def test_button_nodes_belong_to_correct_model(self):
        game=resolve_game(ROOT,'wii-resident-evil-umbrella-chronicles');models={game['model']:game['model_metadata'],game['accessory_metadata']['id']:game['accessory_metadata']}
        for row in game['data']['element']:
            if row['node'].startswith('button_'):
                self.assertIn(row['node'],{b['node'] for b in models[row['model']]['button']})
    def test_native_two_players_never_claim_independent_aim(self):
        data=resolve_game(ROOT,'wii-resident-evil-umbrella-chronicles',route='dolphinxr')['data']
        self.assertFalse(data['policy']['native_two_remotes']);self.assertEqual(data['policy']['native_hand_config'],'Android/QuestLeftHanded')
    def test_split_z_is_trigger_and_c_is_grip_with_dual_same_hand_policy(self):
        rows={r['id']:r for r in resolve_game(ROOT,'wii-resident-evil-umbrella-chronicles')['data']['element']}
        self.assertEqual(rows['p1-z']['binding']['control'],'trigger');self.assertEqual(rows['p1-c']['binding']['control'],'grip')
        self.assertEqual(rows['p1-z']['binding']['physical_dsu_slot_by_mode'],{'one_gun':1,'two_remotes':0})
        self.assertEqual(rows['p2-z']['binding']['physical_dsu_slot_by_mode'],{'one_gun':1,'two_remotes':1})
    def test_primary_hand_swap_and_fail_closed_route_selection(self):
        gid='wii-resident-evil-umbrella-chronicles'
        data=resolve_game(ROOT,gid,'left',route='dolphinxr')['data'];self.assertEqual(data['policy']['p1_hand'],'left')
        self.assertEqual(data['policy']['supported_primary_hands'],['right','left'])
        with self.assertRaisesRegex(ValueError,'route'):resolve_game(ROOT,gid,route='unknown')
        with self.assertRaisesRegex(ValueError,'primary hand'):resolve_game(ROOT,gid,'neither')
    def test_baseline_resolver_uses_shell_without_arcade_coin(self):
        row=Resolver().resolve_game('wii-resident-evil-umbrella-chronicles')
        self.assertEqual(row['model'],'con-remote-shell');self.assertFalse(any('coin' in e['id'] for e in row['data']['element']))
        for e in row['data']['element']:
            if e['target'] in ('pointer','accelerometer_gyro','nunchuk_acceleration'):continue
            self.assertEqual(e['availability']['state'],'planned')
        with self.assertRaisesRegex(ValueError,'unsupported layers'):Resolver().resolve_game(row['game_id'],user_override={'policy':{'p1_hand':'left'}})
    def test_variants_cannot_repair_missing_or_mismatched_baseline_route(self):
        from pathlib import Path
        from tempfile import TemporaryDirectory
        gid='wii-resident-evil-umbrella-chronicles'
        valid=compile_library()
        fixtures=ROOT/'.local/wii-route-tests';fixtures.mkdir(parents=True,exist_ok=True)
        for declared in (None,'dolphinxr','future-route'):
            with self.subTest(declared=declared),TemporaryDirectory(dir=fixtures) as temp:
                catalog=deepcopy(valid)
                for row in catalog['games']:
                    if row['game_id']!=gid:continue
                    if declared is None:row['data'].pop('route')
                    else:row['data']['route']=declared
                output=Path(temp)/'data/controls/wii/view-catalog.json'
                output.parent.mkdir(parents=True);output.write_text(json.dumps(catalog),encoding='utf-8')
                for selected in ('flat-dolphin','dolphinxr'):
                    with self.assertRaisesRegex(ValueError,'baseline route'):resolve_game(temp,gid,route=selected)
if __name__=='__main__':unittest.main()
