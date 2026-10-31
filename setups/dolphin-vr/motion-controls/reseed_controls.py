#!/usr/bin/env python
"""Bring an existing DolphinXR user folder up to the universal controls (U3 recentre, U4 quit).

The seeding steps (install recipes, the sitting tool) only write Hotkeys.ini and the stop
setting into a FRESH user folder. An older folder keeps what it had. This merges the missing
pieces in, and touches nothing else:

  Config/Hotkeys.ini   [Hotkeys] Device                 added when missing
                       [Hotkeys] VR/Reset VR Position   the both-click recentre chord (150 ms pair window)
                       [Hotkeys] General/Exit           the both-click 3 s hold that quits and saves
  Config/Dolphin.ini   [Interface] ConfirmStop = False  (otherwise Exit waits on a message box)
                       [General] HotkeysRequireFocus = False  added only when missing (the headset
                                                       window is not the focused one)

Rules for a hotkey line:
  * missing or empty                     -> the universal value is written.
  * already carries the chord            -> left alone (running it twice changes nothing).
  * an older shipped value (250 ms pair) -> upgraded to the current one.
  * a binding of the user's own          -> kept; the chord is OR-ed in so the user's key still works.
Every other line, section, comment, line ending (CRLF or LF) and byte-order mark stays as it was.
A file that would change is first copied once to <name>.before-universal-controls; an existing
backup is never overwritten, so the first copy stays the true original.

Standard library only.  Usage:
  reseed_controls.py USER_DIR [USER_DIR ...]          show what would change (no files written)
  reseed_controls.py --apply USER_DIR [USER_DIR ...]  write the changes (with backups)
USER_DIR is a Dolphin user folder (the one that holds Config/). GameSettings, GCPadNew.ini and
WiimoteNew.ini are never opened.
"""
import os
import re
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import universal_controls as uc  # noqa: E402

BACKUP_SUFFIX = '.before-universal-controls'

_DEV = uc.DEVICE
_L = f'`{_DEV}:Left Button Thumbstick`'
_R = f'`{_DEV}:Right Button Thumbstick`'

# Current universal values, from the single source in universal_controls.py.
RESET_BASE = '@(Shift+R)'
RESET_CHORD = uc.qualify(uc.RECENTRE)
EXIT_CHORD = uc.qualify(uc.QUIT)
HOTKEY_DEVICE = 'DInput/0/Keyboard Mouse'

# The 250 ms pair window shipped before the contract fixed it at 150 ms.
STALE_RESET_CHORDS = (
    RESET_CHORD.replace(f', {uc.CHORD_PAIR:g})', ', 0.25)'),
)


class Ini:
    """A file as lines, each keeping its own line ending (a file can mix CRLF and LF)."""

    def __init__(self, text):
        self.lines, self.ends = [], []
        for part in re.findall(r'[^\n]*\n|[^\n]+', text):
            end = '\r\n' if part.endswith('\r\n') else '\n' if part.endswith('\n') else ''
            self.lines.append(part[:len(part) - len(end)])
            self.ends.append(end)
        self.default = '\r\n' if text.count('\r\n') * 2 > text.count('\n') else '\n'

    def text(self):
        return ''.join(a + b for a, b in zip(self.lines, self.ends))

    def bounds(self, section):
        """(start, end) of the lines belonging to [section]; start is the header index. None if absent."""
        start = None
        for i, line in enumerate(self.lines):
            stripped = line.strip()
            if stripped.startswith('[') and stripped.endswith(']'):
                if start is not None:
                    return start, i
                if stripped == f'[{section}]':
                    start = i
        return (start, len(self.lines)) if start is not None else None

    def _find(self, bounds, key):
        for i in range(bounds[0] + 1, bounds[1]):
            left, eq, _ = self.lines[i].partition('=')
            if eq and left.strip() == key:
                return i
        return None

    def get(self, section, key):
        """The value, or None when the section or the key is missing."""
        bounds = self.bounds(section)
        i = None if bounds is None else self._find(bounds, key)
        return None if i is None else self.lines[i].partition('=')[2].strip()

    def _insert(self, at, line):
        if at >= len(self.lines):
            # Appending: a last line without an ending gets one; the new last line takes its place.
            tail = self.ends[-1] if self.ends else ''
            if self.ends and not tail:
                self.ends[-1] = self.default
            self.lines.append(line)
            self.ends.append(tail if self.ends else self.default)
        else:
            self.lines.insert(at, line)
            self.ends.insert(at, self.ends[at - 1] if at > 0 and self.ends[at - 1] else self.default)

    def put(self, section, key, value):
        """Set key in section (adding the section or key when missing). Other lines are untouched."""
        line = f'{key} = {value}'
        bounds = self.bounds(section)
        if bounds is None:
            self._insert(len(self.lines), f'[{section}]')
            self._insert(len(self.lines), line)
            return
        i = self._find(bounds, key)
        if i is not None:
            self.lines[i] = line
            return
        start, end = bounds
        at = end
        while at > start + 1 and not self.lines[at - 1].strip():
            at -= 1   # after the last non-blank line of the section
        self._insert(at, line)


