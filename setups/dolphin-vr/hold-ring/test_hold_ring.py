#!/usr/bin/env python
"""The DolphinXR hold ring must follow Dolphin's own Exit hotkey, or the ring and the quit disagree.

Checks, with the standard library only (no Dolphin source, no compiler):
  - Hotkeys.ini (template and generator) quits on a plain hold of both stick clicks, with no chord window,
    which is what acvr::BothHeldProgress counts;
  - that hold lasts as long as the ring's quit time (libacvr hold_ring.hpp, HoldRingTiming::quit_ms);
  - apply.py reads both clicks from the OpenXR input and paints after the eye blit and the flat blit.
"""
import re
import sys
from pathlib import Path

HERE = Path(__file__).resolve().parent
ROOT = HERE.parents[2]
sys.path.insert(0, str(HERE.parent / "motion-controls"))
import universal_controls  # noqa: E402


def main():
    ring = (ROOT / "libacvr/src/hold_ring.hpp").read_text(encoding="utf-8")
    quit_ms = int(re.search(r"uint32_t quit_ms = (\d+);", ring).group(1))
    assert universal_controls.QUIT_HOLD * 1000 == quit_ms, "Exit hotkey hold and ring quit time differ"
    host = (ROOT / "libacvr/src/hold_ring_host.hpp").read_text(encoding="utf-8")
    assert "BothHeldProgress(uint32_t hold_ms = HoldRingTiming{}.quit_ms)" in host

    L, R = universal_controls.L, universal_controls.R
    assert universal_controls.QUIT == f"hold({L} & {R}, {universal_controls.QUIT_HOLD})", universal_controls.QUIT
    hotkeys = (HERE.parent / "templates/Hotkeys.ini").read_text(encoding="utf-8")
    exit_line = next(line for line in hotkeys.splitlines() if line.startswith("General/Exit"))
    assert exit_line == "General/Exit = " + universal_controls.qualify(universal_controls.QUIT), exit_line

    apply = (HERE / "apply.py").read_text(encoding="utf-8")
    for needle in ("input.controllers[0].thumbstick_button && input.controllers[1].thumbstick_button",
                   "s_ring.update(now, s_hold.update(now, both))",
                   "ACVREyeSpot(eye, eye_rect.GetWidth(), eye_rect.GetHeight())",
                   "acvr::hold_ring_screen_spot(flat_rect.GetWidth(), flat_rect.GetHeight())",
                   "context1->ClearView(rtv, color, &box, 1);"):
        assert needle in apply, needle
    print(f"PASS dolphin hold ring: follows General/Exit = hold(both clicks, {universal_controls.QUIT_HOLD} s), "
          f"ring quit at {quit_ms} ms, drawn after the eye and flat blits")
    return 0


if __name__ == "__main__":
    sys.exit(main())
