# MAME Model 2 stereo work

Base: MAME commit `9a119fe94494433fb49b3a05ae7c38318bf20055` (0.289).

Apply `model2-stereo.patch` to that source checkout. The current patch adds
Windows audio property-key definitions to the OSD library and links `ksguid`
after MAME's objects. It also contains the stereo renderer in development.
With `ACVR_M2_STEREO` unset, the original screen and renderer remain selected.

The small target built successfully using MINGW64 with:

```sh
make -j2 SUBTARGET=model2 SOURCES=src/mame/sega/model2.cpp PTR64=1 \
  MINGW64=/mingw64 BUILDDIR=build-mingw64 REGENIE=1 OPTIMIZE=2 SYMBOLS=0 \
  NOWERROR=1 NO_USE_MIDI=1 NO_OPENGL=1 USE_SDL=0 USE_SDL3=0
```

Step 1 passed: a timed, windowless attract-mode run saved MAME's snapshot.
That run used an owner-authorized, development-only zero-filled billboard ROM
substitute. It produced the expected wrong-checksum warning. That substitute
and all game files, pictures, logs and build products remain private.

Steps 2-4 passed with MAME's own timed snapshots: a side-by-side pair, an
approximately calibrated pair and two additional games with the same code.
A logical live render target publishes the stereo screen even with video
disabled. It does not create an operating-system window. For a pair snapshot,
select `-snapview "Screen 1"` with stereo enabled.

The code retains view-space polygons and clips/projects them separately for
parallel eyes. It caches both scenes without advancing emulation twice and
duplicates the background and foreground tile layers. Arcade timing remains
on the original screen; a separate presentation screen assembles the pair.

Settings read from the process environment are `ACVR_M2_STEREO` (0/1),
`ACVR_M2_IPD_M` (default 0.064 metres), `ACVR_M2_UNITS_PER_M` (default 1), and
`ACVR_M2_ZERO_M` (default 4 metres). An upright character measured 1.66142726
game units from retained view-space vertices. Assuming 1.75 m height gives
an approximate calibration of 0.949387005 units/m. Pose, assumed stature and
mesh selection limit its accuracy; the additional games' metric scales were
not independently measured. Runtime profiles and measurement logs stay private.

Step 5's viewer is built and ready for the owner's headset test. The optional
`ACVR_M2_STREAM=1` bitmap publisher sends both 496x384 eye images and the main
viewport's field-of-view tangents to a separate viewer through shared memory.
It drops a publication when the reader is busy, rather than blocking emulation.
The viewer flips bitmap rows for OpenGL, uploads both textures using its own
hidden context, and uses the proven presenter's separate OpenXR thread and
two eye-specific quad layers. The quad size follows the game's focal lengths
at the zero-parallax distance. Initial placement and Home use current gaze;
Esc requests a graceful emulator exit. Owner launchers are outside the repository.

The final transport/upload test received 1,263 pairs and passed a GPU texture
readback comparison with OpenXR disabled. The viewer and its PowerShell 5.1
build script compiled successfully. Headset visibility, fusion, colour and
Home behavior have not been tested; owner acceptance remains pending.

The `presenter` directory is a separate GPL-3.0-or-later program adapted from
the owner-provided ACVR screen presenter. It is not linked into MAME. Its
Win32/WGL adapter replaces SDL/GLEW dependencies; no additional installation
was needed. `build-viewer.ps1` takes a patched MAME source directory, local
OpenXR include directory and a private output directory. Keep the existing
OpenXR loader DLL beside the resulting viewer executable. Header/loader copies
and all executable products remain outside the repository.

Step 6 is report-only: see `HEAD-TRACKING.md` for the needed geometry/clip,
game submission and late-redraw changes, and Virtua Cop's gun input path.

`stereo_math_test.cpp` tests numeric near/far disparity, convergence, eye
symmetry, zero/double separation and equivalent world-unit scales against
the patched `model2_stereo.h`. Compile with that header's directory on the
include path, and keep the executable outside the repository.

MAME as a whole is GPL-2.0; the new MAME-side source files are BSD-3-Clause.
The source checkout's `COPYING` and `docs/legal` retain the upstream licence
texts and copyright notices. The licence texts are included here as well.
