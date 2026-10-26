#!/usr/bin/env python
"""Universal controls contract v1 (U1-U4) as Dolphin control expressions.

One source for every DolphinXR Wii Remote, GameCube pad and hotkey expression that touches the
stick clicks, so the profiles cannot drift apart. Standard library only.

  U1  Start / Plus     left stick click alone
  U2  Select / Minus   right stick click alone (Wii only; GameCube has no Select)
  U3  Recentre         both clicks, the second within 250 ms of the first, released within 1 s
  U4  Quit and save    both clicks held 3 s  ->  Dolphin's own Exit hotkey (graceful stop, then exit)

Dolphin has no "wait 150 ms, then decide" primitive, so the chord resolver is expressed with
hold() and pulse() (pinned fork, FunctionExpression.cpp). Differences from the contract text:
  * A click held alone starts Start/Select 150 ms after the press (hold) and releases with the
    click; a tap shorter than 150 ms is sent as a 100 ms press when the click is released.
  * After a chord, Start/Select stay blocked for 1.5 s (3.5 s after a hold of 1 s or more), so
    the click released second never sends a stray press.

Usage: universal_controls.py [--write]   (default: check every profile in this repo)
"""
import re
import sys
from pathlib import Path

HERE = Path(__file__).resolve().parent
ROOT = HERE.parents[2]

L = '`Left Button Thumbstick`'
R = '`Right Button Thumbstick`'
Y = '`Left Button Y`'
X = '`Left Button X`'
DEVICE = 'OpenXR/0/OpenXR Controller'

CHORD_WAIT = 0.15      # s: a lone click waits this long before it counts as Start/Select
CHORD_PAIR = 0.25      # s: the second click must follow the first within this
CHORD_RELEASE = 1.0    # s: U3 fires only if both are released within this
QUIT_HOLD = 3.0        # s: U4
HOME_HOLD = 0.5        # s: Wii Home (hold left Y)

# True while a chord is, or has just been, in progress.
CHORD_MASK = f'(pulse({L} & {R}, 1.5) | pulse(!hold({L} & {R}, {CHORD_RELEASE}), 3.5))'


def _click(this, other):
    return (f'(hold({this} & !{other}, {CHORD_WAIT}) | pulse(!{this}, 0.1)) & !{CHORD_MASK}')


START = _click(L, R)
SELECT = _click(R, L)
# Remote 2 on the left Y tap (pulse on release), Home on the 0.5 s hold. Holding Y sends no 2.
REMOTE_TWO = f'pulse(!{Y}, 0.12) & !pulse(!hold({Y}, {HOME_HOLD}), 0.3)'
HOME = f'hold({Y}, {HOME_HOLD})'
# GameCube Z: left X and left Y together; X and Y alone stay X and Y.
GC_X = f'{X} & !{Y}'
GC_Y = f'{Y} & !{X}'
GC_Z = f'{X} & {Y}'

# U3: second click within 250 ms of the first, then both released within 1 s.
_PAIRED = f'pulse({L}, {CHORD_PAIR}) & pulse({R}, {CHORD_PAIR}) & {L} & {R}'
RECENTRE = f'pulse(!{L} & !{R}, 0.1) & pulse({_PAIRED}, {CHORD_RELEASE})'
QUIT = f'hold({L} & {R}, {QUIT_HOLD})'


def qualify(expression):
    """Hotkeys.ini binds the keyboard device, so every input names the OpenXR device."""
    return re.sub(r'`([^`]*)`', lambda m: f'`{DEVICE}:{m.group(1)}`', expression)


HOTKEYS = {
    'VR/Reset VR Position': '@(Shift+R) | (' + qualify(RECENTRE) + ')',
    'General/Exit': qualify(QUIT),
}

# Lines in the Wii Remote profiles that carry the universal layer, by profile key.
WII_KEYS = {'Buttons/+': START, 'Buttons/-': SELECT, 'Buttons/Home': HOME}
OLD_WII_TWO = '`Right Button Thumbstick`'


