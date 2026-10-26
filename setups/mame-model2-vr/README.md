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
at the zero-parallax distance. Initial placement, both stick clicks and Home use current gaze;
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

Holding both thumbstick clicks (left and right) for three continuous real-time
seconds requests the same clean exit (universal controls v1, U4). The binding is
`/input/thumbstick/click` on both hands, the same path on Touch and Index. Left Y
and Index left B no longer quit, and no menu or system button is bound. Both hands'
actions are synchronized; the right-hand shoot mapping remains intact.
Release, lost focus or stale XR input resets an incomplete hold. A completed
request stays latched. `--scripted-input --scripted-quit-hold` (the old name `--scripted-y-hold` still works) is the explicit
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
checks also passed without starting an XR runtime. Physical thumbstick-click
operation still requires the headset.

The final transport/upload test received 1,263 pairs and passed a GPU texture
readback comparison with OpenXR disabled. The viewer and its PowerShell 5.1
build script compiled successfully. Headset visibility, fusion, colour and
Home behavior have not been tested; owner acceptance remains pending.

The viewer now has a right-hand action set with aim pose, analog trigger and
A/B clicks. Suggested bindings cover the Oculus Touch and Valve Index
interaction profiles: trigger shoots; Start and Coin are the universal stick clicks (below), with A and B kept as extra Start and Coin (a cabinet has both; nothing else here uses them). Aim is
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
gun mux and native border test. Virtua Cop 2 retains X 137–630 and Y 36–425;
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

## Shared presenter candidate (Model 2 first)

The new `libacvr/publish-presenter` target owns the presenter mechanics; the
Model 2 source now keeps only the compatibility adapter and its bitmap/input
transport. Build the candidate with `build-viewer.ps1 -SharedPresenter` using
the existing required path parameters. It produces `model2-viewer-shared.exe`.
The default build still uses the original files in `presenter/legacy` and
produces `model2-viewer.exe`; both can be built side by side. No installed viewer
or launcher is replaced. Keep the original until the candidate has passed play.

Run `test-presenter.ps1` for the shared implementation, or add
`-LegacyPresenter` for the preserved original harness. Both remain hidden and
bounded. The shared test adds pose/serial ownership, loader-table failure,
session/focus, 250 ms input expiry, GL drain, layer construction and recentre.
No OpenXR loader or runtime is opened by these tests. This is offline proof,
with headset behavior still pending. The shared library README describes the
API, build target and remaining adapter work for other emulators.


## Controller map, racing controls, open frame and proof scripts (2026-10-19)

Evidence tiers below: E1 built, E2 offline proof. Nothing here has been seen in a headset.

**Controller map.** `model2_input.h` lists, per game, what the right VR controller is
on that cabinet, using the input fields of MAME's own definitions: a light gun
(Virtua Cop 2, Virtua Cop, The House of the Dead), a mounted gun (Gunblade NY, Behind
Enemy Lines, Rail Chase 2), a flight stick (Sky Target) or a wheel with pedals (Daytona
USA, Sega Rally, INDY 500, Manx TT, Motor Raid). Clones use their parent's entry; a
game without an entry stays view only. Overrides go into the original fields, so the
serial gun mux, the 5% off-screen border (reload) and the analog ports read them
normally. Analog fields that MAME reverses for a user's device are reversed here too.
Trigger acknowledgement for vibration now also comes from the button-port reads of
Sky Target, Gunblade NY, Behind Enemy Lines and The House of the Dead.

**Aim, measured against each game's own gun mark** (test menu, Gun Setting, Gun Mark
Check, default calibration; 17 points, no trigger), all E2:
- Virtua Cop 2: largest error 0.0063 of the screen; no trim needed.
- Virtua Cop: largest error 0.005; no trim needed. It needs the model1io2 and hd44780
  device sets beside the game.
