#!/usr/bin/env python
"""Static check of the AladdinsCastle Wii motion mappings for DolphinXR.

Checks every WiimoteNew.ini / profile given (default: the templates next to this file):
  * every backticked input name exists on DolphinXR's "OpenXR/0/OpenXR Controller" device
    (names built the same way as Source/Core/InputCommon/ControllerInterface/OpenXR/OpenXR.cpp
    at the pinned commit; pass --source <DolphinXR source> to re-derive them from the file);
  * every Wii Remote button and D-pad direction has a binding, and so does every Nunchuk
    button and stick direction when the Nunchuk is the selected extension;
  * the device line names the OpenXR controller;
  * with no file arguments, the stick-click layer (Plus, Minus, Home, Remote 2, GameCube Start/Z
    and templates/Hotkeys.ini) matches universal_controls.py.
A file with neither a [Profile] nor a [Wiimote1] section, a missing file, or an empty
Profiles/Wiimote glob fails. Exit code 0 when all files pass. Standard library only.
"""
import argparse
import re
import sys
from pathlib import Path

HERE = Path(__file__).resolve().parent
DEVICE = 'OpenXR/0/OpenXR Controller'
WIIMOTE = ['Buttons/A', 'Buttons/B', 'Buttons/1', 'Buttons/2', 'Buttons/-', 'Buttons/+',
           'Buttons/Home', 'D-Pad/Up', 'D-Pad/Down', 'D-Pad/Left', 'D-Pad/Right']
NUNCHUK = ['Nunchuk/Buttons/C', 'Nunchuk/Buttons/Z', 'Nunchuk/Stick/Up', 'Nunchuk/Stick/Down',
           'Nunchuk/Stick/Left', 'Nunchuk/Stick/Right']


def builtin_names():
    names = set()
    for hand, primary, secondary in (('Left', 'X', 'Y'), ('Right', 'A', 'B')):
        for b in (primary, secondary, 'Menu', 'Trigger', 'Squeeze', 'Thumbstick'):
            names.add(f'{hand} Button {b}')
        for a in ('Trigger', 'Squeeze', 'Squeeze Force'):
            names.add(f'{hand} {a}')
        for axis in 'XY':
            for sign in '+-':
                names.add(f'{hand} Thumbstick {axis}{sign}')
        for m in ('Accel Up', 'Accel Down', 'Accel Left', 'Accel Right', 'Accel Forward',
                  'Accel Backward', 'Gyro Pitch Up', 'Gyro Pitch Down', 'Gyro Roll Left',
                  'Gyro Roll Right', 'Gyro Yaw Left', 'Gyro Yaw Right'):
            names.add(f'{hand} Raw {m}')
    names.update({'Motor', 'Motor Left', 'Motor Right'})
    return names


def names_from_source(src):
    """Confirm the name fragments OpenXR.cpp concatenates are still in the source file."""
    text = (Path(src) / 'Source/Core/InputCommon/ControllerInterface/OpenXR/OpenXR.cpp').read_text(
        encoding='utf-8')
    fragments = ['"OpenXR Controller"', '"OpenXR"', '"Left"', '"Right"', '" Button X"',
                 '" Button A"', '" Button Y"', '" Button B"', '" Button Menu"',
                 '" Button Trigger"', '" Button Squeeze"', '" Button Thumbstick"', '" Trigger"',
                 '" Squeeze"', '" Squeeze Force"', '" Thumbstick "', '" Raw Accel Up"',
                 '" Raw Gyro Yaw Right"', '"Motor"', '"Motor Left"', '"Motor Right"']
    missing = [f for f in fragments if f not in text]
    if missing:
        raise SystemExit('OpenXR.cpp no longer contains: ' + ', '.join(missing))
    return builtin_names()


def parse_ini(path):
    sections, current = {}, None
    for line in Path(path).read_text(encoding='utf-8').splitlines():
        line = line.strip()
        if not line or line.startswith(('#', ';')):
            continue
        if line.startswith('[') and line.endswith(']'):
            current = sections.setdefault(line[1:-1], {})
        elif '=' in line and current is not None:
            k, v = line.split('=', 1)
            current[k.strip()] = v.strip()
    return sections


def check_section(label, sec, valid):
    errors = []
    if sec.get('Device') != DEVICE:
        errors.append(f'{label}: Device is {sec.get("Device")!r}, expected {DEVICE!r}')
    need = list(WIIMOTE) + (NUNCHUK if sec.get('Extension', 'None') != 'None' else [])
    for key in need:
        if not sec.get(key):
            errors.append(f'{label}: {key} has no binding')
    for key, value in sec.items():
        for name in re.findall(r'`([^`]*)`', value):
            if name not in valid:
                errors.append(f'{label}: {key} uses unknown input `{name}`')
        if value.count('(') != value.count(')'):
            errors.append(f'{label}: {key} has unbalanced parentheses')
    return errors


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('files', nargs='*')
    ap.add_argument('--source', help='DolphinXR source root to re-derive input names from')
    args = ap.parse_args()
    valid = names_from_source(args.source) if args.source else builtin_names()
    errors = []
    if args.files:
        files = list(args.files)
    else:
        profiles = sorted((HERE / 'Profiles/Wiimote').glob('*.ini'))
        if not profiles:
            errors.append('Profiles/Wiimote has no *.ini files to check')
        files = [str(HERE / 'WiimoteNew.ini'), *map(str, profiles)]
    for f in files:
        if not Path(f).is_file():
            errors.append(f'{f}: file not found')
            print('FAIL ' + f)
            continue
        sections = parse_ini(f)
        if 'Profile' not in sections and 'Wiimote1' not in sections:
            errors.append(f'{f}: has neither a [Profile] nor a [Wiimote1] section')
        if 'Profile' in sections:
            errors += check_section(f + ' [Profile]', sections['Profile'], valid)
        if 'Wiimote1' in sections:
            if sections['Wiimote1'].get('Source') != '3':
                errors.append(f + ': [Wiimote1] Source must be 3 (OpenXR Wii Remote)')
            errors += check_section(f + ' [Wiimote1]', sections['Wiimote1'], valid)
        print(('FAIL ' if any(e.startswith(f) for e in errors) else 'ok   ') + f)
    if not args.files:
        # Universal controls contract v1: stick-click expressions come from one module.
        import universal_controls
        if universal_controls.main([]):
            errors.append('universal_controls.py --write has not been applied')
    for e in errors:
        print('  ' + e)
    return 1 if errors else 0


if __name__ == '__main__':
    sys.exit(main())
