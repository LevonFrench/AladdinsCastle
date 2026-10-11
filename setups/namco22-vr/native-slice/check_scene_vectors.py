"""Synthetic snapshot/oracle CPU vectors only; no native renderer or GL."""
import copy
import hashlib
import json
import math
from pathlib import Path
import struct

HERE = Path(__file__).parent
POLICY = "shade fog fog_before_shade flat_white write_prio_alpha texel_centre wide_backdrop depth_test g_tex_opaque g_eng_tex_tile16 g_tex_clipbox g_eng_degen_uv_legacy g_tex_pow2sample g_tex_pow2alloc atlas_active".split()
META = "texbank cmode cz_type zsort cz_adjust flags_raw cz_value behind order pick_code".split()
RESOURCE_SIZES = (98304, 16777216, 2621440)


def integer(value, low, high):
    if type(value) is not int or not low <= value <= high:
        raise ValueError("integer range/type")
    return value


def digest_hex(value):
    if type(value) is not str or len(value) != 64 or any(c not in "0123456789abcdef" for c in value):
        raise ValueError("SHA-256 encoding")
    return bytes.fromhex(value)


def finite(value):
    if type(value) not in (int, float) or not math.isfinite(value):
        raise ValueError("finite number required")
    return value


def snapshot_bytes(s):
    camera, p = s["camera"], s["policy"]
    b = bytearray(b"ACN22-SNAPSHOT-V1\0")
    b += struct.pack("<QII4d3B", integer(s["native_frame_counter"], 0, 2**64-1),
                     integer(camera["raster_width"], 1, 4096), integer(camera["raster_height"], 1, 2160),
                     *[finite(camera[k]) for k in ("fx", "fy", "cx", "cy")],
                     *[integer(x, 0, 255) for x in s["background_rgb"]])
    for k in POLICY:
        if type(p[k]) is not bool:
            raise ValueError("policy must be boolean")
        b.append(int(p[k]))
    b += struct.pack("<IB2f", integer(p["g_tex_fixedcap"], 0, 2**32-1),
                     integer(p["fade_policy"], 0, 0), *[finite(x) for x in p["scene_x"]])
    assert len(b) == 97
    if len(s["resources"]) != 3:
        raise ValueError("resource count")
    b.append(3)
    for i, r in enumerate(s["resources"], 1):
        if r["id"] != i or r["byte_count"] != RESOURCE_SIZES[i-1]:
            raise ValueError("resource identity/size/order")
        b += struct.pack("<BQ", i, r["byte_count"]) + digest_hex(r["sha256"])
    b += struct.pack("<I", integer(len(s["quads"]), 1, 1024))
    assert len(b) == 225
    for q in s["quads"]:
        start = len(b)
        if q["nrv"] != 4 or q["ndv"] != 0:
            raise ValueError("unsupported source polygon")
        b += struct.pack("<iiI", q["nrv"], q["ndv"], integer(q["color"], 0, 2**32-1))
        b += struct.pack("<10i", *[integer(q[k], -2**31, 2**31-1) for k in META])
        b += struct.pack("<4i4H2i", *q["clip"], *q["uvbox"], q["objectflags"], q["direct"])
        assert len(b)-start == 84
        for name in ("v", "rv"):
            if len(q[name]) != 4:
                raise ValueError("vertex count")
            for v in q[name]:
                ints = [v[k] for k in "sx16 sy16 z u v bri valid uf vf bf".split()]
                b += struct.pack("<iiiIIiiiii3d", *ints, *[finite(x) for x in v["acn_view"]])
        assert len(b)-start == 596
        f = q["fog"]
        for k in ("applies", "table_present"):
            if type(f[k]) is not bool:
                raise ValueError("fog flag")
        integer(f["sdelta"], -256, 255); integer(f["alpha_const"], -1, 255)
        dh = digest_hex(f["table_sha256"])
        if not f["table_present"] and dh != bytes(32):
            raise ValueError("absent table hash")
        if not f["applies"] and (f["table_present"] or any(f["rgb"]) or f["sdelta"] or f["alpha_const"] != 255):
            raise ValueError("inactive fog fields")
        b += struct.pack("<BB3Bii", f["applies"], f["table_present"], *f["rgb"], f["sdelta"], f["alpha_const"]) + dh
        assert len(b)-start == 641
    return bytes(b)


