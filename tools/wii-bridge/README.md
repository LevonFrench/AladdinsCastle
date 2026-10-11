# Wii controller bridge

Live SteamVR action poses, physical velocity/rotation and buttons become two
loopback DSU devices in ordinary Dolphin. Slot0 is right, slot1 left. This is
the input bridge for **3D theatre**, not evidence of the true-3D target.

Build the small C++ pose reader using an existing pinned OpenVR SDK; no network
fetch is performed by its CMake project. Python3.12 and Tk are used for the
visible status/calibration window and UDP server, with no pip dependencies.

```powershell
cmake -S tools/wii-bridge -B .local/build-wii-pose -DOPENVR_SOURCE=<existing-sdk>
cmake --build .local/build-wii-pose --config Release
python tools/wii-bridge/bridge.py --pose-reader .local/build-wii-pose/Release/acvr-wii-pose.exe
python -m unittest discover -s tools/wii-bridge -p test_bridge.py -v
```

Connect Quest through ALVR/SteamVR first. Face the middle of the intended
Dolphin screen, then click **Centre pointer plane**. The reference plane is
2m away,3.2m wide and1.8m high. It stays fixed until recalibrated. Actual screen
size/yaw/pitch must be matched during the sitting. Displayed dots use the aim
pose's forward ray; DSU right-stick axes carry its absolute coordinates.
When the ray misses, the pointer clamps to the nearest plane edge and trigger
still passes through, so a game can use off-screen fire for reload. A game
launch can choose `--offscreen-pointer hide`: this holds DSU Square off-screen,
which hides Dolphin's Point cursor only if that game's Dolphin profile binds
`IR/Hide` to `Square`. Edge mode is the default until that profile is present.
Closing the window stops its reader and server. `--seconds N` bounds a test.

Buttons: trigger→R2, primary(A/X)→Cross, grip→L2, secondary(B/Y)→Circle,
application menu→Options. The right Touch system button does not provide this
application-menu action; a usable Remote + binding is still needed. Thumbstick remains the left stick. DSU provider name
is determined by Dolphin's server description: use **AladdinsCastle**.
Thumbstick click is read but not sent as a Wii button; visible laser toggle is
not implemented yet. Wii profile generation belongs to tools/wii-dolphin-profile.

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

MSVC build and six protocol/motion tests pass, including two real UDP clients
and expiry. These tests do not establish tracking, controller feel, game motion
recognition, stereo viewing, co-op or headset acceptance. The first live probe
reported Hmd Not Found before ALVR connected.

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
