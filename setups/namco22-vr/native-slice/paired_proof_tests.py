"""CPU protocol/oracle fixtures only; no native process, GL or image artifacts."""
import copy
import json
from pathlib import Path
import struct
import tempfile
import unittest
from unittest.mock import Mock

import paired_proof_protocol as p


class ProofProtocol(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.model=p.oracle()
        cls.base=json.loads(Path(__file__).with_name("snapshot-vectors.json").read_text())["vectors"][0]
        cls.resources=[bytes(n) for n in p.SIZES]

    def raw(self):
        b=bytearray.fromhex(self.base["preimage_hex"])
        for i in range(3): b[98+i*41+9:98+(i+1)*41]=bytes(32)
        return b

    def image(self, phase):
        # Hand-authored byte patches test image predicates, not a rendering substitute.
        rgb=bytearray(bytes([12,18,28])*(1280*480))
        for eye in range(2):
            for f,c in zip(self.model["markers"]["features"],self.model["markers"]["poses"][phase]["eyes"][eye]["centers_f64"]):
                x,y=map(p.frozen.f64hex,c); left=round(x)-6;top=round(y)-6
                for yy in range(top,top+12):
                    n=(yy*1280+eye*640+left)*3;rgb[n:n+36]=bytes(f["rgb"])*12
        return bytes(rgb)

    def test_root_matches_independent_snapshot_vector(self):
        root=p.fill_snapshot(self.raw(),self.resources,b"")
        self.assertEqual(root.hex(),self.base["preimage_hex"])
        self.assertEqual(p.sha(root),self.base["sha256"])

    def test_unhashed_marker_required(self):
        b=self.raw();b[107]=1
        with self.assertRaises(ValueError):p.fill_snapshot(b,self.resources,b"")

    def test_policy_camera_geometry_and_resources_reject(self):
        for offset,value in ((69,0),(77,0),(97,2),(225,3),(225+596,1)):
            b=self.raw();b[offset]=value
            with self.subTest(offset=offset),self.assertRaises(ValueError):p.fill_snapshot(b,self.resources,b"")
        with self.assertRaises(ValueError):p.fill_snapshot(self.raw(),[b"",*self.resources[1:]],b"")

    def test_correlation_matches_actual_input_bits(self):
        spec=self.model["markers"]["poses"][0]
        m=dict(case=0,phase=0,lease=1,sequence=1,frame=1,native_frame_counter=1,target_generation=1,
               head_f32=spec["head_f32"],anchor_f32=self.model["anchor_f32"],scale_f32=self.model["scene_units_per_metre_f32"],projection_f32=spec["projection_f32"],
               eyes=[dict(e,rect=self.model["rectangles"][i]) for i,e in enumerate(spec["eyes"])])
        self.assertEqual(len(p.pair_inputs(m,self.model,0,0,1,1)),64)
        for field,value in (("lease",2),("frame",2),("sequence",0),("target_generation",2)):
            bad=copy.deepcopy(m);bad[field]=value
            with self.assertRaises(ValueError):p.pair_inputs(bad,self.model,0,0,1,1)
        bad=copy.deepcopy(m);bad["eyes"][1]["view_f32"][12]="00000000"
        with self.assertRaises(ValueError):p.pair_inputs(bad,self.model,0,0,1,1)

    def test_independent_marker_predicates(self):
        a,b=self.image(0),self.image(1)
        self.assertEqual(len(p.pixels(a,self.model,0,0)),2)
        p.pixels(b,self.model,0,1);p.temporal(a,b,self.model)
        with self.assertRaises(ValueError):p.temporal(a,a,self.model)

    def test_blank_swapped_and_only_left_fail(self):
        a,b=self.image(0),self.image(1)
        blank=bytearray(a)
        for y in range(480): blank[(y*1280+640)*3:(y*1280+1280)*3]=bytes([12,18,28])*640
        with self.assertRaises(ValueError):p.pixels(blank,self.model,0,0)
        swapped=bytearray(a)
        for y in range(480):
            n=y*1280*3;swapped[n:n+1920],swapped[n+1920:n+3840]=a[n+1920:n+3840],a[n:n+1920]
        with self.assertRaises(ValueError):p.pixels(swapped,self.model,0,0)
        only_left=bytearray(b)
        for y in range(480):only_left[(y*1280+640)*3:(y*1280+1280)*3]=a[(y*1280+640)*3:(y*1280+1280)*3]
        with self.assertRaises(ValueError):p.temporal(a,only_left,self.model)

    def test_cz_must_have_both_eye_interiors(self):
        rgb=bytearray(bytes([12,18,28])*(1280*480))
        for eye,s in enumerate(self.model["cz"]["samples"]):
            x,y=s["pixel"]
            for yy in range(y-5,y+5):
                n=(yy*1280+eye*640+x-5)*3;rgb[n:n+30]=bytes([44,59,74])*10
        p.pixels(rgb,self.model,1,0)
        x,y=self.model["cz"]["samples"][1]["pixel"];n=(y*1280+640+x)*3;rgb[n:n+3]=bytes([80,100,120])
        with self.assertRaises(ValueError):p.pixels(rgb,self.model,1,0)

    def test_transaction_never_admits_partial_swap(self):
        t=p.Transaction()
        with self.assertRaises(ValueError):t.draw()
        t.preflight(1)
        with self.assertRaises(ValueError):t.swap()
        t.draw()
        with self.assertRaises(ValueError):t.swap()
        t.validated();t.swap()
        with self.assertRaises(ValueError):t.preflight(1)
        t.preflight(2)
        with self.assertRaises(ValueError):t.preflight(3)

    def test_json_duplicates_nonfinite_and_cache_drift(self):
        for text in ('{"x":1,"x":2}','{"x":NaN}'):
            with self.assertRaises(ValueError):p.strict_json(text)
        c=dict(locked=1,atlas_dim=1024,pages=1,budget_draw=0,budget_pump=0,pending=0,in_pump=0,
               evictions=0,page_resets=0,entries=[dict(texture=4,pal_gen=0,pal_group=1,cap=256)],
               palette_generation=[0]*128,misses=1,reallocs=0,subimages=0,cache_bytes=4096,page_texture=4)
        p.cache_valid(c);p.cache_valid(c,c)
        for field,value in (("pending",1),("misses",2),("page_texture",8)):
            bad=copy.deepcopy(c);bad[field]=value
            with self.assertRaises(ValueError):p.cache_valid(bad,c)

    def test_actual_scene_bound_to_frozen_model(self):
        # Separate schema encoder supplies a CZ source vector; this does not call C.
        s=copy.deepcopy(self.base["snapshot_fields"]);s["native_frame_counter"]=2;s["policy"]["fog"]=True
        q=s["quads"][0];q["order"]=0;q["cz_value"]=0
        table=bytearray(8192);table[4:6]=bytes([130,250])
        q["fog"]=dict(applies=True,table_present=True,rgb=[10,20,30],sdelta=0,alpha_const=-1,table_sha256="0"*64)
        for name in ("v","rv"):
            for vertex in q[name]:
                x,y,z=vertex["acn_view"];vertex["sx16"]=int((320+400*x/z)*16+.5);vertex["sy16"]=int((240-400*y/z)*16+.5)
                vertex["bri"]=64;vertex["bf"]=64*65536
        s["resources"]=[dict(r,sha256="0"*64) for r in s["resources"]]
        raw=p.frozen.snapshot_bytes(s)
        palette=bytearray(p.SIZES[0])
        for group,rgb in ((1,[30,155,245]),(2,[240,160,35]),(6,[80,100,120])):
            n=(group*256+2)*3;palette[n:n+3]=bytes(rgb)
        resources=[palette,bytes([2])*256+bytes(p.SIZES[1]-256),self.resources[2]]
        self.assertTrue(p.scene_valid(raw,resources,table,self.model,1,2))
        for offset in (225+24,225+84+40):
            changed=bytearray(raw);changed[offset]^=1
            with self.assertRaises(ValueError):p.scene_valid(changed,resources,table,self.model,1,2)
        palette[0]=1
        with self.assertRaises(ValueError):p.scene_valid(raw,resources,table,self.model,1,2)

    def test_generated_header_frozen_to_exact_oracle(self):
        import generate_paired_oracle as g
        self.assertEqual(Path(__file__).with_name("paired_oracle.h").read_text(),g.render())

    def test_dangling_links_rejected_in_requested_path_and_ancestors(self):
        import paired_proof as controller
        # Inert path doubles model dangling filesystem links without requiring
        # Windows symlink privileges or creating a native process.
        def path_double():
            part=Mock()
            part.parents=()
            part.exists.return_value=False
            part.is_symlink.return_value=False
            part.is_junction.return_value=False
            return part
        for location in ("requested","ancestor"):
            for kind in ("is_symlink","is_junction"):
                requested,parent=path_double(),path_double()
                requested.parents=(parent,)
                linked=requested if location=="requested" else parent
                getattr(linked,kind).return_value=True
                with self.subTest(location=location,kind=kind):
                    with self.assertRaisesRegex(ValueError,"linked private output/input forbidden"):
                        controller.no_links(requested)
                    requested.exists.assert_not_called()
                    parent.exists.assert_not_called()
                    requested.resolve.assert_not_called()
        fresh=path_double()
        controller.no_links(fresh)
        fresh.exists.assert_not_called()

    def test_source_interface_example_and_live_serializer_shape(self):
        import paired_proof_schema as schema
        import paired_proof as controller
        schema.check_example(schema.example())
        self.assertEqual(json.loads(Path(__file__).with_name("producer-interface.json").read_text()),schema.keysets())
        spec=self.model["markers"]["poses"][0]
        meta=dict(case=0,phase=0,lease=1,sequence=1,frame=1,native_frame_counter=1,target_generation=1,
          head_f32=spec["head_f32"],anchor_f32=self.model["anchor_f32"],projection_f32=spec["projection_f32"],
          readback_qpc=123,qpc_frequency=1000,eyes=[])
        for i,e in enumerate(spec["eyes"]):
            stamp=dict(lease=1,frame=1,native_frame_counter=1,sequence=1,target_generation=1,view_f32=e["view_f32"],projection_f32=spec["projection_f32"])
            meta["eyes"].append(dict(e,rect=self.model["rectangles"][i],entry_state=stamp,exit_state=copy.deepcopy(stamp)))
        scratch=Path.cwd()/".local";scratch.mkdir(exist_ok=True)
        with tempfile.TemporaryDirectory(dir=scratch) as tmp:
            root=Path(tmp)
            for name in ("scene_1.snapshot.bin","device.json","pair_1.before.json","pair_1.after.json","pair_1.cache_before.json","pair_1.cache_after.json"):
                (root/name).write_bytes(b"source fixture only\n")
            image=self.image(0)
            record=controller.pair_record(root,"0"*32,meta,"0"*64,"scene_1.snapshot.bin",image,p.pixels(image,self.model,0,0))
            self.assertEqual(set(record),set(schema.PAIR_KEYS))
            self.assertEqual(set(record["readback"]),set(schema.READBACK_KEYS))
            self.assertEqual(set(record["preflight"]),set(schema.PREFLIGHT_KEYS))
            for eye in record["eyes"]:
                self.assertEqual(set(eye),set(schema.EYE_KEYS));self.assertEqual(set(eye["entry"]),set(schema.STAMP_KEYS))
            bad=copy.deepcopy(meta);bad["eyes"][1]["entry_state"]["native_frame_counter"]=2
            with self.assertRaises(ValueError):controller.pair_record(root,"0"*32,bad,"0"*64,"scene_1.snapshot.bin",image,p.pixels(image,self.model,0,0))


if __name__ == "__main__":unittest.main()