def recipe_digest(size, patches):
    """Fixture-only zero-filled byte recipe; full actual resources must be checked."""
    h = hashlib.sha256(); offset = 0
    for patch in patches + [{"offset": size, "hex": ""}]:
        target = patch["offset"]
        if target < offset or target > size:
            raise ValueError("patch order/range")
        while offset < target:
            n = min(target-offset, 4096); h.update(bytes(n)); offset += n
        raw = bytes.fromhex(patch["hex"])
        if offset+len(raw) > size:
            raise ValueError("patch outside blob")
        h.update(raw); offset += len(raw)
    return h.hexdigest()


def oracle_bytes(o):
    def check(x):
        if isinstance(x, dict):
            for k, v in x.items():
                if type(k) is not str: raise ValueError("string key required")
                check(k); check(v)
        elif isinstance(x, list):
            for v in x: check(v)
        elif type(x) is str:
            if any(ord(c) < 32 or ord(c) > 126 for c in x): raise ValueError("ASCII string required")
        elif type(x) is int:
            integer(x, -(2**53-1), 2**53-1)
        elif x is not None and type(x) is not bool:
            raise ValueError("oracle floating numbers/unknown types forbidden")
    check(o)
    return b"ACN22-ORACLE-V1\0" + json.dumps(o, sort_keys=True, separators=(",", ":"), ensure_ascii=True, allow_nan=False).encode("ascii")


def f32hex(word):
    return struct.unpack("<f", int(word, 16).to_bytes(4, "little"))[0]


def f64hex(word):
    return struct.unpack("<d", int(word, 16).to_bytes(8, "little"))[0]


def eye_point(view, xyz):
    p = [xyz[0], xyz[1], -xyz[2], 1]
    return [sum(view[c*4+r]*p[c] for c in range(4)) for r in range(4)]


def project(view, projection, xyz):
    e = eye_point(view, xyz)
    c = [sum(projection[k*4+r]*e[k] for k in range(4)) for r in range(4)]
    return [320*(1+c[0]/c[3]), 240*(1-c[1]/c[3])]


def composed_view(o, head_words, eye_words):
    f = lambda x: struct.unpack("<f", struct.pack("<f", x))[0]
    def mul(a,b):
        out = []
        for col in range(4):
            for row in range(4):
                total = f(0)
                for k in range(4): total = f(total+f(a[k*4+row]*b[col*4+k]))
                out.append(total)
        return out
    scale = [1 if i%5 == 0 else 0 for i in range(16)]
    scale[0] = scale[5] = scale[10] = f(1/f32hex(o["scene_units_per_metre_f32"]))
    return mul(mul(mul(list(map(f32hex,eye_words)),list(map(f32hex,head_words))),
                   list(map(f32hex,o["anchor_f32"]))),scale)


