"""Source-only paired producer protocol helpers; CPU tests use synthetic bytes."""
import hashlib
import json
import math
from pathlib import Path
import struct

import check_composition_vectors as composition
import check_scene_vectors as frozen

WIDTH, HEIGHT = 1280, 480
SIZES = (98304, 16777216, 2621440)
ORACLE_SHA = "91a70c73edadf62cafaaebea83f073fb3aeffd97a585f9cd2a0b876e00cf14d1"


def sha(data):
    return hashlib.sha256(data).hexdigest()


def strict_json(text):
    def unique(pairs):
        d = {}
        for k, v in pairs:
            if k in d: raise ValueError("duplicate JSON key")
            d[k] = v
        return d
    return json.loads(text, object_pairs_hook=unique,
                      parse_constant=lambda x: (_ for _ in ()).throw(ValueError("nonfinite JSON")))


def oracle():
    value = strict_json((Path(__file__).with_name("oracle-manifest.json")).read_text())
    if sha(frozen.oracle_bytes(value)) != ORACLE_SHA:
        raise ValueError("oracle drift")
    return value


def fill_snapshot(raw, resources, fog_table):
    """Complete actual native export's SHA holes, never infer native fields."""
    if raw[:18] != b"ACN22-SNAPSHOT-V1\0" or len(resources) != 3:
        raise ValueError("snapshot signature/resources")
    if len(raw) < 225 or raw[97] != 3:
        raise ValueError("snapshot prefix")
    count = struct.unpack_from("<I", raw, 221)[0]
    if count not in (1, 2) or len(raw) != 225+641*count:
        raise ValueError("bounded quad count")
    camera = struct.unpack_from("<II4d", raw, 26)
    if camera != (640,480,400.,400.,320.,240.) or raw[66:69] != bytes([12,18,28]):
        raise ValueError("native camera/background policy")
    flags = list(raw[69:84])
    wanted = [1,flags[1],0,0,0,1,0,0,1,0,0,0,0,0,1]
    if flags != wanted or flags[1] not in (0,1) or struct.unpack_from("<IB2f",raw,84) != (1,0,0.,640.):
        raise ValueError("native consumed policy")
    result = bytearray(raw)
    for i, blob in enumerate(resources):
        offset = 98+i*41
        if raw[offset] != i+1 or struct.unpack_from("<Q",raw,offset+1)[0] != SIZES[i] or len(blob) != SIZES[i]:
            raise ValueError("resource size/order")
        if raw[offset+9:offset+41] != bytes(32):
            raise ValueError("export is not explicitly unhashed")
        result[offset+9:offset+41] = bytes.fromhex(sha(blob))
    for i in range(count):
        q = 225+i*641
        meta = struct.unpack_from("<iiI10i4i4H2i",raw,q)
        if meta[0:2] != (4,0) or meta[10] != 0 or meta[-2:] != (0,0) or meta[13:17] != (0,639,0,479):
            raise ValueError("unsupported native geometry")
        for v in range(8):
            values = struct.unpack_from("<iiiIIiiiii3d",raw,q+84+v*64)
            if values[6] != 1 or values[2] <= 0 or any(not math.isfinite(x) for x in values[-3:]):
                raise ValueError("invalid source vertex")
        if raw[q+84:q+340] != raw[q+340:q+596]:
            raise ValueError("source/raw draw vertex drift")
        f = q+596; applies,present,r,g,b,delta,const = struct.unpack_from("<BB3Bii",raw,f)
        if raw[f+13:f+45] != bytes(32): raise ValueError("fog hash hole")
        if present:
            if not applies or len(fog_table) != 8192 or (r,g,b,delta,const) != (10,20,30,0,-1):
                raise ValueError("CZ material")
            result[f+13:f+45] = bytes.fromhex(sha(fog_table))
        elif (applies,r,g,b,delta,const) != (0,0,0,0,0,255):
            raise ValueError("inactive fog material")
    return bytes(result)


def cache_valid(report, old=None):
    if report["locked"] != 1 or report["atlas_dim"] != 1024 or report["pages"] != 1:
        raise ValueError("cache admission")
    if any(report[k] != 0 for k in ("budget_draw","budget_pump","pending","in_pump","evictions","page_resets")):
        raise ValueError("deferred/evicted cache")
    if not 1 <= len(report["entries"]) <= 3:
        raise ValueError("cache entries")
    if report["palette_generation"] != [0]*128:
        raise ValueError("fixture palette generation drift")
    for e in report["entries"]:
        if e["texture"] <= 0 or e["pal_gen"] != report["palette_generation"][e["pal_group"]] or e["cap"] != 256:
            raise ValueError("stale material cache")
    if old:
        for k in ("entries","palette_generation","misses","reallocs","subimages","cache_bytes","page_texture"):
            if report[k] != old[k]: raise ValueError("cache drift: "+k)


