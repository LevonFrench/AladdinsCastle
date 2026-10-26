# vpx-vr: hold-to-quit for Visual Pinball X in the headset

A small patch for [Visual Pinball X](https://github.com/vpinball/vpinball) (VPX) that adds the universal quit
(U4 in [docs/controls.md](../../docs/controls.md)) inside a pinball table:

- **Hold both stick clicks for 3 seconds, hands still:** VPX closes the table through its own Exit Game action, the
  same as pressing Esc. The table script's exit runs, PinMAME writes its NVRAM, and VPX saves its settings, as on any
  other exit. Started from the Hub (`-Play`), VPX then closes and you are back at the Hub.
- **Hands moving:** in pinball the same held clicks let your hands scale, turn and move the table (U3p). If either
  hand travels more than 5 cm from where it was when the hold began, that hold does not quit. Let go of both clicks
  and the next hold starts fresh.
- **Countdown:** from 1 second the headset shows "Keep holding both stick clicks to quit: 2 s", then "Quitting and
  saving...". A cancelled hold shows "Quit cancelled: your hands moved the table". The VPX log gets `[ACVR]` lines.

It contains no table, ROM or built program. CI builds the program from VPX's source with this patch applied.
The VPX source files this patch edits are marked GPL v3 or later, and so is everything in this folder. The shared
chord header from libacvr is MIT.

## Base

| | |
|---|---|
| Upstream | `https://github.com/vpinball/vpinball`, tag `v10.8.1-5436-af26b2d93` (commit `af26b2d9392b4c9ec3e3edff4bbd69562085a44c`) |
| Why this one | The owner's `VPinballX_BGFX64.exe` reports FileVersion 10.8.1.5436 and ProductVersion 10.8.1.5436.af26b2d93 (version information only; it was not run) |
| Build | Windows x64, BGFX, Release, Visual Studio 2026. OpenXR is only in the Windows BGFX program |
| Patched build's version | FileVersion 10.8.1.5436, ProductVersion **10.8.1.5436.af26b2d93-acvr** (so you can tell the two apart) |

## Files

| File | What it is |
|---|---|
| `patch/apply_acvr.py` | Run from the VPX source root. Copies the two headers below into `src/input/acvr/`, edits `src/input/XRInputHandler.h`, `src/renderer/VRDevice.cpp` and `src/core/git_version.h`. Every edit checks its anchor and stops if the source has moved |
| `patch/acvr/HoldToQuit.h` | The hold logic: libacvr's chord timing plus the 5 cm hand-travel cancel. No OpenXR, no VPX |
| `../../libacvr/src/stick_chord.hpp` | The chord timing every AladdinsCastle host shares (150 ms window for the second click, quit at 3 s). Copied in unchanged at build time, not duplicated here |
| `tests/hold_to_quit_test.cpp` | CPU-only test of the hold logic: still hold quits at 3 s, jitter under 5 cm still quits, 6 cm cancels, a late move still cancels, late second click is not a chord, untracked hands still quit, one quit per hold |
| `upstream-pin.json` | The pinned tag and commit |

How it fits into VPX: the OpenXR input handler (`XRInputHandler`) reads both stick clicks and both grip poses every
input step, in a room-fixed (LOCAL) space at the last frame's display time. When the hold reaches 3 s it presses
VPX's Exit Game action for one step through the action's own "direct state" slot (the same mechanism VPX uses for
touch input and auto-start), so the exit path is VPX's own. The renderer passes the display time in and shows the
countdown as a VPX notification.

## CI

- `.github/workflows/compile-vpx.yml` fetches the pinned tag, applies the patch, builds VPX's external libraries with
  upstream's own `platforms/windows-x64/external.sh` (cached), and compiles `VPinballX_BGFX64.exe`. It checks the
  version information and that the patch's strings are in the program, then keeps it as the artifact
  `vpx-acvr-10.8.1.5436-hold-to-quit` (`VPinballX_BGFX64_acvr.exe`, `SHA256SUMS.txt`, this README) for 90 days.
  Nothing is run.
- `.github/workflows/patch-apply.yml` checks that the patch still applies to the pinned commit.
- `.github/workflows/ci.yml` compiles and runs `tests/hold_to_quit_test.cpp` with g++.

## Install (by hand; the Hub does not do it)

1. From the latest green "VPX hold-to-quit compile" run, download the artifact `vpx-acvr-10.8.1.5436-hold-to-quit`
   (Actions page, or `gh run download <run id> -n vpx-acvr-10.8.1.5436-hold-to-quit -D <a new folder>`).
2. Check the hash: `Get-FileHash VPinballX_BGFX64_acvr.exe` must match `SHA256SUMS.txt`.
3. Copy `VPinballX_BGFX64_acvr.exe` into your VPX folder, **beside** `VPinballX_BGFX64.exe`. Do not replace the
   original. It uses the same DLLs, plugins, settings and tables, because it is the same VPX version.
4. In the Hub, set the pinball program to `VPinballX_BGFX64_acvr.exe`.

Undo: set the Hub's pinball program back to `VPinballX_BGFX64.exe` and delete the `_acvr` file.

## Limits

- Start and Coin still fire the moment a stick click goes down (they are plain VPX bindings), so a chord can send a
  short Start or Coin first. This patch adds only the quit.
- The 5 cm limit is straight-line travel of either hand from where it was when the chord began. If the hands are not
  tracked, travel cannot be measured and the hold quits as on every other system.
- If VPX's own in-game menu is open, Exit Game steps back in the menu instead of quitting (VPX's own behaviour).
- Not tried in a headset yet.