def merge_hotkey(existing, base, chord, stale_chords=()):
    """The new value for one hotkey line, or None when the existing line is already right."""
    current = (existing or '').strip()
    if not current:
        return f'{base} | ({chord})' if base else chord
    if chord in current:
        return None
    for old in stale_chords:
        if old in current:
            return current.replace(old, chord)
    if base and current == base:
        return f'{base} | ({chord})'
    return f'({current}) | ({chord})'


def merge_hotkeys_text(text):
    """Return (new_text, changes). changes is a list of (key, 'added'|'upgraded'|'chord added')."""
    changes = []
    ini = Ini(text)
    if ini.get('Hotkeys', 'Device') in (None, ''):
        ini.put('Hotkeys', 'Device', HOTKEY_DEVICE)
        changes.append(('Device', 'added'))
    for key, base, chord, stale in (('VR/Reset VR Position', RESET_BASE, RESET_CHORD, STALE_RESET_CHORDS),
                                    ('General/Exit', None, EXIT_CHORD, ())):
        existing = ini.get('Hotkeys', key)
        new = merge_hotkey(existing, base, chord, stale)
        if new is None:
            continue
        kind = ('added' if not (existing or '').strip()
                else 'upgraded' if any(s in existing for s in stale) else 'chord added')
        ini.put('Hotkeys', key, new)
        changes.append((key, kind))
    return (ini.text() if changes else text), changes


def merge_dolphin_text(text):
    changes = []
    ini = Ini(text)
    if (ini.get('Interface', 'ConfirmStop') or '').lower() != 'false':
        was = ini.get('Interface', 'ConfirmStop')
        ini.put('Interface', 'ConfirmStop', 'False')
        changes.append(('Interface/ConfirmStop', 'added' if was is None else 'set to False'))
    if ini.get('General', 'HotkeysRequireFocus') is None:
        ini.put('General', 'HotkeysRequireFocus', 'False')
        changes.append(('General/HotkeysRequireFocus', 'added'))
    return (ini.text() if changes else text), changes


def _decode(raw):
    bom = raw.startswith(b'\xef\xbb\xbf')
    return raw[3:].decode('utf-8') if bom else raw.decode('utf-8'), bom


def _encode(text, bom):
    return (b'\xef\xbb\xbf' if bom else b'') + text.encode('utf-8')


def _write_atomic(path, data):
    tmp = path.with_name(path.name + '.tmp-reseed')
    tmp.write_bytes(data)
    os.replace(tmp, path)


def reseed_file(path, merge, create_text=None, apply=False):
    """Merge one file. Returns (state, changes); state is 'created', 'changed' or 'unchanged'
    (or 'missing' when the file is absent and create_text is None)."""
    path = Path(path)
    if not path.is_file():
        if create_text is None:
            return 'missing', []
        new, changes = merge(create_text)
        if apply:
            path.parent.mkdir(parents=True, exist_ok=True)
            _write_atomic(path, _encode(new, False))
        return 'created', changes
    raw = path.read_bytes()
    text, bom = _decode(raw)
    new, changes = merge(text)
    if not changes:
        return 'unchanged', []
    if apply:
        backup = path.with_name(path.name + BACKUP_SUFFIX)
        if not backup.exists():
            backup.write_bytes(raw)
        _write_atomic(path, _encode(new, bom))
    return 'changed', changes


def reseed_user_folder(user_dir, apply=False):
    """Reseed one Dolphin user folder. Returns {file name: (state, changes)}."""
    config = Path(user_dir) / 'Config'
    if not config.is_dir():
        raise FileNotFoundError(f'no Config folder in {user_dir}')
    return {
        'Hotkeys.ini': reseed_file(config / 'Hotkeys.ini', merge_hotkeys_text, create_text='[Hotkeys]\n', apply=apply),
        'Dolphin.ini': reseed_file(config / 'Dolphin.ini', merge_dolphin_text, apply=apply),
    }


def main(argv):
    apply = '--apply' in argv
    folders = [a for a in argv if not a.startswith('--')]
    if not folders:
        print(__doc__)
        return 2
    status = 0
    for folder in folders:
        try:
            result = reseed_user_folder(folder, apply)
        except (OSError, UnicodeDecodeError) as error:
            print(f'{folder}: {error}')
            status = 1
            continue
        print(f'{folder}  ({"applied" if apply else "dry run"})')
        for name, (state, changes) in result.items():
            detail = ', '.join(f'{k} {v}' for k, v in changes) or '-'
            print(f'  {name}: {state}: {detail}')
    return status


if __name__ == '__main__':
    sys.exit(main(sys.argv[1:]))