def wii_profile_files():
    base = HERE
    files = [base / 'WiimoteNew.ini', base / 'gun' / 'WiimoteNew.ini',
             ROOT / 'setups/dolphin-vr/crossbow-training/AC Crossbow Candidate.ini']
    files += sorted((base / 'Profiles/Wiimote').glob('*.ini'))
    return files


def gc_profile_files():
    base = ROOT / 'setups/dolphin-vr/gamecube'
    return [base / 'GCPadNew.ini', base / 'GCPadNew-roll-steer.ini',
            *sorted((base / 'Profiles/GCPad').glob('*.ini'))]


def _set(lines, key, value):
    out, hit = [], False
    for line in lines:
        if line.split('=', 1)[0].strip() == key and '=' in line:
            out.append(f'{key} = {value}' if value is not None else f'{key} =')
            hit = True
        else:
            out.append(line)
    return out, hit


def expected_wii(text):
    """Return the profile text with the universal layer applied."""
    lines = text.split('\n')
    kv = {l.split('=', 1)[0].strip(): l.split('=', 1)[1].strip() for l in lines if '=' in l}
    for key, value in WII_KEYS.items():
        lines, hit = _set(lines, key, value)
        if not hit:
            raise ValueError(f'profile has no {key}')
    # Remote 2 moved from the right stick click to the left Y tap (never the pad profiles that
    # already give 2 its own button).
    if kv.get('Buttons/2') in (OLD_WII_TWO, REMOTE_TWO):
        lines, _ = _set(lines, 'Buttons/2', REMOTE_TWO)
    # Resort's nunchuk toggle used the left click: left X + left Y together instead.
    lines = [re.sub(r'^Extension = toggle\(.*\)$', f'Extension = toggle({X} & {Y})', l) for l in lines]
    return '\n'.join(lines)


def expected_gc(text):
    lines = text.split('\n')
    changes = {
        'Buttons/X': GC_X, 'Buttons/Y': GC_Y, 'Buttons/Z': GC_Z, 'Buttons/Start': START,
        'Main Stick/Up': '`Left Thumbstick Y+`', 'Main Stick/Down': '`Left Thumbstick Y-`',
        'D-Pad/Up': None, 'D-Pad/Down': None, 'D-Pad/Left': None, 'D-Pad/Right': None,
    }
    for key, value in changes.items():
        lines, hit = _set(lines, key, value)
        if not hit:
            raise ValueError(f'profile has no {key}')
    for key in ('Main Stick/Left', 'Main Stick/Right'):
        for i, line in enumerate(lines):
            if line.startswith(key + ' ='):
                lines[i] = line.replace(' & !' + L, '')
    return '\n'.join(lines)


def hotkeys_text():
    lines = ['[Hotkeys]', 'Device = DInput/0/Keyboard Mouse']
    lines += [f'{key} = {value}' for key, value in HOTKEYS.items()]
    return '\n'.join(lines) + '\n'


def main(argv):
    write = '--write' in argv
    bad = []
    hotkeys = HERE.parent / 'templates' / 'Hotkeys.ini'
    if not hotkeys.is_file() or hotkeys.read_bytes().decode('utf-8') != hotkeys_text():
        if write:
            hotkeys.write_bytes(hotkeys_text().encode('utf-8'))
        else:
            bad.append(str(hotkeys))
    for files, fn in ((wii_profile_files(), expected_wii), (gc_profile_files(), expected_gc)):
        for path in files:
            raw = path.read_bytes()
            text = raw.decode('utf-8')
            eol = '\r\n' if '\r\n' in text else '\n'
            norm = text.replace('\r\n', '\n')
            want = fn(norm)
            if want != norm:
                if write:
                    path.write_bytes(want.replace('\n', eol).encode('utf-8'))
                else:
                    bad.append(str(path))
    if bad:
        print('universal controls not applied in:\n  ' + '\n  '.join(bad))
        return 1
    print('universal controls: all Wii Remote and GameCube profiles match')
    return 0


if __name__ == '__main__':
    sys.exit(main(sys.argv[1:]))
