# DolphinXR U4 hold ring

Holding both stick clicks quits Dolphin after 3 seconds (U4, Dolphin's own `General/Exit` hotkey in
`../templates/Hotkeys.ini`). This patch shows the same ring as every other AladdinsCastle host while the
clicks are held (libacvr `hold_ring.hpp`, as in acvr-ps1):

- nothing for the first second, so a recentre never flashes it
- a ring that fills clockwise from 1 s to 3 s
- one flash at 3 s, as the Exit fires
- gone as soon as either click lets go

The ring counts the same thing as the hotkey, `hold(left click & right click, 3.0)`, read from the same
OpenXR input, so it cannot disagree with the quit. The quit itself is unchanged, so Dolphin may stop
before the flash has fully played.

| File | Change |
|---|---|
| `VideoCommon/VR/ACVR/*.hpp` | copies of `libacvr/src/stick_chord.hpp`, `hold_ring.hpp`, `hold_ring_gl.hpp`, `hold_ring_host.hpp` |
| `VideoCommon/VR/OpenXRManager.h` | `IOpenXRSwapchain::FillEyeRects`: paint rectangles into an acquired eye image (default: nothing) |
| `VideoBackends/D3D/D3DOpenXR.h/.cpp` | `FillEyeRects` with D3D11.1 `ClearView`; display colours turned back to linear on an sRGB view |
| `VideoCommon/Present.cpp` | after each eye blit, the ring straight ahead in that eye; after the flat-screen blit, in the middle. While the ring shows, the per-eye path runs instead of the layered one |

Only the Direct3D 11 backend draws the ring (the backend this repository builds and the owner uses).
Vulkan, Direct3D 12 and OpenGL keep the default `FillEyeRects`, which draws nothing. In flat-screen mode
the picture is stretched onto the panel, so the ring is stretched with it.

Apply after the other deltas, as `.github/workflows/compile-dolphin.yml` does:

```
python -B setups/dolphin-vr/hold-ring/apply.py <private DolphinXR clone under .local, at the pin> [--revert]
```

Checks: `test_hold_ring.py` (the ring and the Exit hotkey count the same hold), the libacvr
`acvr_hold_ring_host` test, and the Dolphin compile check in CI. Not tried in a headset yet.
