"""Verify CPU review coverage and real mounting-node/handedness contracts."""
import hashlib
import itertools
import json
import math
from pathlib import Path
import runpy
import tomllib
import unittest
from hand_assembly import FINGERS,ROOT,compile_assembly,node_positions,trigger_blade
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

def turn(point,pivot,angle):
    """Rotate a point about the axis [-1,0,0] through pivot (the hand and trigger motion axis)."""
    c,s=math.cos(-angle),math.sin(-angle);y,z=point[1]-pivot[1],point[2]-pivot[2]
    return (point[0],pivot[1]+y*c-z*s,pivot[2]+y*s+z*c)

def pull_gap(mid,pose,pull):
    """Gap in metres from the index capsule to the trigger blade with both pulled to `pull` (0..1).

    The blade swings about the model's pivot_trigger through its motion range; the index rotates about
    its first spine point through the hand's index range. Computed from the GLB and TOML data only."""
    motion=tomllib.loads((ROOT/'data/guns'/f'{mid}.toml').read_text(encoding='utf-8'))['motion']['trigger']
    hand=tomllib.loads((ROOT/'assets/hands/grip-hand-right.toml').read_text(encoding='utf-8'))['motion']['index']
    assert motion['axis']==[-1,0,0] and hand['axis']==[-1,0,0]
    pivot=node_positions(mid)['pivot_trigger'];angle=motion['range'][0]+(motion['range'][1]-motion['range'][0])*pull
    blade={turn(p,pivot,angle) for triangle in trigger_blade(mid) for p in triangle}
    points,radii=FINGERS['index'];root=points[0]
    swung=[turn(p,root,hand['range'][0]+(hand['range'][1]-hand['range'][0])*pull) for p in points]
    return finger_gap(swung,radii,pose,1,blade)

def lightness(colour):
    """CIE L* of an sRGB hex colour."""
    lin=[(v/12.92 if v<=.04045 else ((v+.055)/1.055)**2.4) for v in (int(colour[i:i+2],16)/255 for i in (1,3,5))]
    y=.2126*lin[0]+.7152*lin[1]+.0722*lin[2]
    return 116*y**(1/3)-16 if y>216/24389 else 903.3*y
DARK_LSTAR=lightness('#1d1d24')  # the fixed `dark` role, #1d1d24 in every GLB

class HandGripCoverageTests(unittest.TestCase):
    def test_every_gun_mode_model_has_a_hand_entry(self):
        shipped={p.stem for p in (ROOT/'data/guns').glob('*.toml') if p.stem!='defaults' and (ROOT/tomllib.loads(p.read_text(encoding='utf-8'))['model']).is_file()}
        entries=set(json.loads((ROOT/'assets/hands/assembly.json').read_text(encoding='utf-8'))['models'])
        self.assertFalse(shipped-entries,'gun models with a GLB but no assembly.json entry')
        self.assertFalse(entries-shipped,'assembly.json entries without a gun GLB')
        self.assertTrue({'arc-light-gun','funcon','con-pointer-remote'}<=entries)
    def test_label_anchors_sit_on_the_exported_nodes(self):
        for mid,row in compile_assembly()['models'].items():
            found=node_positions(mid)
            for name,anchor in row['label_anchors'].items():
                with self.subTest(model=mid,anchor=name):
                    self.assertLess(max(abs(a-b) for a,b in zip(found[anchor['node']],anchor['bind_position'])),1e-3)
    def test_index_follows_the_trigger_through_the_pull_today(self):
        # Regression bound for the current rigid hand: the index leaves the blade as it is pulled
        # (studio review 2026-10-25, rigging F1). The target is test_index_stays_on_the_trigger_for_the_whole_pull.
        for mid,row in compile_assembly()['models'].items():
            if mid in ('mnt-mg-heavy','con-stick-grip','con-pointer-remote') or not trigger_blade(mid):continue
            gaps=[pull_gap(mid,row['hands']['right'],pull) for pull in (0,.5,1)]
            with self.subTest(model=mid):
                self.assertLess(gaps[0],.003)
                self.assertLess(gaps[2],.013)
    @unittest.expectedFailure
    def test_index_stays_on_the_trigger_for_the_whole_pull(self):
        # Needs the hand rebuild (static proximal index, distal part curling about Y): Blender work.
        for mid,row in compile_assembly()['models'].items():
            if mid in ('mnt-mg-heavy','con-stick-grip','con-pointer-remote') or not trigger_blade(mid):continue
            for pull in (0,.5,1):
                with self.subTest(model=mid,pull=pull):
                    self.assertLess(pull_gap(mid,row['hands']['right'],pull),.0015)
    def test_colour_contract_base_factors_and_value_contrast(self):
        for path in sorted((ROOT/'assets/guns').glob('*.glb'))+sorted((ROOT/'assets/hands').glob('*.glb')):
            doc,_=read_glb(path)
            for material in doc['materials']:
                if material['name'] in ('body','accent'):
                    with self.subTest(glb=path.name,material=material['name']):
                        self.assertEqual(material['pbrMetallicRoughness']['baseColorFactor'][:3],[1,1,1])
        for meta in [p for p in sorted((ROOT/'data/guns').glob('*.toml')) if p.stem!='defaults' and (ROOT/'assets/guns'/(p.stem+'.glb')).is_file()]+sorted((ROOT/'assets/hands').glob('grip-hand-*.toml')):
            tints=tomllib.loads(meta.read_text(encoding='utf-8'))['tints']
            for name in {tints[k] for k in ('default','p1','p2')}:
                body,accent=lightness(tints[name]['body']),lightness(tints[name]['accent'])
                with self.subTest(model=meta.stem,tint=name):
                    self.assertGreaterEqual(body,DARK_LSTAR+9);self.assertGreaterEqual(accent,DARK_LSTAR+9)
                    if meta.stem.startswith('grip-hand'):continue
                    self.assertGreaterEqual(abs(body-accent),15)
                    self.assertGreaterEqual(accent-DARK_LSTAR,12)

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
