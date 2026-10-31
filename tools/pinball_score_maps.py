# SPDX-License-Identifier: GPL-3.0-only
"""Build data/pinball-score-maps.json from a pinned checkout of the Pinball Memory Maps project.

Only the high score and mode champion entries are kept, each with its CPU addresses already worked out
(start and count, or a list when they are not in a row), plus the ROM index and each platform's NVRAM
layout. The output is a derived database under the Open Database License (ODbL) v1.0, like its source.

    python -I tools/pinball_score_maps.py <maps-checkout> <commit> data/pinball-score-maps.json
"""
import json, pathlib, sys

SOURCE = 'https://github.com/tomlogic/pinball-memory-maps'
NOTICE = 'This program makes use of content from the Pinball Memory Maps project.'
KEPT = ('encoding', 'endian', 'nibble', 'mask', 'scale', 'suffix', 'null', 'default')

def number(value):
    return int(value, 16) if isinstance(value, str) else int(value)

def addresses(part):
    if 'offsets' in part:
        base = number(part.get('start', 0))
        return [base + number(o) for o in part['offsets']]
    start = number(part['start'])
    if 'end' in part:
        return list(range(start, number(part['end']) + 1))
    return list(range(start, start + int(part.get('length', 1))))

def descriptor(part, step=0):
    out = {k: part[k] for k in KEPT if k in part}
    if 'mask' in out:
        out['mask'] = number(out['mask'])
    found = [a + step for a in addresses(part)]
    if found == list(range(found[0], found[0] + len(found))):
        out['start'], out['count'] = found[0], len(found)
    else:
        out['addresses'] = found
    return out

def entries(section, kind):
    for template in section:
        repeat = template.get('repeat', {'count': 1, 'step': 0})
        for i in range(int(repeat['count'])):
            score = template.get('score')
            if not isinstance(score, dict) or score.get('encoding') not in ('bcd', 'int'):
                continue
            label = template.get('label', '').replace('{#}', str(i + 1))
            entry = {'kind': kind, 'label': label, 'short': template.get('short_label', label).replace('{#}', str(i + 1)),
                     'score': descriptor(score, i * int(repeat['step']))}
            initials = template.get('initials')
            if isinstance(initials, dict) and initials.get('encoding') == 'ch':
                entry['initials'] = descriptor(initials, i * int(repeat['step']))
            yield entry

def platform(path):
    p = json.loads(path.read_text(encoding='utf-8'))
    nvram = [{'address': number(r['address']), 'size': number(r['size']), 'nibble': r.get('nibble', 'both')}
             for r in p['memory_layout'] if r['type'] == 'nvram']
    return {'endian': p.get('endian', 'big'), 'nvram': nvram}

def main(root, commit, out):
    root = pathlib.Path(root)
    platforms, maps, roms = {}, [], {}
    for path in sorted(root.glob('maps/**/*.map.json')):
        m = json.loads(path.read_text(encoding='utf-8'))
        meta = m['_metadata']
        scores = list(entries(m.get('high_scores', []), 'high')) + list(entries(m.get('mode_champions', []), 'champion'))
        if not scores:
            continue
        name = meta['platform']
        if name not in platforms:
            platforms[name] = platform(root / 'platforms' / (name + '.json'))
        entry = {'platform': name, 'scores': scores}
        if 'char_map' in meta:
            entry['charMap'] = meta['char_map']
        for rom in meta['roms']:
            roms[rom] = len(maps)
        maps.append(entry)
    data = {'source': SOURCE, 'commit': commit, 'license': 'ODbL-1.0', 'notice': NOTICE,
            'platforms': platforms, 'maps': maps, 'roms': dict(sorted(roms.items()))}
    pathlib.Path(out).write_bytes((json.dumps(data, separators=(',', ':'), ensure_ascii=False) + '\n').encode('utf-8'))
    print(f'{len(maps)} maps, {len(roms)} ROM sets, {len(platforms)} platforms')

if __name__ == '__main__':
    main(*sys.argv[1:4])
