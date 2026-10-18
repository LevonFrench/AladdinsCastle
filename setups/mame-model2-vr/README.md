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

For Hub-owned clean shutdown, set `ACVR_M2_STOP_FILE` to an absolute, unique
file path for that launch. Create its parent directory and remove a stale file
before starting the viewer; to stop, create a regular file at that exact path.
Its contents do not matter. The viewer also honors a file created during
startup, leaves the file in place, and logs the resolved absolute path. If the
variable is absent, the default is `model2-<MAME-PID>.stop` in the viewer's
working directory. The private game launchers use `runtime/stop/<game>.stop`
under their private Model 2 work directory and clear it before each launch.

The viewer sets the existing shared-memory exit request, which MAME processes
through `machine().schedule_exit()`. It releases controller inputs and waits
for the MAME process to exit, allowing normal NVRAM/settings persistence before
the viewer exits. The Hub should wait for that exit instead of force-closing
either process when requesting a normal stop. If MAME has not exited within
30 seconds after the request, the viewer reports failure without terminating
MAME; the launcher's existing bounded failure path remains a last resort.
Escape and the viewer's own time limit use this same clean request path. Only the
diagnostic runs (`--scripted-input`, `--no-xr`) have a time limit; a game in the
headset runs until it is quit.

Holding left Y on Touch for three continuous real-time seconds requests the
same clean exit. The corresponding binding on Index is left B, its upper face
button (that controller has no Y-labelled button). Both hands' actions are
synchronized; existing right-hand Start/Coin/shoot mappings remain intact.
Release, lost focus or stale XR input resets an incomplete hold. A completed
request stays latched. `--scripted-input --scripted-y-hold` is the explicit
headset-free proof switch; it synthesizes the hold after 65 emulated seconds,
using the production three-second timer and exit path.

Hidden save proofs passed for both stop-file and scripted-Y requests. Each
run sent a real Coin input and set an explicitly scripted high-score entry in
emulated backup RAM; the test never wrote the save file itself. MAME's normal
exit persisted the coin audit and scripted score, changed the NVRAM timestamp
and contents, and exited successfully before the viewer. The stop-file run
changed 30 bytes; the hold run changed 25. The hold run first cold-loaded the
stop-file run's saved score and audit successfully. A separate cold boot also
reloaded the hold run's final score, coin audit and initials. The test scores are labelled
data for persistence testing, not earned gameplay scores. The original play
saves, proof Lua scripts, byte comparisons, logs and MAME snapshots stay private.
Timer, interrupted hold, both-hand action synchronization, binding and focus-loss
checks also passed without starting an XR runtime. Physical Y/Index-button
operation still requires the headset.

The final transport/upload test received 1,263 pairs and passed a GPU texture
readback comparison with OpenXR disabled. The viewer and its PowerShell 5.1
build script compiled successfully. Headset visibility, fusion, colour and
Home behavior have not been tested; owner acceptance remains pending.

The viewer now has a right-hand action set with aim pose, analog trigger and
A/B clicks. Suggested bindings cover the Oculus Touch and Valve Index
interaction profiles: trigger shoots, A starts, and B inserts a coin. Aim is
intersected with the submitted quad in its local coordinates, including the
quad's rotation. Coordinates run from the top left `(0,0)` to bottom right
`(1,1)`; a miss of the quad supplies an off-screen input. Triggering off screen
uses the game's existing reload behavior. Lost aim tracking suppresses shots;
loss of session focus or a stale/disconnected viewer releases input overrides.

Shared-memory protocol version 3 carries controller states back to MAME and
returns a monotonically increasing acknowledgement from the game's P1 button
port read. Each fresh controller-trigger edge requests a strong 30 ms pulse on
the right-hand haptic output. Holding the trigger cannot repeat it. Stage
selection and off-screen reload edges use the same feedback path; this does not
inspect game RAM or claim that a bullet hit an enemy. Late attachment, stale
events, duplicate frames and loss of focus do not replay old pulses.
The patch updates the existing P1 gun and button fields, keeping the serial
gun mux and native border test. Virtua Cop 2 retains X 137â€“630 and Y 36â€“425;
the original 5% edge border remains active. Player 2 fields are untouched.
Without the optional publisher or an active fresh controller, native inputs
remain selected. The MAME publisher and viewer must be rebuilt together.

