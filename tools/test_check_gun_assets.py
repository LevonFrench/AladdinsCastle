"""Synthetic malformed-GLB regressions. No Blender, GPU, or game content."""
import importlib.util
from copy import deepcopy
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


def motion_fixture():
    doc,binary,meta=fixture()
    doc['nodes'][0]['children'].remove(4)
    doc['nodes'][7]['children'].append(4)
    doc['nodes'][4]['extras']={'semantic_node':'pivot_trigger'}
    doc['nodes'].append({'name':'pivot_trigger_lod1','extras':{'semantic_node':'pivot_trigger'}})
    doc['nodes'][8]['children'].append(11)
    meta['motion']={'trigger':{'node':'pivot_trigger','kind':'rotate','axis':[1,0,0],
        'range':[0,.3],'drive':'trigger','lod_nodes':['pivot_trigger','pivot_trigger_lod1']}}
    return doc,binary,meta


def color_fixture(components=4,component=5126,normalized=False):
    doc,binary,meta=fixture()
    binary+=b'\0'*(-len(binary)%4)
    offset=len(binary)
    if component==5126:
        values=[.25,.5,.75,1][:components]*3
        payload=struct.pack('<'+'f'*len(values),*values)
    else:
        code,maximum=('B',255) if component==5121 else ('H',65535)
        values=[maximum//4,maximum//2,maximum*3//4,maximum][:components]*3
        payload=struct.pack('<'+code*len(values),*values)
    binary+=payload
    doc['buffers'][0]['byteLength']=len(binary)
    doc['bufferViews'].append({'buffer':0,'byteOffset':offset,'byteLength':len(payload)})
    doc['accessors'].append({'bufferView':2,'componentType':component,'count':3,
        'type':'VEC'+str(components),'normalized':normalized})
    for primitive in doc['meshes'][0]['primitives']:
        primitive['attributes']['COLOR_0']=2
    return doc,binary,meta


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

    def test_supported_drives_duration_boundaries_and_distinct_targets(self):
        for drive in ('trigger','recoil','pump','selector','yaw','pitch','button:special'):
            for duration in (None,1,10000):
                doc,binary,meta=motion_fixture()
                row=meta['motion']['trigger']; row['drive']=drive
                if duration is not None:
                    row['duration_ms']=duration
                with self.subTest(drive=drive,duration=duration):
                    self.check(doc,binary,meta)
        # Multiple distinct targets may share a logical drive, as native accepts.
        doc,binary,meta=motion_fixture()
        doc['nodes'].extend([{'name':'second','extras':{'semantic_node':'second'}},
                            {'name':'second_lod1','extras':{'semantic_node':'second'}}])
        doc['nodes'][7]['children'].append(12); doc['nodes'][8]['children'].append(13)
        meta['motion']['second']={**meta['motion']['trigger'],'node':'second','lod_nodes':['second','second_lod1']}
        self.check(doc,binary,meta)

    def test_rejects_unsupported_drives_and_invalid_durations(self):
        for drive in ('invented','button:','button','',None,True):
            doc,binary,meta=motion_fixture(); meta['motion']['trigger']['drive']=drive
            with self.subTest(drive=drive),self.assertRaisesRegex(ValueError,'unsupported motion drive'):
                self.check(doc,binary,meta)
        for duration in (0,-1,10001,1.0,True,None,'45'):
            doc,binary,meta=motion_fixture(); meta['motion']['trigger']['duration_ms']=duration
            with self.subTest(duration=duration),self.assertRaisesRegex(ValueError,'duration'):
                self.check(doc,binary,meta)

    def test_rejects_duplicate_driven_targets(self):
        doc,binary,meta=motion_fixture()
        meta['motion']['second']=deepcopy(meta['motion']['trigger'])
        meta['motion']['second']['drive']='recoil'
        with self.assertRaisesRegex(ValueError,'multiple motion drivers'):
            self.check(doc,binary,meta)

    def test_motion_table_shape_and_native_count_limit(self):
        for invalid in ([],{'trigger':[]},{'':motion_fixture()[2]['motion']['trigger']}):
            doc,binary,meta=motion_fixture(); meta['motion']=invalid
            with self.subTest(motion=invalid),self.assertRaisesRegex(ValueError,'motion.*table'):
                self.check(doc,binary,meta)
        doc,binary,meta=motion_fixture()
        for index in range(1,65):
            name='extra_'+str(index); start=len(doc['nodes'])
            doc['nodes'].extend([{'name':name,'extras':{'semantic_node':name}},
                                {'name':name+'_lod1','extras':{'semantic_node':name}}])
            doc['nodes'][7]['children'].append(start); doc['nodes'][8]['children'].append(start+1)
            meta['motion'][name]={**meta['motion']['trigger'],'node':name,'lod_nodes':[name,name+'_lod1']}
            if index==63:
                self.check(doc,binary,meta) # exactly 64 distinct motions
        with self.assertRaisesRegex(ValueError,'at most 64'):
            self.check(doc,binary,meta)

    def test_static_references_cannot_be_driven_or_descend_from_either_lod_driver(self):
        for name in ('grip','muzzle','sight_front','sight_rear','fx_muzzle','fx_laser','grip_two','sight_extra','fx_extra'):
            doc,binary,meta=motion_fixture()
            names=[n['name'] for n in doc['nodes']]
            if name not in names:
                doc['nodes'][0]['children'].append(len(doc['nodes']))
                doc['nodes'].append({'name':name})
            canonical=next(n for n in doc['nodes'] if n['name']==name)
            canonical['extras']={'semantic_node':name}
            doc['nodes'][0]['children'].append(len(doc['nodes']))
            doc['nodes'].append({'name':name+'_lod1','extras':{'semantic_node':name}})
            meta['motion']['trigger'].update(node=name,lod_nodes=[name,name+'_lod1'])
            with self.subTest(direct=name),self.assertRaisesRegex(ValueError,'static'):
                self.check(doc,binary,meta)
        for name in ('sight_front','sight_rear','grip_two','sight_extra','fx_extra'):
            for driven in (4,11):
                doc,binary,meta=motion_fixture()
                names=[n['name'] for n in doc['nodes']]
                if name not in names:
                    doc['nodes'][0]['children'].append(len(doc['nodes']))
                    doc['nodes'].append({'name':name})
                index=next(i for i,n in enumerate(doc['nodes']) if n['name']==name)
                doc['nodes'][0]['children'].remove(index)
                # Indirect ancestry proves this walks beyond the immediate parent.
                intermediate=len(doc['nodes'])
                doc['nodes'].append({'name':'reference_parent','children':[index]})
                doc['nodes'][driven]['children']=[intermediate]
                with self.subTest(descendant=name,lod=driven),self.assertRaisesRegex(ValueError,'static'):
                    self.check(doc,binary,meta)

    def test_material_rgba_defaults_and_transparency_are_valid(self):
        doc,binary,meta=fixture()
        for material in doc['materials']:
            material['pbrMetallicRoughness']={'baseColorFactor':[0,.5,1,.25]}
        self.check(doc,binary,meta)

    def test_rejects_material_rgba_and_texture_references(self):
        for rgba in ([1,1,1],[1,1,1,1,1],[1,1,1,float('nan')],[1,1,float('inf'),1],
                     [1,1,1,-.1],[1.1,1,1,1],[True,1,1,1],['1',1,1,1],[10**500,1,1,1],None):
            doc,binary,meta=fixture()
            doc['materials'][0]['pbrMetallicRoughness']={'baseColorFactor':rgba}
            with self.subTest(rgba=rgba),self.assertRaisesRegex(ValueError,'material RGBA'):
                self.check(doc,binary,meta)
        for key in ('baseColorTexture','metallicRoughnessTexture'):
            doc,binary,meta=fixture(); doc['materials'][0]['pbrMetallicRoughness']={key:{'index':0}}
            with self.subTest(texture=key),self.assertRaisesRegex(ValueError,'textures'):
                self.check(doc,binary,meta)

    def test_vertex_rgb_rgba_float_and_normalized_colors(self):
        for count in (3,4):
            for component,normalized in ((5126,False),(5121,True),(5123,True)):
                with self.subTest(components=count,component=component):
                    self.check(*color_fixture(count,component,normalized))

    def test_rejects_invalid_vertex_colors(self):
        for value in (float('nan'),float('inf'),-.01,1.01):
            doc,binary,meta=color_fixture()
            offset=doc['bufferViews'][2]['byteOffset']+12 # first alpha
            binary=binary[:offset]+struct.pack('<f',value)+binary[offset+4:]
            with self.subTest(alpha=value),self.assertRaisesRegex(ValueError,'non-finite|vertex color'):
                self.check(doc,binary,meta)
        for mutate in (lambda d:d['accessors'][2].update(count=2),
                       lambda d:d['accessors'][2].update(type='SCALAR'),
                       lambda d:d['accessors'][2].update(componentType=5121,normalized=False),
                       lambda d:d['meshes'][0]['primitives'][0]['attributes'].update(COLOR_0=None)):
            doc,binary,meta=color_fixture(); mutate(doc)
            with self.subTest(mutate=mutate),self.assertRaisesRegex(ValueError,'vertex color'):
                self.check(doc,binary,meta)


if __name__ == '__main__':
    unittest.main()
