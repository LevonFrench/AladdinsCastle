"""Actual exported shell/controller checks and source hold/motion invariants."""
import sys
import unittest
from copy import deepcopy
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1];sys.path.insert(0,str(ROOT/'assets/guns/src'))
from common import plan,metadata
from check_gun_assets import check_asset,check_previews
class ShellModelsTests(unittest.TestCase):
    def test_original_hand_exports_and_assembly_references(self):
        import json,math
        from check_hand_assets import check
        from hand_assembly import compile_assembly
        from check_gun_assets import read_glb
        self.assertEqual(set(check()),{'right','left'})
        assembly=compile_assembly();self.assertEqual(assembly,json.loads((ROOT/'assets/hands/assembly.json').read_text()))
        self.assertEqual(assembly['quaternion_order'],'xyzw')
        for mid,row in assembly['models'].items():
            doc,_=read_glb(ROOT/metadata(mid)['model']);nodes={n['name'] for n in doc['nodes']}
            for anchor in row['label_anchors'].values():self.assertIn(anchor['node'],nodes)
            for hand,pose in row['hands'].items():
                self.assertTrue((ROOT/pose['asset']).is_file());self.assertIn(pose['node'],nodes)
                self.assertAlmostEqual(sum(v*v for v in pose['rotation_xyzw']),1)
                self.assertTrue(all(math.isfinite(v) for v in pose['translation']))
    def test_hand_anatomy_is_mirrored_without_negative_scale(self):
        import runpy
        module=runpy.run_path(str(ROOT/'assets/hands/src/grip_hand.py'))
        right=module['make_design']('right').result('right');left=module['make_design']('left').result('left')
        for a,b in zip(right['parts'],left['parts']):
            if a['shape']=='sweep':
                for p,q in zip(a['points'],b['points']):self.assertEqual((-p[0],p[1],p[2]),q)
    def test_both_models_have_valid_exported_lods_materials_and_callouts(self):
        for mid in ('con-remote-shell','con-stick-grip'):
            meta=metadata(mid);result=check_asset(ROOT/meta['model'],meta)
            self.assertLessEqual(result['triangles']['LOD0'],8000);self.assertLessEqual(result['triangles']['LOD1'],2000)
            check_previews(ROOT/'assets/guns/preview',mid,meta)
    def test_shell_sights_and_rear_hold_are_static_forward_trigger_grip(self):
        spec=plan('con-remote-shell',None);meta=metadata('con-remote-shell')
        self.assertEqual(meta['assembly']['default_hold'],'split');self.assertEqual(meta['assembly']['dock_node'],'grip_two')
        self.assertGreater(spec['nodes']['grip_two']['at'][2],.10)
        self.assertLess(spec['nodes']['pivot_trigger']['at'][2],0)
        self.assertEqual(spec['nodes']['sight_front']['at'][:2],spec['nodes']['sight_rear']['at'][:2])
        self.assertEqual({b['id'] for b in meta['button']},{'a','dpad','plus','minus','one','two','home'})
    def test_controller_has_no_gun_anchors_and_actual_two_axis_stick(self):
        spec=plan('con-stick-grip',None);meta=metadata('con-stick-grip')
        self.assertFalse({'muzzle','fx_laser','sight_front'}&set(spec['nodes']))
        self.assertEqual(spec['nodes']['button_stick']['parent'],'pivot_stick_y')
        self.assertEqual(spec['nodes']['pivot_stick_y']['parent'],'pivot_stick_x')
        self.assertEqual(meta['motion']['stick-x']['drive'],'stick_x');self.assertEqual(meta['motion']['stick-y']['drive'],'stick_y')
        # A controller cannot be admitted as a gun by metadata relabelling.
        invalid=deepcopy(meta);invalid['asset_kind']='gun'
        with self.assertRaisesRegex(ValueError,'missing nodes'):check_asset(ROOT/meta['model'],invalid)
if __name__=='__main__':unittest.main()