A hidden, silent, below-normal-priority scripted run passed without OpenXR:
Coin increased credit from 0 to 1, Start consumed it and entered stage
selection, fixed-point shots produced visible game bullet impacts, and an
off-screen trigger refilled the empty revolver. The second run completed at
44 emulated seconds with both processes exiting successfully. MAME itself
saved the intermediate pictures; they remain private. The first attempt's
shots were too late, after the player had died. Both runs used the same private
development billboard substitute described above. This proves the shared
memory and original game-input path, not headset tracking or runtime bindings.

For that development proof, run the viewer with `--scripted-input` and enable
`ACVR_M2_INPUT_TEST=1` in MAME. The viewer uses emulated time for Coin/Start,
stage selection, fixed shots and reload. Snapshot requests are honored only
with that explicit MAME test switch. Normal launchers use the live XR actions.
The scripted viewer does not create a graphics context or open an XR session.

The viewer's one shared stereo texture slot completes producer copies before
publication and consumer reads before returning ownership. The producer waits
for that consumer completion before overwriting or resizing the slot. Both
transfers currently use `glFinish` under the slot mutex for correctness; no
throughput or latency improvement is claimed. Swapchain acquisition/waits run
outside that mutex. Every image wait is limited to 10 ms, checks stop between
retries, and yields after a 250 ms retry budget. An expired wait does not imply
readiness: its acquired image is retained for retry and is never released before
a successful wait. A resize race declines the pair rather than mixing eyes.

Stop activates each owning context before graphics cleanup. If activation
fails, it skips those destructive callbacks and retains the resources for a
later Stop retry. The viewer skips its own graphics destruction on failure.
Successful Stop clears texture/FBO identities, dimensions, serials, session
handles, input and haptic state so a same-size restart begins empty. Partial
initialization is also reclaimed by Stop. Core `xrWaitFrame`, other XR calls
without a timeout parameter, and GL driver completion cannot be given an API
timeout here; the existing launcher's process deadline remains the fallback.
The finite image waits do not establish universally bounded driver shutdown.

`test-presenter.ps1` builds and runs hidden, below-normal-priority checks with
time limits using existing OpenXR headers and compiler tools. Pass
`OpenXRInclude` and a private `OutputDirectory`. It starts no XR loader/runtime.
It uses the actual presenter start/stop, texture handoff and copy paths with
an offline consumer, and injects only context failures and runtime API results.
Acceptance passed for 48 exact stereo pairs with alternating frame IDs,
repeated resizes and a deliberately delayed consumer; 46 attempted slot reuses
waited for completed consumer GPU work. Both same-size restart cycles passed.
Context activation failures preserved an unrelated context's texture, skipped
destruction and recovered on retry. Image timeouts/stops preserved acquisition
order and resumed with a complete pair. Haptic tests exercised both suggested
output bindings and game acknowledgement through the 30 ms API dispatch,
including duplicate/stale/attach/focus-regain cases. These are correctness
checks; GPU readback and injected delays make them unsuitable for timing claims.

The rebuilt game also acknowledged all 11 scripted trigger edges in a fresh
44-second run, with OpenXR disabled and MAME snapshots kept private. Physical
vibration, live profile bindings, headset visibility and real-runtime stop and
relaunch still require headset testing.

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
`controller_math_test.cpp` covers rotated quad intersections, corners, misses,
parallel/backward/invalid rays and original gun-port endpoints. It includes
the viewer's `AimMath.h` and patched MAME's `model2_input.h`.

MAME as a whole is GPL-2.0; the new MAME-side source files are BSD-3-Clause.
The source checkout's `COPYING` and `docs/legal` retain the upstream licence
texts and copyright notices. The licence texts are included here as well.


## Build tool versions

`upstream-pin.json` records the exact MINGW64 package versions from the preserved
build transaction, cross-checked against the package-manager history. Core
tools are GCC 16.2.0-3, binutils 2.47-3, Python 3.14.7-1, GNU make 4.4.1-3,
bash 5.3.015-1 and MSYS2 runtime 3.6.10-3. The complete dependency package map
and existing small-target build flags are in the pin. Do not substitute the
separate UCRT64 package set. OpenXR 1.1.43 header hashes are recorded as well.
This is documentation and version identity only: no installs, rebuild or
duplicate patch-apply check is part of this update.