def numeric_checks(o):
    for pose in o["markers"]["poses"]:
        pose_centers = []
        for eye in pose["eyes"]:
            v = list(map(f32hex, eye["view_f32"])); p = list(map(f32hex, pose["projection_f32"]))
            assert struct.pack("<16f", *v) == struct.pack("<16f", *composed_view(o,pose["head_f32"],eye["eye_from_head_f32"]))
            centers = []
            for feature, expected in zip(o["markers"]["features"], eye["centers_f64"]):
                got = project(v, p, feature["center_xyz"])
                assert max(abs(got[i]-f64hex(expected[i])) for i in range(2)) < 1e-7
                centers.append(got)
            pose_centers.append(centers)
        for i, expected in enumerate(pose["disparities_f64"]):
            got = pose_centers[0][i][0]-pose_centers[1][i][0]
            assert abs(got-f64hex(expected)) < 1e-7
        assert f64hex(pose["disparities_f64"][0])-f64hex(pose["disparities_f64"][1]) > 8
    cz = o["cz"]
    xyz = cz["xyz"]; uv = cz["uv_q16"]
    table = bytearray(8192); table[4] = 130; table[5] = 250
    for sample in cz["samples"]:
        v = list(map(f32hex, sample["view_f32"])); p = list(map(f32hex, cz["projection_f32"]))
        assert struct.pack("<16f", *v) == struct.pack("<16f", *composed_view(o,cz["head_f32"],sample["eye_from_head_f32"]))
        ids = sample["triangle"]
        e = [eye_point(v, xyz[i]) for i in ids]; screen = [project(v, p, xyz[i]) for i in ids]
        sub = lambda a,b: [a[0]-b[0], a[1]-b[1]]
        cross = lambda a,b: a[0]*b[1]-a[1]*b[0]
        a,b,c = screen; point = [sample["pixel"][0]+.5, sample["pixel"][1]+.5]
        det = cross(sub(b,a),sub(c,a))
        l1 = cross(sub(point,a),sub(c,a))/det; l2 = cross(sub(b,a),sub(point,a))/det
        weights = [1-l1-l2,l1,l2]; den = sum(weights[i]/-e[i][2] for i in range(3))
        bary = [weights[i]/-e[i][2]/den for i in range(3)]
        board = sum(bary[i]*xyz[ids[i]][2] for i in range(3)); depth = 1/den
        affine = sum(weights[i]*xyz[ids[i]][2] for i in range(3))
        got_uv = [sum(bary[i]*uv[ids[i]][k]/65536 for i in range(3)) for k in range(2)]
        assert min(bary) > .05 and depth > .16
        assert abs(board-f64hex(sample["board_z_f64"])) < 1e-6
        assert abs(depth-f64hex(sample["eye_depth_m_f64"])) < 1e-9
        assert max(abs(got_uv[k]-f64hex(sample["uv_f64"][k])) for k in range(2)) < 1e-6
        alpha = lambda z: 255-table[max(0,min(8191,int(z)//256))]
        assert alpha(board) == 125 and alpha(depth*1000) == 255 and alpha(affine) == 5
        color = [round(10+70*125/255),round(20+80*125/255),round(30+90*125/255)]
        assert color == cz["expected_rgb"]
        print("CZ eye", sample["eye_index"], "center", point, "board", board, "eye_m", depth, "alpha", alpha(board))


def main():
    vectors = json.loads((HERE/"snapshot-vectors.json").read_text())
    for vector in vectors["vectors"]:
        s = vector["snapshot_fields"]
        for r in s["resources"]:
            assert recipe_digest(r["byte_count"], r["fixture_patches"]) == r["sha256"]
        f = s["quads"][0]["fog"]
        if f["table_present"]:
            assert recipe_digest(8192, vector["fog_fixture_patches"]) == f["table_sha256"]
        raw = snapshot_bytes(s)
        assert raw.hex() == vector["preimage_hex"]
        assert hashlib.sha256(raw).hexdigest() == vector["sha256"]
        print(vector["name"], len(raw), vector["sha256"])
    assert len({v["sha256"] for v in vectors["vectors"]}) == 3
    base = vectors["vectors"][0]["snapshot_fields"]
    for mutate in (lambda x: x["resources"].reverse(),
                   lambda x: x["resources"][0].update(byte_count=1),
                   lambda x: x["policy"].update(shade=1),
                   lambda x: x["quads"][0].update(nrv=3),
                   lambda x: x["quads"][0]["fog"].update(table_sha256="1"*64)):
        bad = copy.deepcopy(base); mutate(bad)
        try: snapshot_bytes(bad)
        except ValueError: pass
        else: raise AssertionError("malformed snapshot accepted")
    changed = copy.deepcopy(base); changed["resources"][0]["sha256"] = "1"*64
    assert snapshot_bytes(changed) != snapshot_bytes(base)
    assert recipe_digest(98304, [{"offset":0,"hex":"01"}]) != base["resources"][0]["sha256"]
    o = json.loads((HERE/"oracle-manifest.json").read_text())
    raw = oracle_bytes(o)
    frozen = json.loads((HERE/"oracle-vector.json").read_text())
    assert raw.hex() == frozen["preimage_hex"]
    assert hashlib.sha256(raw).hexdigest() == frozen["sha256"]
    assert oracle_bytes(dict(reversed(list(o.items())))) == raw
    for value in (1.5, float("nan"), "nonascii-\u00e9"):
        bad = copy.deepcopy(o); bad["schema"] = value
        try: oracle_bytes(bad)
        except ValueError: pass
        else: raise AssertionError("malformed oracle accepted")
    print("oracle", len(raw), frozen["sha256"])
    numeric_checks(o)
    print("3 snapshot roots, oracle bytes, 2-pose/2-eye marker projections, 2 CZ pixel centers; CPU-only.")


if __name__ == "__main__": main()
