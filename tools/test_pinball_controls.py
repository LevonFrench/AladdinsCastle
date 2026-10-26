import json
import unittest
from pinball_controls import ROOT,resolve_route

class PinballControlsTests(unittest.TestCase):
    def test_generated_view_current_and_no_runtime_capability(self):
        row=resolve_route();self.assertEqual(row,json.loads((ROOT/'data/controls/pinball/view.json').read_text()))
        actions={a['id']:a for a in row['data']['element']}
        self.assertEqual(actions['recenter']['availability']['state'],'planned')
        self.assertTrue(actions['exit']['warning']);self.assertIn('Quit and save',actions['exit']['label'])
        # Universal layer: stick clicks only; no menu button and no Y binding anywhere.
        self.assertEqual(actions['start']['binding'],{'hand':'left','control':'click'})
        self.assertEqual(actions['coin']['binding'],{'hand':'right','control':'click'})
        self.assertEqual(actions['exit']['binding'],{'hand':'left','control':'click'})
        self.assertEqual(actions['recenter']['input']['semantic'],'VRControllerCalibration')
        self.assertEqual(actions['table-adjust']['input']['semantic'],'VRControllerCalibration')
        for a in actions.values():
            self.assertNotIn(a['binding']['control'],('menu','system'))
            self.assertNotEqual((a['binding']['hand'],a['binding']['control']),('left','secondary'))
        self.assertEqual({a['availability']['state'] for a in actions.values()},{'planned'})
        for hand in ('left','right'):
            self.assertEqual(actions[hand+'-flipper']['threshold'],.3)
            self.assertEqual(actions[hand+'-flipper']['staged_threshold'],.6)
        self.assertEqual(actions['plunger']['scale'],-1)
    def test_unknown_route_rejected(self):
        with self.assertRaises(ValueError):resolve_route('dolphinxr')
if __name__=='__main__':unittest.main()
