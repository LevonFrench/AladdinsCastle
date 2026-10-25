"""Pure-Python checks of all authored plans and motion metadata; no bpy import."""
import math
from pathlib import Path
import runpy
import sys
import unittest

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0,str(ROOT/'assets/guns/src'))
from common import (TIERS, BUILT_MODELS, BUILDABLE_MODELS, LEGACY_BUILD_MODELS,
                    REDESIGN_PLAN_MODELS, MATERIAL_RGBA, metadata, plan,
                    preview_material_rgba, geometry, runtime_preview_rgba,
                    RUNTIME_LIGHTING)


class SourcePlanTests(unittest.TestCase):
    def test_redesign_benchmark_input_is_independent_of_output_root(self):
        import hashlib,json
        from tempfile import TemporaryDirectory
        builder=runpy.run_path(str(ROOT/'assets/guns/src-redesign/build_render.py'))
        scratch=ROOT/'.local/gun-source-fixtures'
        scratch.mkdir(parents=True,exist_ok=True)
        with TemporaryDirectory(dir=scratch) as temp:
            directory=Path(temp);source=directory/'reference.glb';source.write_bytes(b'fixture')
            manifest=directory/'review.json'
            manifest.write_text(json.dumps({'records':[{'id':'001','representative_path':str(source),
                'sha1_at_contact_sheet':hashlib.sha1(source.read_bytes()).hexdigest()}]}))
            args=builder['parse_args'](['--model','benchmark','--output-root',str(directory/'other'),
                                       '--benchmark-manifest',str(manifest)])
            builder['main'].__globals__['ROOT']=args.output_root
            self.assertEqual(builder['read_benchmark'](args.benchmark_manifest)[1],source)
            source.write_bytes(b'changed')
            with self.assertRaisesRegex(ValueError,'Benchmark changed'):
                builder['read_benchmark'](args.benchmark_manifest)

    def test_redesigned_special_exports_and_bind_anchors(self):
        import check_gun_assets as checker
        for mid in REDESIGN_PLAN_MODELS:
            with self.subTest(model=mid):
                meta=metadata(mid)
                checker.check_asset(ROOT/meta['model'],meta)
                checker.check_previews(ROOT/'assets/guns/preview',mid,meta)
                doc,_=checker.read_glb(ROOT/meta['model'])
                nodes=doc['nodes']; parents={c:i for i,n in enumerate(nodes) for c in n.get('children',[])}
                worlds={}
                def world(index):
                    if index not in worlds:
                        local=checker.local_matrix(nodes[index])
                        worlds[index]=checker.multiply(world(parents[index]),local) if index in parents else local
                    return worlds[index]
                names={n['name']:i for i,n in enumerate(nodes)}
                for name,anchor in plan(mid,{})['nodes'].items():
                    self.assertIn(name,names)
                    actual=checker.transform(world(names[name]),(0,0,0))
                    for got,want in zip(actual,anchor['at']):
                        self.assertAlmostEqual(got,want,places=6,msg=mid+' '+name)
                if mid=='game-revolver-heavy':
                    for suffix in ('','_lod1'):
                        lens=next(i for i,n in enumerate(nodes) if n['name'].startswith('part_release_status_lens') and n['name'].endswith('_lod1')==bool(suffix))
                        self.assertEqual(nodes[parents[lens]]['name'],'button_reload'+suffix)

    def test_runtime_lighting_uses_camera_normal_lambert_and_preserves_alpha(self):
        self.assertEqual(RUNTIME_LIGHTING['direction_to_light'],[.36,.70,.62])
        color=(.2,.4,.6,.3); direction=(.36,.70,.62)
        for n in (direction,tuple(v*8 for v in direction)):
            for actual,expected in zip(runtime_preview_rgba(n,color),color): self.assertAlmostEqual(actual,expected)
        dark=runtime_preview_rgba(tuple(-v for v in direction),color)
        for actual,expected in zip(dark,(.07,.14,.21,.3)): self.assertAlmostEqual(actual,expected)
        self.assertGreater(runtime_preview_rgba((0,1,0),color)[0],runtime_preview_rgba((1,0,0),color)[0])
        with self.assertRaises(ValueError): runtime_preview_rgba((0,0,0),color)

    def test_all_sight_anchors_share_bore_parallel_tip_height(self):
        for mid in TIERS:
            spec=plan(mid,None)
            with self.subTest(model=mid):
                self.assertEqual(spec['sights']['kind'],'notch-blade')
                front,rear=(spec['nodes'][name]['at'] for name in ('sight_front','sight_rear'))
                self.assertEqual(front[:2],rear[:2]); self.assertLess(front[2],rear[2])
                parts={p['name']:p for p in spec['parts']}
                for lod in (0,1):
                    self.assertEqual(geometry(parts['rear_sight_geometry'],lod)[0],geometry(parts['rear_sight_geometry'],0)[0])
                self.assertEqual(parts['rear_sight_geometry']['shape'],'notched_sight')
                self.assertLess(parts['front_sight_geometry']['size'][0],parts['rear_sight_geometry']['notch_width'])

    def test_sights_are_seated_in_actual_side_surface_not_global_bounds(self):
        # A slope can leave a small sight floating while whole-part AABBs overlap.
        # Check the authored roof at the sight centre, with a small seated overlap.
        for mid in TIERS:
            spec=plan(mid,runpy.run_path(str(ROOT/'assets/guns/src'/(mid+'.py')))['PARAMETERS'])
            parts={p['name']:p for p in spec['parts']}
            for name in ('front_sight_geometry','rear_sight_geometry'):
                sight=parts[name]; z=sight['at'][2]; roofs=[]
                for upper_name in ('barrel_shell','receiver','sight_rail_geometry'):
                    if upper_name not in parts: continue
                    upper=parts[upper_name]
                    points=[(a+upper['at'][2],b+upper['at'][1]) for a,b in upper['points']]
                    for a,b in zip(points,points[1:]+points[:1]):
                        if a[0]!=b[0] and min(a[0],b[0])<=z<=max(a[0],b[0]):
                            roofs.append(a[1]+(z-a[0])*(b[1]-a[1])/(b[0]-a[0]))
                bottom=sight['at'][1]-sight['size'][1]/2
                self.assertLessEqual(bottom,max(roofs)-.0002,(mid,name))

    def test_authored_silhouettes_and_connected_source_parts(self):
        from check_gun_assets import part_bounds, check_part_touches
        outlines=[]
        for mid in TIERS:
            module=runpy.run_path(str(ROOT/'assets/guns/src'/(mid+'.py')))
            outline=module['BODY_OUTLINE']; outlines.append(tuple(outline))
            self.assertTrue(25<=len(outline)<=50)
            self.assertEqual(len(set(outline)),len(outline))
            spec=plan(mid,module['PARAMETERS'])
            bounds={}
            for p in spec['parts']:
                verts,_=geometry(p,0)
                bounds[p['name']]=part_bounds([tuple(a+b for a,b in zip(v,p['at'])) for v in verts])
            check_part_touches(bounds,mid)
            if mid!='mnt-mg-heavy':
                grip=module['GRIP_OUTLINE']
                # Compare the centres of the two top and two heel corners.
                top=tuple(sum(p[i] for p in grip[:2])/2 for i in (0,1))
                bottom=tuple((grip[4][i]+grip[6][i])/2 for i in (0,1))
                rake=math.degrees(math.atan2(bottom[0]-top[0],top[1]-bottom[1]))
                self.assertTrue(12<=rake<=18,(mid,rake))
                self.assertTrue(85<=max(y for _,y in grip)-min(y for _,y in grip)<=100)
        self.assertEqual(len(set(outlines)),7)

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
                    self.assertIn(part['shape'],('outline','ring','box','cylinder','vented_tube','notched_sight'))
                    self.assertIn(part['parent'],[None,*spec['nodes']])
                for motion in meta['motion'].values():
                    self.assertIn(motion['node'],spec['nodes'])
                    self.assertIn(motion['kind'],('rotate','slide'))
                    self.assertEqual(motion['lod_nodes'],[motion['node'],motion['node']+'_lod1'])
                    self.assertAlmostEqual(sum(v*v for v in motion['axis']),1)
                    self.assertLess(motion['range'][0],motion['range'][1])
                self.assertEqual({n for n in spec['nodes'] if n.startswith('button_')},
                                 {b['node'] for b in meta.get('button',[])})

    def test_redesigned_special_adapters_match_metadata_and_static_aim(self):
        for mid in REDESIGN_PLAN_MODELS:
            with self.subTest(model=mid):
                module=runpy.run_path(str(ROOT/'assets/guns/src'/(mid+'.py')))
                params=module['PARAMETERS'];spec=plan(mid,params);meta=metadata(mid)
                self.assertEqual(params['length_mm'],meta['length_mm'])
                self.assertAlmostEqual(spec['length_m'],meta['length_mm']/1000)
                self.assertEqual(spec['sights']['kind'],'notch-blade')
                nodes=spec['nodes']
                for name in ('muzzle','fx_muzzle','fx_laser','sight_front','sight_rear'):
                    self.assertIsNone(nodes[name]['parent'])
                muzzle=nodes['muzzle']['at']
                self.assertLess(muzzle[2],-.01)
                front,rear=(nodes[name]['at'] for name in ('sight_front','sight_rear'))
                self.assertAlmostEqual(front[0],rear[0],places=8)
                self.assertAlmostEqual(front[1],rear[1],places=8)
                self.assertLess(front[2],rear[2])
                self.assertEqual({n for n in nodes if n.startswith('button_')},
                                 {b['node'] for b in meta.get('button',[])})
                self.assertEqual(meta['tints']['red'],
                                 {'body':'#a0a5aa','accent':'#c5484f'})
                self.assertEqual(meta['tints']['blue'],
                                 {'body':'#a0a5aa','accent':'#4276bf'})
                for motion in meta.get('motion',{}).values():
                    self.assertIn(motion['node'],nodes)
                self.assertEqual(meta['motion']['reload_control']['axis'],[1,0,0])
        for mid in ('game-energy-gun','game-tool-cutter'):
            nodes=plan(mid,None)['nodes']
            self.assertAlmostEqual(nodes['limit_recoil']['at'][2]-nodes['slide_recoil']['at'][2],.015,places=8)

    def test_redesign_plans_are_excluded_from_legacy_blender_dispatch(self):
        self.assertEqual(BUILDABLE_MODELS,LEGACY_BUILD_MODELS)
        self.assertEqual(BUILT_MODELS,LEGACY_BUILD_MODELS+REDESIGN_PLAN_MODELS)
        self.assertTrue(set(REDESIGN_PLAN_MODELS).isdisjoint(BUILDABLE_MODELS))
        build=runpy.run_path(str(ROOT/'assets/guns/build.py'))
        self.assertEqual(build['BUILDABLE_MODELS'],BUILDABLE_MODELS)
        self.assertEqual(build['select_models'](),list(BUILDABLE_MODELS))
        self.assertEqual(build['select_models'](['generic-pistol']),['generic-pistol'])
        for mid in REDESIGN_PLAN_MODELS:
            with self.subTest(model=mid):
                with self.assertRaisesRegex(ValueError,'not built by the legacy batch'):
                    build['select_models']([mid])

    def test_mounted_grip_is_at_rear_handle(self):
        path = ROOT/'assets/guns/src/mnt-mg-heavy.py'
        spec = plan('mnt-mg-heavy',runpy.run_path(str(path))['PARAMETERS'])
        handle = next(p for p in spec['parts'] if p['name']=='rear_handle_1')
        self.assertAlmostEqual(handle['at'][0],0)
        self.assertLess(spec['nodes']['grip_two']['at'][0],-.1)
        self.assertEqual(spec['nodes']['pivot_pitch']['parent'],'pivot_yaw')
        self.assertEqual(spec['nodes']['pivot_trigger']['parent'],'pivot_pitch')

    def test_slide_moves_upper_geometry_and_kick_moves_whole_visible_body(self):
        furniture={'frame','hand_grip','grip_panel_left','trigger_guard','cable_boss'}
        upper={'barrel_shell','muzzle_lens','front_sight_geometry','rear_sight_geometry'}
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
                    expected=list(parts[visible]['at'])
                    if 'sights' in spec: expected[1]+=parts[visible]['size'][1]/2
                    for actual,wanted in zip(expected,spec['nodes'][marker]['at']): self.assertAlmostEqual(actual,wanted)
                    self.assertIsNone(spec['nodes'][marker]['parent'])

    def test_preview_multiplies_exported_linear_factors_for_all_palettes(self):
        # Exported values stay stable; native GL multiplies these factors rather
        # than replacing them. The colour contract sets the body/accent factor
        # to 1.0, so the tint is the colour. Black/white/low-sRGB cases exercise
        # both branches.
        self.assertEqual(MATERIAL_RGBA,{'body':(1,1,1,1),'accent':(1,1,1,1),
                                        'dark':(.012,.012,.018,1),'glass':(.025,.10,.16,1)})
        palettes=[{'body':'#000000','accent':'#ffffff'}, {'body':'#0a0bff','accent':'#ff0b0a'}]
        for mid in BUILT_MODELS:
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
