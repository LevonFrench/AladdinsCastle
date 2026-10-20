#!/usr/bin/env python
"""Add (or remove) the env-gated offline input probe in a private DolphinXR source clone.

Usage: python -B apply.py <absolute-private-source-path> [--revert]

Copies ACInputProbe.h to Source/Core/Core/ and adds two guarded calls to
Source/Core/Core/IOS/USB/Bluetooth/BTEmu.cpp. Without the DOLPHINXR_INPUT_SCRIPT and
DOLPHINXR_INPUT_PROBE environment variables the calls do nothing. Nothing is downloaded.
"""
import argparse
import shutil
from pathlib import Path

HERE = Path(__file__).resolve().parent
BTEMU = Path('Source/Core/Core/IOS/USB/Bluetooth/BTEmu.cpp')
HEADER = Path('Source/Core/Core/ACInputProbe.h')

EDITS = [
    ('#include "Core/HW/WiimoteEmu/DesiredWiimoteState.h"\n',
     '#include "Core/HW/WiimoteEmu/DesiredWiimoteState.h"\n#include "Core/ACInputProbe.h"\n'),
    ('    core_timing.Throttle(now);\n',
     '    core_timing.Throttle(now);\n'
     '#ifdef ENABLE_VR\n'
     '    const double ac_probe_t =\n'
     '        double(now) / double(GetSystem().GetSystemTimers().GetTicksPerSecond());\n'
     '    ACInputProbe::Feed(ac_probe_t);\n'
     '#endif\n'),
    ('      next_call[i] = m_wiimotes[i]->PrepareInput(&wiimote_states[i]);\n',
     '      next_call[i] = m_wiimotes[i]->PrepareInput(&wiimote_states[i]);\n'
     '#ifdef ENABLE_VR\n'
     '    ACInputProbe::Record(ac_probe_t, wiimote_states[0]);\n'
     '#endif\n'),
]


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('source')
    ap.add_argument('--revert', action='store_true')
    args = ap.parse_args()
    root = Path(args.source)
    target = root / BTEMU
    raw = target.read_bytes().decode('utf-8')
    crlf = '\r\n' in raw
    text = raw.replace('\r\n', '\n')
    for old, new in EDITS:
        if args.revert:
            if new in text:
                text = text.replace(new, old)
            continue
        if new in text:
            continue  # already applied
        if text.count(old) != 1:
            raise SystemExit('anchor missing or ambiguous: ' + old.strip()[:80])
        text = text.replace(old, new)
    if crlf:
        text = text.replace('\n', '\r\n')
    target.write_bytes(text.encode('utf-8'))
    header = root / HEADER
    if args.revert:
        header.unlink(missing_ok=True)
    else:
        shutil.copyfile(HERE / 'ACInputProbe.h', header)
    print(('reverted ' if args.revert else 'applied ') + str(target))


if __name__ == '__main__':
    main()
