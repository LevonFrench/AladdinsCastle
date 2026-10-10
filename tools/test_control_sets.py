"""Synthetic v0.1 resolution/schema tests. No input emission or GPU use."""
from copy import deepcopy
import sys
sys.dont_write_bytecode = True
import unittest

from control_sets import Resolver, ROOT, load, merge, validate_set


class ControlSetTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.resolver = Resolver()

    def generic(self):
        data = deepcopy(self.resolver.sets['gun-generic-pistol'])
        return data,self.resolver.models['generic-pistol']

    def test_all_catalog_games_resolve_without_claiming_acceptance(self):
        rows = [self.resolver.resolve_game(gid) for gid,g in self.resolver.games.items() if g.get('genre')=='gun']
        self.assertEqual(len(rows),152)
        self.assertEqual(sum(r['two_gun_eligible'] for r in rows),94)
        self.assertEqual(sum(r['separate_views'] for r in rows),6)
        self.assertTrue(all(r['gaps'] and r['declared_active_slots']==0 for r in rows))
        self.assertTrue(all(e['availability']['state']=='unverified' for r in rows for e in r['data']['element']))

    def test_unknown_extensions_and_keyed_array_merge(self):
        base = {'extension':{'x':1,'erase':2},'element':[{'id':'fire','binding':{'control':'trigger','hand':'slot'},'future':9}]}
        later = {'extension':{'erase':'!delete','y':3},'element':[{'id':'fire','binding':{'mode':'hold'}},{'id':'coin','future':5}]}
        result = merge(base,later)
        self.assertEqual(result['extension'],{'x':1,'y':3})
        self.assertEqual(result['element'][0]['binding'],{'control':'trigger','hand':'slot','mode':'hold'})
        self.assertEqual(result['element'][0]['future'],9)
        self.assertEqual(result['element'][1]['id'],'coin')
        self.assertEqual(base['extension']['erase'],2)
        self.assertEqual(merge([1,2],[3]),[3])
        self.assertEqual(merge(base['element'],[]),[])
        with self.assertRaisesRegex(ValueError,'duplicate'):
            merge(base['element'],[{'id':'fire'},{'id':'fire'}])

    def test_rejects_known_field_errors(self):
        changes = [lambda d:d.update(version='9'),lambda d:d['element'].append(deepcopy(d['element'][0])),
                   lambda d:d['element'][0].update(slot=True),lambda d:d['element'][0].update(slot=9),
                   lambda d:d['element'][0]['input'].update(semantic='invented_port'),
                   lambda d:d['element'][0]['binding'].update(control='system_button'),
                   lambda d:d['element'][0]['binding'].update(mode='long_press'),
                   lambda d:d['element'][0]['binding'].update(threshold=float('nan')),
                   lambda d:d['element'][0]['binding'].update(invert='yes')]
        for change in changes:
            data,model = self.generic()
            change(data)
            with self.subTest(change=change),self.assertRaises(ValueError):
                validate_set(data,self.resolver.vocab,model)

    def test_collision_detects_different_modes_and_either_hand(self):
        data,model = self.generic()
        data['element'][2]['binding'] = {'hand':'slot','control':'trigger','mode':'press'}
        with self.assertRaisesRegex(ValueError,'collision'):
            validate_set(data,self.resolver.vocab,model)
        data,model = self.generic()
        data['element'][0]['binding']['hand'] = 'either'
        with self.assertRaises(ValueError):
            validate_set(data,self.resolver.vocab,model)

    def test_pump_fallback_is_seated_and_slot_local(self):
        row = self.resolver.resolve_game('hotd3')
        reloads = [e for e in row['data']['element'] if e['input']['semantic']=='reload']
        self.assertEqual(len(reloads),2)
        for element in reloads:
            self.assertEqual(element['binding']['control'],'pump')
            self.assertEqual(element['fallback_binding'],{'hand':'slot','control':'flick_up','mode':'press'})
        data = deepcopy(row['data'])
        data['element'][1]['fallback_binding']['control'] = 'secondary'
        with self.assertRaisesRegex(ValueError,'fallback collision'):
            validate_set(data,self.resolver.vocab,self.resolver.models[row['model']])

    def test_actual_node_checks_are_conditional(self):
        data,model = self.generic()
        validate_set(data,self.resolver.vocab,model,nodes=None)
        with self.assertRaisesRegex(ValueError,'missing built node'):
            validate_set(data,self.resolver.vocab,model,nodes={'muzzle','fx_laser'})
        validate_set(data,self.resolver.vocab,model,nodes={'pivot_trigger','muzzle','fx_laser'})

    def test_candidate_time_crisis_has_no_start_and_is_never_implicit(self):
        default = self.resolver.resolve_game('timecris')
        self.assertFalse(any(e['input']['semantic']=='cover_pedal' for e in default['data']['element']))
        proposal = load(ROOT/'data/controls/proposed-game-overrides/timecris.toml')
        candidate = self.resolver.resolve_game('timecris',proposed_override=proposal)
        self.assertTrue(any(e['input']['semantic']=='cover_pedal' for e in candidate['data']['element']))
        self.assertFalse(any(e['input']['semantic']=='start' for e in candidate['data']['element']))
        self.assertEqual(candidate['configured_slots'],1)
        self.assertIn('EXPLICIT SOURCE-ONLY proposed game override',candidate['layers'])

    def test_backend_declaration_gates_inputs_and_two_guns(self):
        declaration = {'guns':2,'players':2,'shared_view':False,'runtime_actions':['laser_toggle'],
                       'control':[{'kind':'gun','semantic':s,'player':p} for p in (0,1) for s in ('trigger','reload')]}
        row = self.resolver.resolve_game('hotd2',backend=declaration)
        self.assertTrue(row['two_gun_eligible'])
        self.assertEqual(row['configured_slots'],2)
        self.assertEqual(row['declared_active_slots'],1)
        self.assertTrue(all(e['availability']['state']=='unavailable' for e in row['data']['element'] if e['slot']==1))
        self.assertEqual(next(e for e in row['data']['element'] if e['id']=='p1-coin')['availability']['state'],'unavailable')
        declaration['shared_view']=True
        self.assertEqual(self.resolver.resolve_game('hotd2',backend=declaration)['declared_active_slots'],2)
        declaration['guns']=True
        with self.assertRaises(ValueError):
            self.resolver.resolve_game('hotd2',backend=declaration)

    def test_declared_missing_start_is_reported_not_invented(self):
        backend={'guns':1,'players':1,'shared_view':True,'control':[{'kind':'button','semantic':'start','player':0}]}
        row = self.resolver.resolve_game('timecris',backend=backend)
        self.assertTrue(any(g['category']=='declared-input' and g['id']=='p1-start' for g in row['gaps']))
        self.assertFalse(any(e['input']['semantic']=='start' for e in row['data']['element']))

    def test_missing_model_button_and_inactive_malformed_rows_fail(self):
        data = deepcopy(self.resolver.sets['gun-con-pistol-slim'])
        data['unmapped_part'] = []
        with self.assertRaisesRegex(ValueError,'model button lacks'):
            validate_set(data,self.resolver.vocab,self.resolver.models['con-pistol-slim'])
        with self.assertRaises(ValueError):
            self.resolver.resolve_game('timecris',user_override={'element':[{'id':'p2-trigger','slot':9}]})


if __name__ == '__main__':
    unittest.main()