- The House of the Dead (the original revision, MAME's hotdo): with MAME's port range
  the game's mark sat at 0.867x+0.065 and 0.829y+0.087, up to 0.073 off at the edges.
  Its calibration targets are inset, so the patch widens P1's range to 141..629 and
  56..410. Measured again afterwards: largest error 0.0028.
- Gunblade NY: its reticle followed 1.144x-0.060 and 1.154y-0.084 (mission select).
  With the inverse trim the largest error is 0.0133 and the mean 0.0045; one step of
  the 8-bit turret pot is about 0.0076 of the screen, so this is near its resolution.
- Rail Chase 2: its reticle followed the turret with different scales either side of
  centre (x 0.362 at centre, y 0.569), so the trim is piecewise: where the mark lands
  for cabinet input 0, 0.5 and 1 per axis. After it: errors within 0.01 except the
  lowest point (0.038, near the dashboard). Needs the owner's epr-17895.ic8.
- Behind Enemy Lines: its test mode (hold Test at power-on) has GUN CALIBRATION, which
  the patch's full-range turret input passes (min/max stops recorded). In play the game
  shows no reticle; shots land as tracer and hit sparks, so its aim is not measured.

**Shots, with a control run** (MAME reads the script, frame-exact; runs A and B are
identical, C is the same script without trigger pulls): A and B matched in every
picture for Virtua Cop, Virtua Cop 2, The House of the Dead, Gunblade NY and Behind
Enemy Lines. C differed by the games' own ammunition displays, impact marks and, in
Virtua Cop, The House of the Dead and Gunblade NY, enemies hit (E2).

**Racing.** Protocol 4 adds steering (-1..1), accelerator and brake (0..1) and two grip
buttons to the shared controller block. The viewer binds the left trigger (brake),
the right trigger value (accelerator), the left thumbstick X and both grips on the
Touch and Index profiles. Steering is the left thumbstick. Rolling the right controller like a
wheel (60 degrees = full lock) is an opt-in, `ACVR_M2_ROLL_STEER=1` for the viewer; off
by default, and a pushed stick still wins. Grips shift up/down
(an H-pattern box steps one gear per press) or are the game's extra buttons.
Daytona USA, Sega Rally Championship, INDY 500, Motor Raid, Super GT 24h and Desert
Tank, full left steering
from a fixed time against the same run straight: identical repeat runs, and the car
or bike turns left (E2). Steering only takes effect once the race is the player's
(after Daytona's rolling start). Gear shifting on the grips is built, not proven.

**Open frame.** `ACVR_M2_WIDE=<pixels per side>` (93 = 16:9, stereo only) widens the
game's full-width 3D window: the same frustum construction as the hardware's window
command with the left and right edges moved out, a wider render target and wider eye
pictures. The 2D tile layers (HUD, some skies) stay in the middle 4:3 area. Gun aim in
the added sides counts as off the screen. At 16:9 the world continues into both added
sides in Virtua Cop, Virtua Cop 2, Daytona USA and Sega Rally (94-99% of side pixels
drawn), Behind Enemy Lines (97-100%), INDY 500 (79-92%), Gunblade NY (78-92% in play)
and The House of the Dead (70-79%; its 2D sky picture stays 4:3). Motor Raid's track
fills the sides but its 2D sky does not (28-75%). For light guns the added sides read
as off the screen (House of the Dead: the mark goes to its off-screen corner); inside
the 4:3 area aim is unchanged (within 0.0025). A turret stops at the screen edge. Other games are unchecked; a
game that culls to 4:3 shows black sides. The viewer uploads the wider pictures
(hidden OpenGL readback check passed).

**Proof scripts.** A plain text script drives the controls by emulated time:
`<seconds> u= v= off= trigger= start= coin= steer= accel= brake= gripr= gripl=` and
`snap` (see `model2_script.h`). The viewer reads it with `--scripted-input` and
`ACVR_M2_SCRIPT`, through the real shared-memory path. MAME reads it itself, frame-exact,
with `ACVR_M2_INPUT_TEST=1` and `ACVR_M2_MAME_SCRIPT`; two such runs give identical
pictures, so a run without the trigger is a clean control. Virtua Cop 2: two identical
runs matched in all 29 pictures; the same script without trigger pulls differed only
in the game's revolver display and impact marks (E2).

The protocol-4 viewer and MAME must be rebuilt and installed together; the protocol-3
viewer refuses the new transport.


## Universal controls (2026-10-25)

The viewer follows the shared stick-click layer. The same logic is in `presenter/legacy/ACVRChord.h`
(a copy of `setups/supermodel-vr/patch/ACVRChord.h`; `setups/flycast-hotd2-vr/tools/test_chord_copies.py` checks the copies).
The shared candidate (`presenter/`, which links `libacvr/publish-presenter`) runs libacvr's `src/stick_chord.hpp` in
`Presenter.cpp` `ReadController` instead (same 150 ms window, 1 s recentre, 3 s quit), and `presenter/presenter_test.cpp` pins its bindings.

| Gesture | Does |
|---|---|
| Left stick click, alone | Start (the game's own Start input) |
| Right stick click, alone | Coin |
| Both clicks, the second within 150 ms of the first, released within 1 s | Recentre the screen in front of the head |
| Both clicks held for 3 s | Clean exit: MAME persists NVRAM, settings and coin audits, then the viewer exits |

A click waits 150 ms for the other, so Start and Coin reach the game 150 ms late and never as part of a chord.
A and B stay as extra Start and Coin on every Model 2 game (guns and racers): no Model 2 viewer feature uses them,
and this viewer draws no laser, so the laser toggle (right B) is not needed here. Guns: aim ray, trigger, and a
shot off the screen reloads (unchanged). Racers: left stick steers, right trigger accelerates, left trigger brakes,
grips are the hold actions; rolling the controller like a wheel stays opt-in (`ACVR_M2_ROLL_STEER=1`).
Left Y has no function here now; the Home key on a keyboard still recentres.

`presenter/legacy` is the default build and carries this layer itself (in `ReadController`). The shared-presenter
candidate (`presenter/`, `build-viewer.ps1 -SharedPresenter`) reads the same `start`, `coin`, `quitHeld` fields from
`libacvr/publish-presenter`, so it has the layer once that library resolves the stick clicks the same way (lane D);
its `presenter_test.cpp` still pins the old Y binding until then. `chord_resolver_test.cpp` here builds the header test:
`g++ -std=c++17 -Ipresenter/legacy chord_resolver_test.cpp`.
