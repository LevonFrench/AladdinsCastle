"""Pure-Python checks of all authored plans and motion metadata; no bpy import."""
import math
from pathlib import Path
import runpy
import sys
import unittest

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0,str(ROOT/'assets/guns/src'))
from common import TIERS, metadata, plan


class SourcePlanTests(unittest.TestCase):
    def test_all_seven_plans(self):
        for mid in TIERS:
            with self.subTest(model=mid):
                parameters = runpy.run_path(str(ROOT/'assets/guns/src'/(mid+'.py')))['PARAMETERS']
                spec, meta = plan(mid,parameters),metadata(mid)
                self.assertEqual(parameters['length_mm'],meta['length_mm'])
                self.assertEqual({p['material'] for p in spec['parts']},{'body','accent','dark','glass'})
                self.assertEqual(len({p['name'] for p in spec['parts']}),len(spec['parts']))
                for node,data in spec['nodes'].items():
                    seen, parent = {node},data['parent']
                    while parent:
                        self.assertIn(parent,spec['nodes'])
                        self.assertNotIn(parent,seen)
                        seen.add(parent)
                        parent = spec['nodes'][parent]['parent']
                    self.assertTrue(all(math.isfinite(v) for v in data['at']))
                for name in ('muzzle','fx_muzzle','fx_laser','sight_front','sight_rear'):
                    self.assertIsNone(spec['nodes'][name]['parent'])
                for part in spec['parts']:
                    self.assertIn(part['shape'],('box','cylinder'))
                    self.assertIn(part['parent'],[None,*spec['nodes']])
                for motion in meta['motion'].values():
                    self.assertIn(motion['node'],spec['nodes'])
                    self.assertIn(motion['kind'],('rotate','slide'))
                    self.assertEqual(motion['lod_nodes'],[motion['node'],motion['node']+'_lod1'])
                    self.assertAlmostEqual(sum(v*v for v in motion['axis']),1)
                    self.assertLess(motion['range'][0],motion['range'][1])
                self.assertEqual({n for n in spec['nodes'] if n.startswith('button_')},
                                 {b['node'] for b in meta.get('button',[])})

    def test_mounted_grip_is_at_rear_handle(self):
        path = ROOT/'assets/guns/src/mnt-mg-heavy.py'
        spec = plan('mnt-mg-heavy',runpy.run_path(str(path))['PARAMETERS'])
        handle = next(p for p in spec['parts'] if p['name']=='rear_handle_1')
        self.assertAlmostEqual(handle['at'][0],0)
        self.assertLess(spec['nodes']['grip_two']['at'][0],-.1)
        self.assertEqual(spec['nodes']['pivot_pitch']['parent'],'pivot_yaw')
        self.assertEqual(spec['nodes']['pivot_trigger']['parent'],'pivot_pitch')


if __name__ == '__main__':
    unittest.main()
