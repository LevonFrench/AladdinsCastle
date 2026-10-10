"""Pure-Python checks of all authored plans and motion metadata; no bpy import."""
import math
from pathlib import Path
import runpy
import sys
import unittest

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0,str(ROOT/'assets/guns/src'))
from common import TIERS, MATERIAL_RGBA, metadata, plan, preview_material_rgba


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

    def test_slide_moves_upper_geometry_and_kick_moves_whole_visible_body(self):
        furniture={'frame','hand_grip','guard_post_-1','guard_post_1','guard_lower',
                   'cable_boss','grip_rib_0','grip_rib_1','grip_rib_2'}
        upper={'barrel_shell','rear_shell','muzzle_lens','front_sight_geometry','rear_sight_geometry'}
        for mid in ('arc-pistol-slide','arc-pistol-twin'):
            spec=plan(mid,runpy.run_path(str(ROOT/'assets/guns/src'/(mid+'.py')))['PARAMETERS'])
            parts={p['name']:p for p in spec['parts']}
            with self.subTest(model=mid):
                for name in furniture:
                    self.assertEqual(parts[name]['parent'],'visual_kick' if mid=='arc-pistol-twin' else None)
                for name in upper:
                    self.assertEqual(parts[name]['parent'],'visual_kick' if mid=='arc-pistol-twin' else 'slide_recoil')
                for name in ('muzzle','fx_muzzle','fx_laser','sight_front','sight_rear'):
                    self.assertIsNone(spec['nodes'][name]['parent'])

    def test_animated_visible_meshes_do_not_claim_static_reference_names(self):
        for mid in TIERS:
            spec=plan(mid,runpy.run_path(str(ROOT/'assets/guns/src'/(mid+'.py')))['PARAMETERS'])
            driven={m['node'] for m in metadata(mid)['motion'].values()}
            with self.subTest(model=mid):
                for part in spec['parts']:
                    ancestor=part['parent']; animated=False
                    while ancestor:
                        animated |= ancestor in driven
                        ancestor=spec['nodes'][ancestor]['parent']
                    if animated:
                        # Exact grip/muzzle names plus sight_/fx_ prefixes mirror
                        # native protection. Ordinary grip_rib geometry is valid.
                        for name in (part['name'],part['name']+'_lod0',part['name']+'_lod1'):
                            self.assertNotIn(name,('grip','grip_two','muzzle'))
                            self.assertFalse(name.startswith(('sight_','fx_')),name)
                parts={p['name']:p for p in spec['parts']}
                for marker,visible in (('sight_front','front_sight_geometry'),('sight_rear','rear_sight_geometry')):
                    self.assertEqual(parts[visible]['at'],spec['nodes'][marker]['at'])
                    self.assertIsNone(spec['nodes'][marker]['parent'])

    def test_preview_multiplies_exported_linear_factors_for_all_palettes(self):
        # Exported values stay stable; native GL multiplies these factors rather
        # than replacing them. Black/white/low-sRGB cases exercise both branches.
        self.assertEqual(MATERIAL_RGBA,{'body':(.55,.55,.55,1),'accent':(.32,.32,.34,1),
                                        'dark':(.012,.012,.018,1),'glass':(.025,.10,.16,1)})
        palettes=[{'body':'#000000','accent':'#ffffff'}, {'body':'#0a0bff','accent':'#ff0b0a'}]
        for mid in TIERS:
            palettes.extend(v for v in metadata(mid)['tints'].values() if isinstance(v,dict))
        for tint in palettes:
            with self.subTest(tint=tint):
                result=preview_material_rgba(tint)
                for role in ('dark','glass'):
                    self.assertEqual(result[role],MATERIAL_RGBA[role])
                for role in ('body','accent'):
                    self.assertEqual(result[role][3],1)
                    for index in range(3):
                        s=int(tint[role][1+2*index:3+2*index],16)/255
                        expected=MATERIAL_RGBA[role][index]*(s/12.92 if s<=.04045 else ((s+.055)/1.055)**2.4)
                        self.assertAlmostEqual(result[role][index],expected)


if __name__ == '__main__':
    unittest.main()
