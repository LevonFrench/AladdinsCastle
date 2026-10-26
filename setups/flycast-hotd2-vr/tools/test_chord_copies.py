#!/usr/bin/env python3
"""The universal stick-click layer (ACVRChord.h) is copied into three hosts that build separately. Fail if the copies differ.

  Model 2 viewer     setups/mame-model2-vr/presenter/legacy/ACVRChord.h
  Supermodel patch   setups/supermodel-vr/patch/ACVRChord.h   (the reference copy and its test)
  Flycast patch      setups/flycast-hotd2-vr/patch/acvr-hotd2-vr.patch, new file core/rend/vr/acvr_chord.h

The Flycast patch also carries copies of the shared U4 hold ring headers (libacvr/src, core/rend/vr/acvr/): fail if
any of those differs from libacvr.
"""
import pathlib
import sys

ROOT = pathlib.Path(__file__).resolve().parents[3]
RING_HEADERS = ('stick_chord.hpp', 'hold_ring.hpp', 'hold_ring_gl.hpp', 'hold_ring_host.hpp')


def patch_copy(patch: pathlib.Path, name: str) -> str:
    lines = patch.read_text(encoding='utf-8').split('\n')
    out, inside, body = [], False, False
    for line in lines:
        if line.startswith('diff --git'):
            inside = line.endswith('b/' + name)
            body = False
            continue
        if not inside:
            continue
        if line.startswith('@@'):
            body = True
            continue
        if body and line.startswith('+'):
            out.append(line[1:])
    return '\n'.join(out) + '\n'


def main() -> int:
    reference = (ROOT / 'setups/supermodel-vr/patch/ACVRChord.h').read_text(encoding='utf-8')
    copies = {
        'Model 2 viewer': (ROOT / 'setups/mame-model2-vr/presenter/legacy/ACVRChord.h').read_text(encoding='utf-8'),
        'Flycast patch': patch_copy(ROOT / 'setups/flycast-hotd2-vr/patch/acvr-hotd2-vr.patch', 'core/rend/vr/acvr_chord.h'),
    }
    failed = False
    for name, text in copies.items():
        if text.replace('\r\n', '\n') != reference.replace('\r\n', '\n'):
            print(f'FAIL: the {name} copy of ACVRChord.h differs from setups/supermodel-vr/patch/ACVRChord.h')
            failed = True
    patch = ROOT / 'setups/flycast-hotd2-vr/patch/acvr-hotd2-vr.patch'
    for name in RING_HEADERS:
        shared = (ROOT / 'libacvr/src' / name).read_text(encoding='utf-8').replace('\r\n', '\n')
        if patch_copy(patch, 'core/rend/vr/acvr/' + name) != shared:
            print(f'FAIL: the Flycast patch copy of {name} differs from libacvr/src/{name}')
            failed = True
    if failed:
        return 1
    print('PASS: ACVRChord.h is identical in the Model 2 viewer, the Supermodel patch and the Flycast patch')
    print('PASS: the Flycast patch carries the libacvr hold ring headers unchanged: ' + ', '.join(RING_HEADERS))
    return 0


if __name__ == '__main__':
    sys.exit(main())
