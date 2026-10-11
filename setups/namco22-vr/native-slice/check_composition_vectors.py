"""CPU-only contract vectors, not a producer/Viewer receipt implementation."""
import copy
import hashlib
import json
import math
from pathlib import Path
import re
import struct

DOMAIN = b"ACN22-COMPOSITION-V1\0"
REVISIONS = ("pose_revision", "anchor_revision", "scale_revision")
FIELDS = {"head_from_anchor", "anchor_from_scene", "scene_units_per_metre",
          "scale_provisional", *REVISIONS}


def f32(value):
    if type(value) not in (int, float) or not math.isfinite(value):
        raise ValueError("finite numeric binary32 input required")
    try:
        raw = struct.pack("<f", value)
    except (OverflowError, struct.error) as exc:
        raise ValueError("binary32 overflow") from exc
    if not math.isfinite(struct.unpack("<f", raw)[0]):
        raise ValueError("binary32 overflow")
    return raw


def preimage(receipt):
    if set(receipt) != FIELDS:
        raise ValueError("fixture common input fields differ")
    raw = bytearray(DOMAIN)
    for field in ("head_from_anchor", "anchor_from_scene"):
        values = receipt[field]
        if type(values) is not list or len(values) != 16:
            raise ValueError("matrix length/type")
        for value in values:
            raw.extend(f32(value))
    scale = receipt["scene_units_per_metre"]
    scale_bytes = f32(scale)
    if struct.unpack("<f", scale_bytes)[0] <= 0:
        raise ValueError("positive scale required")
    raw.extend(scale_bytes)
    flag = receipt["scale_provisional"]
    if type(flag) is not bool:
        raise ValueError("boolean flag required")
    raw.append(int(flag))
    for field in REVISIONS:
        value = receipt[field]
        if type(value) is not str or not re.fullmatch(r"0|[1-9][0-9]*", value):
            raise ValueError("canonical decimal revision required")
        number = int(value)
        if number > 0xffffffffffffffff:
            raise ValueError("uint64 revision overflow")
        raw.extend(struct.pack("<Q", number))
    assert len(raw) == 178
    return bytes(raw)


def reject(receipt):
    try:
        preimage(receipt)
    except ValueError:
        return
    raise AssertionError("malformed input accepted")


def main():
    fixture = json.loads(Path(__file__).with_name("composition-vectors.json").read_text(encoding="utf-8"))
    assert fixture["schema"] == "acn22-composition-vectors/v1"
    vectors = fixture["vectors"]
    assert len(vectors) == 4
    for vector in vectors:
        raw = preimage(vector["receipt_fields"])
        expected = bytes.fromhex(vector["preimage_hex"])
        assert raw == expected, vector["name"]
        assert hashlib.sha256(raw).hexdigest() == vector["sha256"], vector["name"]
        reversed_keys = dict(reversed(list(vector["receipt_fields"].items())))
        assert preimage(reversed_keys) == raw
        print(vector["name"], len(raw), vector["sha256"])
    assert vectors[0]["sha256"] != vectors[1]["sha256"]  # flag participates
    assert vectors[2]["sha256"] != vectors[3]["sha256"]  # signed zero retained
    base = vectors[0]["receipt_fields"]
    mutations = [
        ("pose_revision", "01"), ("pose_revision", "-1"),
        ("pose_revision", "18446744073709551616"), ("pose_revision", 1),
        ("scale_provisional", 1), ("scale_provisional", "true"),
        ("scene_units_per_metre", 0), ("scene_units_per_metre", -1),
        ("scene_units_per_metre", True), ("scene_units_per_metre", float("inf")),
        ("scene_units_per_metre", 1e100), ("scene_units_per_metre", 1e-100),
        ("head_from_anchor", [0] * 15),
        ("head_from_anchor", [float("nan")] + [0] * 15),
        ("anchor_from_scene", [False] + [0] * 15),
    ]
    for field, value in mutations:
        changed = copy.deepcopy(base)
        changed[field] = value
        reject(changed)
    for field in REVISIONS:
        changed = copy.deepcopy(base)
        changed[field] = "1"
        assert preimage(changed) != preimage(base)
    print("4 frozen vectors; 15 invalid inputs rejected; key order and revision mutations checked. No GL/build/runtime.")


if __name__ == "__main__":
    main()
