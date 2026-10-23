"""Signed horizontal patch disparities for explicitly declared two-layer SBS captures.

Example (run only while the timing queue is idle):
  python -B measure_rois.py capture.png --layout sbs --eye-layers 2 --rois rois.json --out result.json
ROI JSON: [{"name":"caller label", "rect":[x0,y0,x1,y1]}], normalized within one eye.
Optional --provenance JSON must contain capture_sha256, layout="sbs", eye_layers=2,
eye_width, eye_height; it may additionally reference a source CSV row. A declaration
or source-row reference is recorded as supplied evidence, never inferred from pixels.
No near/far/HUD meaning or physical scale is inferred from caller labels.
"""
import argparse
from collections import Counter
import hashlib
import json
import math
from pathlib import Path

import numpy as np
from PIL import Image


def digest(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest()


def validate_layout(width, height, layout, eye_layers):
    if layout != 'sbs' or eye_layers != 2:
        raise ValueError('requires explicit SBS layout and exactly two source eye layers; mono is unsupported')
    if width < 2 or width % 2 or height < 1:
        raise ValueError('SBS image width must be positive and even')
    return width // 2, height


def validate_rois(rois):
    if not isinstance(rois, list) or not 1 <= len(rois) <= 16:
        raise ValueError('supply 1 to 16 normalized ROI definitions')
    names = set()
    for roi in rois:
        if not isinstance(roi, dict) or not isinstance(roi.get('name'), str) or not roi['name']:
            raise ValueError('each ROI requires a nonempty caller name')
        if roi['name'] in names:
            raise ValueError('ROI names must be unique')
        names.add(roi['name'])
        rect = roi.get('rect')
        if (not isinstance(rect, list) or len(rect) != 4 or
                any(not isinstance(v, (float, int)) or not math.isfinite(v) for v in rect)):
            raise ValueError('ROI rect must contain four finite normalized numbers')
        x0, y0, x1, y1 = rect
        if not (0 <= x0 < x1 <= 1 and 0 <= y0 < y1 <= 1):
            raise ValueError('ROI rectangle must be ordered and within [0,1]')


def best_shift(reference, other, x, y, size, max_disparity, min_std=6.0,
               max_sad=25.0, min_uniqueness=0.15):
    """Find d where other(x-d) matches reference(x), at identical y."""
    template = reference[y:y + size, x:x + size]
    if template.shape != (size, size) or float(template.std()) < min_std:
        return None, 'low_texture'
    scores = []
    for disparity in range(-max_disparity, max_disparity + 1):
        other_x = x - disparity
        if other_x < 0 or other_x + size > other.shape[1]:
            continue
        patch = other[y:y + size, other_x:other_x + size]
        score = float(np.mean(np.abs(template - patch)))
        scores.append((score, disparity))
    if not scores:
        return None, 'no_candidate'
    scores.sort()
    score, disparity = scores[0]
    alternatives = [s for s, d in scores if abs(d - disparity) >= 3]
    if not alternatives:
        return None, 'insufficient_alternatives'
    alternative = min(alternatives)
    uniqueness = (alternative - score) / (alternative + 1e-6)
    if score > max_sad:
        return None, 'photometric_mismatch'
    if uniqueness < min_uniqueness:
        return None, 'ambiguous'
    if abs(disparity) == max_disparity:
        return None, 'search_boundary'
    return {'d_px': int(disparity), 'sad': score, 'uniqueness': uniqueness}, None


def match_patch(left, right, x, y, size=24, max_disparity=240,
                reverse_tolerance=1, **thresholds):
    forward, reason = best_shift(left, right, x, y, size, max_disparity, **thresholds)
    if forward is None:
        return None, reason
    right_x = x - forward['d_px']
    reverse, reason = best_shift(right, left, right_x, y, size, max_disparity, **thresholds)
    if reverse is None:
        return None, 'reverse_' + reason
    reverse_error = abs(forward['d_px'] + reverse['d_px'])
    if reverse_error > reverse_tolerance:
        return None, 'reverse_inconsistent_or_occluded'
    return dict(forward, x=x, y=y, size=size, right_x=right_x,
                reverse_d_px=reverse['d_px'], reverse_error_px=reverse_error), None


def summarize(matches, eye_width):
    ds = [match['d_px'] for match in matches]
    result = {'accepted': len(ds), 'median_px': None, 'p25_px': None, 'p75_px': None,
              'min_px': None, 'max_px': None, 'median_at_1920_px': None,
              'integer_search_step_px': 1, 'physical_scale': 'unmeasured'}
    if ds:
        result.update(median_px=float(np.median(ds)), p25_px=float(np.percentile(ds, 25)),
                      p75_px=float(np.percentile(ds, 75)), min_px=min(ds), max_px=max(ds),
                      median_at_1920_px=float(np.median(ds)) * 1920 / eye_width)
    return result


def measure(path, rois, layout, eye_layers, patch_size=24, max_disparity=240,
            max_patches=64, provenance=None):
    validate_rois(rois)
    if patch_size < 4 or max_disparity < 1 or not 1 <= max_patches <= 256:
        raise ValueError('invalid patch size, disparity range or patch count')
    with Image.open(path) as image:
        width, height = image.size
        eye_width, _ = validate_layout(width, height, layout, eye_layers)
        if patch_size > min(eye_width, height):
            raise ValueError('patch exceeds eye image')
        pixels = np.asarray(image.convert('L'), dtype=np.float32)
    capture_hash = digest(path)
    if provenance is not None:
        if not isinstance(provenance, dict):
            raise ValueError('provenance must be a JSON object')
        expected = dict(capture_sha256=capture_hash, layout='sbs', eye_layers=2,
                        eye_width=eye_width, eye_height=height)
        if any(provenance.get(key) != value for key, value in expected.items()):
            raise ValueError('provenance capture hash, layout, layers or dimensions do not match')
    left, right = pixels[:, :eye_width], pixels[:, eye_width:]
    output = {
        'capture': str(Path(path).resolve()), 'capture_sha256': capture_hash,
        'layout': layout, 'eye_layers': eye_layers, 'eye_size': [eye_width, height],
        'provenance_kind': 'caller_supplied_capture_metadata' if provenance is not None else 'caller_declared_layout_and_layers',
        'source_row_linkage': 'caller assertion; not independently verified',
        'provenance': provenance,
        'limits': 'Horizontal integer-pixel matches only; occlusion rejection is conservative, not a proof. Labels and layer provenance come from caller. No automatic stereo, depth, scale or headset acceptance.',
        'parameters': dict(patch_size=patch_size, max_disparity=max_disparity,
                           max_patches_per_roi=max_patches, min_std=6, max_sad=25,
                           min_uniqueness=0.15, reverse_tolerance_px=1),
        'rois': []}
    for roi in rois:
        x0, y0, x1, y1 = roi['rect']
        x0, y0 = math.ceil(x0 * eye_width), math.ceil(y0 * height)
        x1, y1 = math.floor(x1 * eye_width), math.floor(y1 * height)
        positions = [(x, y) for y in range(y0, y1 - patch_size + 1, patch_size)
                     for x in range(x0, x1 - patch_size + 1, patch_size)]
        if len(positions) > max_patches:
            indices = np.linspace(0, len(positions) - 1, max_patches, dtype=int)
            positions = [positions[int(i)] for i in indices]
        matches, rejected = [], Counter()
        for x, y in positions:
            match, reason = match_patch(left, right, x, y, patch_size, max_disparity)
            if match is None:
                rejected[reason] += 1
            else:
                matches.append(match)
        output['rois'].append(dict(name=roi['name'], normalized_rect=roi['rect'],
                                   pixel_rect=[x0, y0, x1, y1], attempted=len(positions),
                                   rejected=dict(rejected), summary=summarize(matches, eye_width),
                                   matches=matches))
    return output


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('png', type=Path)
    parser.add_argument('--layout', required=True, choices=['sbs'])
    parser.add_argument('--eye-layers', required=True, type=int, choices=[2])
    parser.add_argument('--rois', required=True, type=Path)
    parser.add_argument('--provenance', type=Path)
    parser.add_argument('--patch-size', type=int, default=24)
    parser.add_argument('--max-disparity', type=int, default=240)
    parser.add_argument('--max-patches', type=int, default=64)
    parser.add_argument('--out', type=Path)
    args = parser.parse_args()
    try:
        rois = json.loads(args.rois.read_text(encoding='utf-8-sig'))
        provenance = json.loads(args.provenance.read_text(encoding='utf-8-sig')) if args.provenance else None
        result = measure(args.png, rois, args.layout, args.eye_layers, args.patch_size,
                         args.max_disparity, args.max_patches, provenance)
        result['roi_definitions_path'] = str(args.rois.resolve())
        result['roi_definitions_sha256'] = digest(args.rois)
        result['provenance_path'] = str(args.provenance.resolve()) if args.provenance else None
        if args.provenance:
            result['provenance_sha256'] = digest(args.provenance)
        text = json.dumps(result, indent=2, allow_nan=False)
        if args.out:
            args.out.write_text(text + '\n', encoding='utf-8')
        else:
            print(text)
    except (ValueError, OSError) as error:
        parser.error(str(error))


if __name__ == '__main__':
    main()
