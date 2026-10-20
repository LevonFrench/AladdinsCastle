#!/usr/bin/env python
"""Add (or remove) the env-gated offline GameCube pad probe in a private DolphinXR source clone.

Usage: python -B apply_gc.py <absolute-private-source-path> [--revert]

Copies ACPadProbe.h to Source/Core/Core/ and adds guarded calls to
Source/Core/Core/HW/SI/SI.cpp (feed the scripted OpenXR state before the SI input update) and
Source/Core/Core/HW/SI/SI_DeviceGCController.cpp (log pad 1's status). Without the
DOLPHINXR_INPUT_SCRIPT and DOLPHINXR_INPUT_PROBE environment variables the calls do nothing, and
they never run for Wii titles. Nothing is downloaded. Can be combined with the Wii Remote probe
(../../motion-controls/input-probe/apply.py).
"""
import argparse
import shutil
from pathlib import Path

HERE = Path(__file__).resolve().parent
HEADER = Path('Source/Core/Core/ACPadProbe.h')
TICKS = ('double(m_system.GetCoreTiming().GetTicks()) / '
         'double(m_system.GetSystemTimers().GetTicksPerSecond())')

FILES = {
    Path('Source/Core/Core/HW/SI/SI.cpp'): [
        ('#include "Core/HW/SI/SI.h"\n',
         '#include "Core/HW/SI/SI.h"\n#include "Core/ACPadProbe.h"\n'),
        ('  g_controller_interface.SetCurrentInputChannel(ciface::InputChannel::SerialInterface);\n',
         '#ifdef ENABLE_VR\n'
         '  ACPadProbe::Feed(' + TICKS + ');\n'
         '#endif\n'
         '  g_controller_interface.SetCurrentInputChannel(ciface::InputChannel::SerialInterface);\n'),
    ],
    Path('Source/Core/Core/HW/SI/SI_DeviceGCController.cpp'): [
        ('#include "Core/HW/SI/SI_DeviceGCController.h"\n',
         '#include "Core/HW/SI/SI_DeviceGCController.h"\n#include "Core/ACPadProbe.h"\n'),
        ('  HandleMoviePadStatus(m_system.GetMovie(), m_device_number, &pad_status);\n',
         '  HandleMoviePadStatus(m_system.GetMovie(), m_device_number, &pad_status);\n'
         '#ifdef ENABLE_VR\n'
         '  if (m_device_number == 0)\n'
         '    ACPadProbe::Record(' + TICKS + ', pad_status);\n'
         '#endif\n'),
    ],
}


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('source')
    ap.add_argument('--revert', action='store_true')
    args = ap.parse_args()
    root = Path(args.source)
    for rel, edits in FILES.items():
        target = root / rel
        raw = target.read_bytes().decode('utf-8')
        crlf = '\r\n' in raw
        text = raw.replace('\r\n', '\n')
        for old, new in edits:
            if args.revert:
                if new in text:
                    text = text.replace(new, old)
                continue
            if new in text:
                continue  # already applied
            if text.count(old) != 1:
                raise SystemExit('anchor missing or ambiguous in %s: %s' % (rel, old.strip()[:80]))
            text = text.replace(old, new)
        if crlf:
            text = text.replace('\n', '\r\n')
        target.write_bytes(text.encode('utf-8'))
        print(('reverted ' if args.revert else 'applied ') + str(target))
    header = root / HEADER
    if args.revert:
        header.unlink(missing_ok=True)
    else:
        shutil.copyfile(HERE / 'ACPadProbe.h', header)


if __name__ == '__main__':
    main()