def scene_valid(raw, resources, table, model, case, lease):
    if struct.unpack_from("<Q",raw,18)[0] != lease:raise ValueError("snapshot frame/lease mismatch")
    palette=bytearray(SIZES[0])
    for group,rgb in ((1,[30,155,245]),(2,[240,160,35]),(6,[80,100,120])):
        pos=(group*256+2)*3;palette[pos:pos+3]=bytes(rgb)
    if resources[0] != palette or resources[1] != bytes([2])*256+bytes(SIZES[1]-256) or resources[2] != bytes(SIZES[2]):
        raise ValueError("actual formula resource drift")
    expected_table=bytearray(8192);expected_table[4:6]=bytes([130,250])
    if table != expected_table:raise ValueError("actual CZ source table drift")
    count=struct.unpack_from("<I",raw,221)[0]
    if count != (2 if case==0 else 1):raise ValueError("scene count")
    for i in range(count):
        start=225+i*641;meta=struct.unpack_from("<iiI10i4i4H2i",raw,start)
        if case==0:
            feature=model["markers"]["features"][1-i];cx,cy,z=feature["center_xyz"];hw,hh=feature["half_size"]
            xyz=[[cx+ (hw if j in (1,2) else -hw),cy+(-hh if j>=2 else hh),z] for j in range(4)]
            uv=[[63*65536 if j in (1,2) else 0,63*65536 if j>=2 else 0] for j in range(4)]
            group=2-i;zsort=z;order=1-i;uvbox=(0,63,0,63);cz_type=0
        else:
            xyz=model["cz"]["xyz"];uv=model["cz"]["uv_q16"];group=6;zsort=8000;order=0;uvbox=(0,255,0,127);cz_type=2
        if meta[2:13] != ((64<<16)|(group<<8),0,0,cz_type,zsort,0,0,0,0,order,0) or meta[17:21] != uvbox:
            raise ValueError("native metadata/order/material drift")
        for j in range(4):
            values=struct.unpack_from("<iiiIIiiiii3d",raw,start+84+j*64)
            x,y,z=xyz[j]
            sx=math.floor((320+400*x/z)*16+.5);sy=math.floor((240-400*y/z)*16+.5)
            expected=(sx,sy,z,uv[j][0]>>16,uv[j][1]>>16,64,1,uv[j][0],uv[j][1],64*65536,*xyz[j])
            if values != expected:raise ValueError("actual original source XYZ/UV/brightness drift")
    return True


def pair_inputs(meta, model, case, phase, lease, sequence):
    if meta["case"] != case or meta["phase"] != phase or meta["lease"] != lease or meta["sequence"] != sequence:
        raise ValueError("native pair identity")
    if meta["frame"] != lease or meta["native_frame_counter"] != lease or meta["target_generation"] != 1:
        raise ValueError("native frame/target identity")
    spec = model["markers"]["poses"][phase] if case == 0 else model["cz"]
    expected = spec["eyes"] if case == 0 else spec["samples"]
    if meta["head_f32"] != spec["head_f32"] or meta["anchor_f32"] != model["anchor_f32"]:
        raise ValueError("common pose/anchor drift")
    if meta["projection_f32"] != spec["projection_f32"] or meta["scale_f32"] != model["scene_units_per_metre_f32"]:
        raise ValueError("projection/scale drift")
    for i, eye in enumerate(meta["eyes"]):
        if eye["eye_index"] != i or eye["rect"] != model["rectangles"][i] or eye["view_f32"] != expected[i]["view_f32"] or eye["eye_from_head_f32"] != expected[i]["eye_from_head_f32"]:
            raise ValueError("eye input drift")
    if len(meta["eyes"]) != 2: raise ValueError("eye count")
    inputs = dict(head_from_anchor=list(map(frozen.f32hex,meta["head_f32"])),
                  anchor_from_scene=list(map(frozen.f32hex,meta["anchor_f32"])),
                  scene_units_per_metre=frozen.f32hex(meta["scale_f32"]),scale_provisional=True,
                  pose_revision=str(sequence),anchor_revision="0",scale_revision="0")
    return sha(composition.preimage(inputs))


