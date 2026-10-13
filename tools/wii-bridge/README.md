# Wii controller bridge

Live SteamVR action poses, physical velocity/rotation and buttons become two
loopback DSU devices in ordinary Dolphin. The canonical generated route sends
slot 0 as Remote and slot 1 as Nunchuk, with the physical hands selected by the
profile. The explicit legacy route retains slot 0 right and slot 1 left. This is
the input bridge for **3D theatre**, not evidence of the true-3D target.

Build the small C++ pose reader using an existing pinned OpenVR SDK; no network
fetch is performed by its CMake project. Python3.12 and Tk are used for the
visible status/calibration window and UDP server, with no pip dependencies.

```powershell
cmake -S tools/wii-bridge -B .local/build-wii-pose -DOPENVR_SOURCE=<existing-sdk>
cmake --build .local/build-wii-pose --config Release
python tools/wii-bridge/bridge.py --pose-reader .local/build-wii-pose/Release/acvr-wii-pose.exe --control-set wii-dead-space-extraction --primary-hand right --head-file .local/theatre-head.json
python -m unittest discover -s tools/wii-bridge -p test_bridge.py -v
```

The canonical route reads only the selected generated
`data/controls/wii/generated/games/<control-set>/scripted-dsu-<hand>.json`,
checks that JSON and its companion INI against `generated-files.json` SHA-256,
and rejects game, hand, role, slot, server or action contract mismatches before
starting Tk, the reader or the socket. It also requires the producer's
`canonical-dsu/1` capability declaration to mark `canonical_live` and
`live_bridge_compatible` true. The current producer marks them false, so the
live canonical route intentionally refuses until its separate source review
and readiness flip; synthetic CPU tests do not override that gate. The generated INI must be installed by
the separate profile preparation; the bridge does not install it. Historical
hard-coded buttons require explicit `--legacy-input`. Connect a SteamVR headset
first. Face the middle of the intended Dolphin screen, then use the theatre's
Ctrl+Alt+C recenter when `--head-file` is shared. The reference plane is
2m away,3.2m wide and1.8m high. It stays fixed until recalibrated. Actual screen
size/yaw/pitch must be matched during the sitting. Displayed dots use the aim
pose's forward ray; DSU right-stick axes carry its absolute coordinates.
When the ray misses, the pointer clamps to the nearest plane edge and trigger
still passes through, so a game can use off-screen fire for reload. Canonical
profiles reserve DSU Square for Remote 1 and reject `--offscreen-pointer hide`.
The explicit legacy route can choose hide only with a matching Dolphin Point
profile; otherwise edge clamp remains the safe policy.
Closing the window stops its reader and server. `--seconds N` bounds a test.

Canonical buttons follow the generated action expressions: for example,
unmodified firing-hand trigger sends Remote B/R2, primary sends Remote A/Cross,
and grip modifies primary to Remote plus/Options. The opposite hand drives
Nunchuk Z/L2, C/Circle and its analog stick. Remote Home/PS remains explicitly
unsupported and emits no button. Both thumbstick clicks reserve recenter and
suppress game buttons and fallback actions. With a shared head file, the chord
does not recenter the pointer alone: use the theatre's Ctrl+Alt+C until a
theatre command consumer is implemented. The bridge follows the theatre's
published revision. The physical application-menu action is not a substitute
for the generated Remote plus chord. Generated game action labels are proposed
until option-screen evidence confirms them.

The reader emits poses at about120 Hz. A dedicated Python reader thread maps
each sample before the UI can skip it; a separate120 Hz thread owns the DSU
socket and sends packets even if the Tk window stalls. Acceleration is a tracking-space velocity
change over the preceding25ms, with gravity added, rotated into the aim/body
frame and expressed as DSU signed g. Gyro is angular velocity in that frame,
converted to deg/s. A gap over50ms resets the derivative. Each one-second log
reports peak dynamic acceleration in g and peak velocity for each hand; it helps
detect a missing velocity stream and lost shake peaks. A short sensor-only latch
repeats a transient for up to25ms so Dolphin's newest-packet input drain still
sees it if it skips a packet. This can delay the return to neutral by at most25ms;
the current pointer and buttons are never held. Tracking loss clears the latch
immediately. A sample stale for150ms
or an untracked hand sends neutral motion/buttons while both DSU slots remain
connected and discoverable. Signs are derived from the pinned DolphinXR DSU
input names and tested with rotated poses; physical verification is still due.
Do not map Dolphin Shake/Swing in addition to Motion Input. R/V in this window
produce bounded player-one-only accessibility shake/swing waveforms on that same motion path;
they are provisional and not yet tuned in either game. On Windows, temporary
Ctrl+Alt+R/V hotkeys also work while Dolphin has focus and are unregistered on
close. Player two uses physical motion; these keyboard shortcuts do not inject
into its device. Conflicts appear in the status window. Controller fallback bindings and
independent Nunchuk motion depend on profile integration.

Protocol coverage: DSU1001 header/CRC, version query, requested port discovery,
slot/MAC/all subscription, stable distinct MACs, five-second expiry, analog
button mirrors, g/deg/s units. Each subscriber receives only requested slots;
this matters because Dolphin's device sockets do not filter incoming slot IDs.
Loopback only. Pointer has256 steps per axis; rumble is not supported by DSU.

Sources checked for this implementation:
- https://v1993.github.io/cemuhook-protocol/
- https://github.com/dolphin-emu/dolphin/blob/master/Source/Core/InputCommon/ControllerInterface/DualShockUDPClient/DualShockUDPClient.cpp
- Existing OpenVR2.15.6 header/API at commit0924064316de3effbcd1acf1e309182a2deb1c05.

The native pose reader has a previous MSVC build. CPU regressions cover
generated-source hashing, both physical hand roles, actual DSU button packets,
modifier suppression, unsupported Home, tracking loss and the reserved recenter
chord. Existing protocol tests cover two real UDP clients, expiry and a short
pulse surviving Dolphin's newest-packet drain. These tests do not establish tracking, controller
feel, game motion recognition, stereo viewing, co-op or headset acceptance. The
first live probe reported Hmd Not Found before the headset connection was ready.

## Live sitting notes

Close the SteamVR dashboard before aiming. The first owner sitting measured
inactive pose actions with the dashboard open and valid left/right poses in
240/240 samples after it closed. Keep inactive action data neutral; do not
substitute raw device poses to bypass dashboard input ownership.

For the original SBS theatre helper, pass its exact `--head-file` path to this
bridge too. Its initial head pose and each Ctrl+Alt+C revision define both the
screen and pointer plane. The bridge rejects mismatched screen dimensions and
nonfinite calibration. A bridge running without a calibration cannot shoot.
This is a fixed-screen fallback, not true 3D mode.
