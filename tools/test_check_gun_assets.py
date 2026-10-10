"""Synthetic malformed-GLB regressions. No Blender, GPU, or game content."""
import importlib.util
import json
from pathlib import Path
import struct
import tempfile
import unittest

spec = importlib.util.spec_from_file_location('checker', Path(__file__).with_name('check_gun_assets.py'))
checker = importlib.util.module_from_spec(spec)
spec.loader.exec_module(checker)


def fixture():
    points = struct.pack('<9f',-.01,0,-.23,.01,0,-.23,0,.01,0)
    indices = struct.pack('<3H',0,1,2)
    binary = points+indices
    names = ['grip','muzzle','sight_front','sight_rear','pivot_trigger','fx_muzzle','fx_laser','LOD0','LOD1','mesh0','mesh1']
    nodes = [{'name':n} for n in names]
    nodes[0]['children'] = list(range(1,9))
    nodes[1]['translation'] = [0,.02,-.23]
    nodes[7]['children'],nodes[8]['children'] = [9],[10]
    nodes[9]['mesh'],nodes[10]['mesh'] = 0,0
    doc = {'asset':{'version':'2.0'},'scene':0,'scenes':[{'nodes':[0]}], 'nodes':nodes,
           'buffers':[{'byteLength':len(binary)}],
           'bufferViews':[{'buffer':0,'byteOffset':0,'byteLength':36}, {'buffer':0,'byteOffset':36,'byteLength':6}],
           'accessors':[{'bufferView':0,'componentType':5126,'count':3,'type':'VEC3'},
                        {'bufferView':1,'componentType':5123,'count':3,'type':'SCALAR'}],
           'materials':[{'name':n} for n in ('body','accent','dark','glass')],
           'meshes':[{'primitives':[{'attributes':{'POSITION':0},'indices':1,'material':i} for i in range(4)]}]}
    meta = {'id':'synthetic','features':[],'hold':'one-hand','length_mm':230}
    return doc,binary,meta


def write_glb(path,doc,binary):
    chunk = json.dumps(doc).encode()
    chunk += b' ' * (-len(chunk)%4)
    binary += b'\0' * (-len(binary)%4)
    blob = struct.pack('<4sII',b'glTF',2,12+8+len(chunk)+8+len(binary))
    blob += struct.pack('<II',len(chunk),0x4e4f534a)+chunk
    blob += struct.pack('<II',len(binary),0x004e4942)+binary
    path.write_bytes(blob)


class GunAssetTests(unittest.TestCase):
    def check(self,doc,binary,meta):
        local = checker.ROOT/'.local/gun-tests'
        local.mkdir(parents=True,exist_ok=True)
        with tempfile.TemporaryDirectory(dir=local) as directory:
            path = Path(directory)/'fixture.glb'
            write_glb(path,doc,binary)
            return checker.check_asset(path,meta)

    def test_valid_fixture(self):
        result = self.check(*fixture())
        self.assertEqual(result['triangles'],{'LOD0':4,'LOD1':4})
        self.assertAlmostEqual(result['length_mm']['LOD0'],230,places=3)

    def test_rejects_malformed_assets(self):
        mutations = {
            'duplicate node': lambda d: d['nodes'][2].update(name='muzzle'),
            'extra root': lambda d: d['scenes'][0]['nodes'].append(9),
            'unreachable node': lambda d: d['nodes'].append({'name':'stray'}),
            'cyclic hierarchy': lambda d: d['nodes'][9].update(children=[0]),
            'multiple parents': lambda d: d['nodes'][8]['children'].append(9),
            'wrong bore': lambda d: d['nodes'][1].update(rotation=[0,1,0,0]),
            'recoil affects ray': lambda d: (d['nodes'][0]['children'].remove(1), d['nodes'][7]['children'].append(1)),
            'wrong root units': lambda d: d['nodes'][0].update(scale=[1000,1000,1000]),
            'embedded image': lambda d: d.update(images=[{'bufferView':0}]),
            'external buffer': lambda d: d['buffers'][0].update(uri='external.bin'),
            'bad stride': lambda d: d['bufferViews'][0].update(byteStride=4),
            'bad bounds': lambda d: d['accessors'][0].update(count=100),
            'not triangles': lambda d: d['meshes'][0]['primitives'][0].update(mode=1),
            'negative mesh index': lambda d: d['nodes'][9].update(mesh=-1),
            'fake accessor bounds': lambda d: (d['accessors'][0].update(min=[0,0,-.23],max=[0,0,0]),d['nodes'][9].update(scale=[1,1,.1])),
        }
        for name,mutate in mutations.items():
            with self.subTest(name=name):
                doc,binary,meta = fixture()
                mutate(doc)
                with self.assertRaises((ValueError,KeyError,IndexError)):
                    self.check(doc,binary,meta)

    def test_out_of_range_indices(self):
        doc,binary,meta = fixture()
        binary = binary[:36]+struct.pack('<3H',0,1,99)
        with self.assertRaisesRegex(ValueError,'outside POSITION'):
            self.check(doc,binary,meta)

    def test_lengths_use_world_vertices(self):
        doc,binary,meta = fixture()
        for index in (9,10):
            doc['nodes'][index]['scale'] = [1,1,.5]
        with self.assertRaisesRegex(ValueError,'bore length'):
            self.check(doc,binary,meta)

    def test_motion_missing_lod_target(self):
        doc,binary,meta = fixture()
        meta['motion'] = {'trigger':{'node':'pivot_trigger','kind':'rotate','axis':[1,0,0],
                                    'range':[0,.3],'drive':'trigger','lod_nodes':['pivot_trigger','pivot_trigger_lod1']}}
        with self.assertRaisesRegex(ValueError,'missing LOD target'):
            self.check(doc,binary,meta)

    def test_triangle_budget_counts_instances(self):
        doc,binary,meta = fixture()
        doc['meshes'][0]['primitives'] *= 501
        with self.assertRaisesRegex(ValueError,'LOD1 triangle budget'):
            self.check(doc,binary,meta)

    def test_nonfinite_coordinates(self):
        doc,binary,meta = fixture()
        binary = struct.pack('<f',float('nan'))+binary[4:]
        with self.assertRaisesRegex(ValueError,'non-finite'):
            self.check(doc,binary,meta)


if __name__ == '__main__':
    unittest.main()
