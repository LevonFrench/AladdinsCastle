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
Trigger is suppressed when the ray misses the reference plane or its bounds.
Closing the window stops its reader and server. `--seconds N` bounds a test.

Buttons: trigger→R2, primary(A/X)→Cross, grip→L2, secondary(B/Y)→Circle,
application menu→Options. Thumbstick remains the left stick. DSU provider name
is determined by Dolphin's server description: use **AladdinsCastle**.
Thumbstick click is read but not sent as a Wii button; visible laser toggle is
not implemented yet. Wii profile generation belongs to tools/wii-dolphin-profile.

Acceleration is the derivative of tracking-space velocity, with gravity added,
rotated into the current aim/body frame and expressed as DSU signed g. Gyro is
tracking angular velocity rotated into that frame, converted to deg/s. A gap
over50ms resets the derivative; samples stale for150ms disconnect both pads.
All axes require physical sign/orientation verification against Dolphin.
Do not map Dolphin Shake/Swing in addition to Motion Input. R/V in this window
produce bounded accessibility shake/swing waveforms on that same motion path;
they are provisional and not yet tuned in either game. Global/controller
fallback bindings and independent Nunchuk motion depend on profile integration.

Protocol coverage: DSU1001 header/CRC, version query, requested port discovery,
slot/MAC/all subscription, stable distinct MACs, five-second expiry, analog
button mirrors, g/deg/s units. Each subscriber receives only requested slots;
this matters because Dolphin's device sockets do not filter incoming slot IDs.
Loopback only. Pointer has256 steps per axis; rumble is not supported by DSU.

Sources checked for this implementation:
- https://v1993.github.io/cemuhook-protocol/
- https://github.com/dolphin-emu/dolphin/blob/master/Source/Core/InputCommon/ControllerInterface/DualShockUDPClient/DualShockUDPClient.cpp
- Existing OpenVR2.15.6 header/API at commit0924064316de3effbcd1acf1e309182a2deb1c05.

MSVC build and five protocol/motion tests pass, including two real UDP clients
and expiry. These tests do not establish tracking, controller feel, game motion
recognition, stereo viewing, co-op or headset acceptance. The first live probe
reported Hmd Not Found; connect ALVR before repeating it.
