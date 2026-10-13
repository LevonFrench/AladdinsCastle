"""Source behavior checks; no game files, renderer, runtime or input emission."""
from copy import deepcopy
import json,unittest
from control_sets import ROOT,Resolver,load,vocabulary
import system22_controls as s

class System22ControlsTests(unittest.TestCase):
    def tc(self,hand='right'):return s.resolve_game(ROOT,'timecris',hand)
    def test_d63_order_and_correct_non_gun_cabinet_types(self):
        rows=s.catalog()['games'];self.assertEqual([r['game_id'] for r in rows],['timecris','raverace','acedrive','dirtdash','tokyowar','propcycl','adillor'])
        self.assertEqual([r['control_type'] for r in rows],['gun','wheel','wheel','wheel','wheel','handlebars','trackball'])
        self.assertTrue(all(r['model']=='' and r['configured_slots']==0 and not r['two_gun_eligible'] for r in rows[1:]))
    def test_reuses_actual_tc_model_and_named_nodes(self):
        row=self.tc();self.assertEqual(row['model'],Resolver(ROOT).resolve_game('timecris')['model']);self.assertEqual(row['node_validation'],'source-built-references-checked')
        self.assertEqual({e['node'] for e in row['data']['label']},{'muzzle','pivot_trigger','fx_laser'})
    def test_two_triggers_select_same_player_and_slot_not_p2(self):
        r=self.tc();fire=[e for e in r['data']['element'] if e['input']['kind']=='gun'];self.assertEqual(len(fire),2)
        self.assertEqual({e['player'] for e in fire},{0});self.assertEqual({e['slot'] for e in fire},{0});self.assertFalse(r['two_gun_eligible']);self.assertEqual(r['configured_slots'],1)
        self.assertEqual({e['binding']['hand'] for e in fire},{'left','right'});self.assertTrue(all('exact ties' in e['source_rule'] for e in fire))
    def test_saved_layout_buttons_do_not_follow_trigger_handoff(self):
        for saved in ('left','right'):
            r=self.tc(saved)
            for active in ('left','right'):
                packet=s.hand_labels(r,active);labels={e['id']:e for e in packet['labels']}
                self.assertEqual(packet['player'],0);self.assertEqual(packet['gun_slot'],0);self.assertEqual(labels['fire']['controller_hand'],active)
                self.assertEqual(labels['laser']['controller_hand'],saved);self.assertEqual(labels['coin']['controller_hand'],saved)
                self.assertEqual(labels['recenter']['controller_hand'],'left' if saved=='right' else 'right')
                self.assertTrue(all(e['anchor']['physical_hand']==active for e in packet['labels'] if e['anchor']['kind']=='gun-node'))
    def test_left_swap_face_names_and_real_source_paths(self):
        labels={e['id']:e for e in s.hand_labels(self.tc('left'),'right')['labels']}
        self.assertEqual(labels['coin']['controller_button'],'X (left)');self.assertEqual(labels['laser']['controller_button'],'Y (left)')
        self.assertEqual(labels['recenter']['controller_button'],'A (right)');self.assertEqual(labels['laser']['source_openxr_path'],'/user/hand/left/input/y/click')
        self.assertEqual(labels['fire']['source_openxr_path'],'/user/hand/right/input/trigger/value')
    def test_either_grip_and_grip_mode_are_explicit(self):
        r=self.tc();cover=[e for e in r['data']['element'] if e['input']['semantic']=='cover_pedal'];self.assertEqual({e['binding']['hand'] for e in cover},{'left','right'})
        self.assertTrue(all('maximum of either squeeze' in e['source_rule'] for e in cover));self.assertEqual(r['data']['options']['requested_cover_mode'],'grip')
    def test_no_invented_start_offscreen_reload_or_gun_buttons(self):
        r=self.tc();self.assertFalse(any(e['input']['semantic'] in ('start','reload') for e in r['data']['element']))
        self.assertTrue(all(not e['node'] for e in r['data']['element'] if e['id'] in ('coin','recenter','cover-left','cover-right','pause')))
    def test_option_sticks_and_menu_keep_fixed_physical_hands(self):
        for primary in ('left','right'):
            entries={e['id']:e for e in self.tc(primary)['data']['element']}
            self.assertEqual(entries['pause']['binding']['source_openxr_path'],'/user/hand/left/input/menu/click')
            self.assertEqual(entries['gun-angle']['binding']['hand'],'right');self.assertEqual(entries['view-distance']['binding']['hand'],'left')
            self.assertEqual(entries['cover-mode']['when'],'paused')
    def test_racer_bindings_remain_questions_and_no_live_capability_claim(self):
        for r in s.catalog()['games']:
            self.assertEqual(r['declared_active_slots'],0);self.assertTrue(r['gaps'])
            self.assertTrue(all(e['availability']['state']=='unavailable' for e in r['data']['element']))
            if r['control_type']!='gun':self.assertTrue(all(e['binding_evidence']=='question' and not e['node'] for e in r['data']['element']))
    def test_shared_rule_keeps_game_specific_overrides_and_adc_ranges(self):
        rave=s.resolve_game(ROOT,'raverace');ace=s.resolve_game(ROOT,'acedrive');dirt=s.resolve_game(ROOT,'dirtdash')
        self.assertEqual(rave['control_set'],ace['control_set']);self.assertEqual(rave['data']['source_inputs']['steering_range'],[640,3456])
        self.assertEqual(ace['data']['source_inputs']['steering_range'],[512,3584]);self.assertNotEqual(rave['data']['source_inputs']['brake_max'],ace['data']['source_inputs']['brake_max'])
        self.assertIn('motion-stop',{e['id'] for e in dirt['data']['element']});self.assertNotIn('motion-stop',{e['id'] for e in rave['data']['element']})
    def test_tc2_stays_deferred_after_steerable_rave_and_separate_view(self):
        d=s.catalog()['deferred_tc2'];self.assertEqual(d['after'],'raverace-steerable');self.assertTrue(d['separate_views']);self.assertFalse(d['two_guns_ready'])
        with self.assertRaises(ValueError):s.resolve_game(ROOT,'timecrs2')
    def test_rejects_missing_nodes_and_nongun_model_substitution(self):
        r=self.tc();a=load(ROOT/'data/controls/system22/controls.toml')['assignments']['timecris']
        with self.assertRaisesRegex(ValueError,'missing authored node'):s.validate(r['data'],a,vocabulary(ROOT),{'muzzle'})
        a=deepcopy(a);a.update(control_type='wheel')
        with self.assertRaisesRegex(ValueError,'Gun model cannot'):s.validate(r['data'],a,vocabulary(ROOT))
    def test_rejects_live_claim_and_unknown_native_host_port(self):
        r=self.tc();a=load(ROOT/'data/controls/system22/controls.toml')['assignments']['timecris'];d=deepcopy(r['data']);d['element'][0]['availability']['state']='available'
        with self.assertRaisesRegex(ValueError,'cannot claim live'):s.validate(d,a,vocabulary(ROOT))
        d=deepcopy(r['data']);d['element'][0]['input']={'kind':'native-host','semantic':'invented'}
        with self.assertRaisesRegex(ValueError,'unknown input'):s.validate(d,a,vocabulary(ROOT))
    def test_unassigned_games_bad_hand_and_racer_gun_labels_rejected(self):
        with self.assertRaises(ValueError):s.resolve_game(ROOT,'hotd2')
        with self.assertRaises(ValueError):self.tc('either')
        with self.assertRaises(ValueError):s.hand_labels(s.resolve_game(ROOT,'tokyowar'))
    def test_explicit_route_api_preserves_generic_baseline_and_rejects_ignored_layers(self):
        base=Resolver(ROOT);native=base.resolve_game('timecris',route=s.ROUTE)
        self.assertEqual(native['data']['schema'],'system22-native/1');self.assertNotEqual(base.resolve_game('timecris')['control_set'],native['control_set'])
        self.assertEqual(base.resolve_game('raverace',route=s.ROUTE)['model'],'')
        with self.assertRaises(ValueError):base.resolve_game('timecris',route=s.ROUTE,user_override={})
        with self.assertRaises(ValueError):base.resolve_game('timecris',route='invented')
    def test_lod1_fire_label_uses_real_motion_alias_and_invalid_lod_rejected(self):
        row=self.tc();labels={e['id']:e for e in s.hand_labels(row,'left',1)['labels']}
        self.assertEqual(labels['fire']['anchor']['node'],'pivot_trigger_lod1');self.assertEqual(labels['aim']['anchor']['node'],'muzzle')
        with self.assertRaises(ValueError):s.hand_labels(row,lod=True)
    def test_compiled_catalog_is_reproducible(self):
        path=ROOT/'data/controls/system22/view-catalog.json'
        self.assertEqual(json.loads(path.read_text(encoding='utf-8')),s.catalog())

if __name__=='__main__':unittest.main()
