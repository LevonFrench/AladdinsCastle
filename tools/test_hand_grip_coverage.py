"""Verify CPU review coverage and real mounting-node/handedness contracts."""
import hashlib
import itertools
import json
from pathlib import Path
import runpy
import unittest
from hand_assembly import FINGERS,ROOT,compile_assembly,trigger_blade
from check_gun_assets import read_glb

def finger_gap(points,radii,pose,sign,targets):
    """Smallest gap in metres from a posed finger's swept surface to target points."""
    x,y,z,w=pose['rotation_xyzw']
    def place(p):
        p=[sign*p[0]/1000,p[1]/1000,p[2]/1000];t=[2*(y*p[2]-z*p[1]),2*(z*p[0]-x*p[2]),2*(x*p[1]-y*p[0])]
        return [p[0]+w*t[0]+y*t[2]-z*t[1]+pose['translation'][0],p[1]+w*t[1]+z*t[0]-x*t[2]+pose['translation'][1],p[2]+w*t[2]+x*t[1]-y*t[0]+pose['translation'][2]]
    spine=[place(p) for p in points];best=9.0
    for (a,b),(ra,rb) in zip(zip(spine,spine[1:]),zip(radii,radii[1:])):
        d=[q-p for p,q in zip(a,b)];dd=sum(v*v for v in d)
        for q in targets:
            s=max(0,min(1,sum((qv-av)*dv for qv,av,dv in zip(q,a,d))/dd))
            best=min(best,sum((qv-av-s*dv)**2 for qv,av,dv in zip(q,a,d))**.5-(ra+(rb-ra)*s)/1000)
    return best

class HandGripCoverageTests(unittest.TestCase):
    def test_index_finger_rests_on_every_pistol_trigger(self):
        for mid,row in compile_assembly()['models'].items():
            blade={tuple(p) for t in trigger_blade(mid) for p in t}
            if mid in ('mnt-mg-heavy','con-stick-grip') or not blade:continue
            for hand,pose in row['hands'].items():
                gaps={name:finger_gap(*FINGERS[name],pose,1 if hand=='right' else -1,blade) for name in FINGERS}
                with self.subTest(model=mid,hand=hand):
                    self.assertEqual(min(gaps,key=gaps.get),'index')
                    self.assertLess(gaps['index'],.003)
                    self.assertGreater(gaps['middle'],.008)
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
