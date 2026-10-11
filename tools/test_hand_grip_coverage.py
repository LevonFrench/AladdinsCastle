"""Verify CPU review coverage and real mounting-node/handedness contracts."""
import hashlib
import itertools
import json
from pathlib import Path
import runpy
import unittest
from hand_assembly import ROOT,compile_assembly
from check_gun_assets import read_glb

class HandGripCoverageTests(unittest.TestCase):
    def test_every_proposal_and_stick_has_both_primary_hands(self):
        cases=runpy.run_path(str(ROOT/'assets/hands/review_grips.py'))['cases']()
        proposals=json.loads((ROOT/'assets/guns/preview/wii-proposal-sheet.json').read_text())
        models={p['model'] for p in proposals['games']}|{'con-stick-grip'}
        self.assertEqual({(c['model'],c['primary_hand']) for c in cases if c['hold']=='primary'},set(itertools.product(models,('left','right'))))
        self.assertEqual({c['primary_hand'] for c in cases if c['hold']=='docked'},{'left','right'})
    def test_heavy_mirror_mounts_swap_actual_handles_without_mesh_mirroring(self):
        row=compile_assembly()['models']['mnt-mg-heavy']
        self.assertEqual(row['primary_mount_by_hand']['right']['node'],'grip')
        self.assertEqual(row['primary_mount_by_hand']['left']['node'],'grip_two')
        self.assertEqual(row['hands']['left']['node'],'grip_two')
        for primary,other in (('right','left'),('left','right')):
            self.assertEqual(row['support_by_primary'][primary]['node'],row['primary_mount_by_hand'][other]['node'])
    def test_pose_and_label_nodes_are_actual_exported_nodes(self):
        for mid,row in compile_assembly()['models'].items():
            doc,_=read_glb(ROOT/'assets/guns'/f'{mid}.glb');nodes={n['name'] for n in doc['nodes']}
            for group in ('primary_mount_by_hand','support_by_primary'):
                for pose in row.get(group,{}).values():
                    self.assertIn(pose['node'],nodes);self.assertAlmostEqual(sum(q*q for q in pose['rotation_xyzw']),1,places=8)
    def test_review_manifest_matches_inputs_and_exported_image_bytes(self):
        directory=ROOT/'assets/hands/preview/grips';manifest=json.loads((directory/'coverage.json').read_text())
        cases=runpy.run_path(str(ROOT/'assets/hands/review_grips.py'))['cases']()
        self.assertEqual(manifest['cases'],cases)
        self.assertEqual({(i['case'],i['view']) for i in manifest['images']},set(itertools.product((c['id'] for c in cases),('assembly','grip'))))
        for name,digest in manifest['source_sha256'].items():self.assertEqual(hashlib.sha256((directory/name).read_bytes()).hexdigest(),digest)
if __name__=='__main__':unittest.main()