def eye_bytes(rgb, eye):
    if len(rgb) != WIDTH*HEIGHT*3 or eye not in (0,1): raise ValueError("RGB size/eye")
    return b"".join(rgb[(y*WIDTH+eye*640)*3:(y*WIDTH+eye*640+640)*3] for y in range(HEIGHT))


def pixels(rgb, model, case, phase):
    if len(rgb) != WIDTH*HEIGHT*3: raise ValueError("RGB size")
    result=[]
    near_far=[]
    for eye in range(2):
        clear=(10*WIDTH+eye*640+10)*3
        if rgb[clear:clear+3] != bytes([12,18,28]): raise ValueError("eye clear/orientation")
        if case == 1:
            x,y=model["cz"]["samples"][eye]["pixel"]; offset=(y*WIDTH+eye*640+x)*3
            if max(abs(rgb[offset+k]-model["cz"]["expected_rgb"][k]) for k in range(3)) > 2:
                raise ValueError("native CZ pixel")
            count=0
            for yy in range(y-20,y+21):
                for xx in range(x-12,x+13):
                    n=(yy*WIDTH+eye*640+xx)*3
                    if all(abs(rgb[n+k]-model["cz"]["expected_rgb"][k]) <= 2 for k in range(3)): count+=1
            if count < 64: raise ValueError("missing CZ eye interior")
            result.append(dict(eye_index=eye,cz_pixel=[x,y],rgb=list(rgb[offset:offset+3]),interior_count=count))
            continue
        features=[];centers=[]
        spec=model["markers"]["poses"][phase]["eyes"][eye]
        for f, expected in zip(model["markers"]["features"],spec["centers_f64"]):
            cx,cy=map(frozen.f64hex,expected); total_x=total_y=count=0
            # Bounding box is deliberately larger than the formula marker, but excludes borders.
            for y in range(max(16,int(cy)-35),min(464,int(cy)+36)):
                for x in range(max(16,int(cx)-40),min(624,int(cx)+41)):
                    pos=(y*WIDTH+eye*640+x)*3
                    if all(abs(rgb[pos+k]-f["rgb"][k]) <= 2 for k in range(3)):
                        total_x+=x+.5;total_y+=y+.5;count+=1
            if count < 64: raise ValueError("missing per-eye marker content")
            actual=[total_x/count,total_y/count]
            if max(abs(actual[k]-[cx,cy][k]) for k in range(2)) > 1:
                raise ValueError("marker order/orientation/position")
            centers.append(actual);features.append(dict(name=f["name"],count=count,centroid=actual))
        near_far.append(centers);result.append(dict(eye_index=eye,features=features))
    if case == 0:
        ds=[near_far[0][i][0]-near_far[1][i][0] for i in range(2)]
        expected=list(map(frozen.f64hex,model["markers"]["poses"][phase]["disparities_f64"]))
        if any(abs(ds[i]-expected[i]) > 2 for i in range(2)) or ds[0]-ds[1] <= 8:
            raise ValueError("near/far disparity")
    return result


def temporal(before, after, model):
    for eye in range(2):
        for feature_index,f in enumerate(model["markers"]["features"]):
            changed=0
            centers=[list(map(frozen.f64hex,pose["eyes"][eye]["centers_f64"][feature_index])) for pose in model["markers"]["poses"]]
            for y in range(max(16,int(min(c[1] for c in centers))-35),min(464,int(max(c[1] for c in centers))+36)):
                for x in range(max(16,int(min(c[0] for c in centers))-40),min(624,int(max(c[0] for c in centers))+41)):
                    n=(y*WIDTH+eye*640+x)*3
                    a,b=before[n:n+3],after[n:n+3]
                    if a != b and (all(abs(a[k]-f["rgb"][k]) <= 2 for k in range(3)) or all(abs(b[k]-f["rgb"][k]) <= 2 for k in range(3))): changed+=1
            if changed < 32: raise ValueError("only one eye/marker changed")


class Transaction:
    """Controller state gate: no draw before preflight, no swap before pixels."""
    def __init__(self): self.sequence=0;self.state="idle"
    def preflight(self, sequence):
        if self.state != "idle" or sequence != self.sequence+1: raise ValueError("stale/incomplete pair")
        self.sequence=sequence;self.state="ready"
    def draw(self):
        if self.state != "ready": raise ValueError("no preflight")
        self.state="drawn"
    def validated(self):
        if self.state != "drawn": raise ValueError("no complete image")
        self.state="checked"
    def swap(self):
        if self.state != "checked": raise ValueError("partial swap")
        self.state="idle"
