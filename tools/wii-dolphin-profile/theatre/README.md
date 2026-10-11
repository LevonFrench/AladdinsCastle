# Dolphin SBS capture and theatre

The Windows helper uses Windows.Graphics.Capture for one explicit Dolphin.exe
PID and its single visible unowned window. It crops the captured window to the
client area using matching physical bounds. It never captures a monitor or the
desktop. Overlapping windows do not block capture and their pixels are not read.
The selected process/window disappearing, changing identity or minimising stops
capture. GDI and the former overlap/cursor exceptions have been removed.

Build with an existing Windows SDK, C++/WinRT headers and OpenVR SDK:

```powershell
cmake -S tools/wii-dolphin-profile/theatre -B .local/build-theatre -DOPENVR_SOURCE=<existing-sdk>
cmake --build .local/build-theatre --config Release
ctest --test-dir .local/build-theatre -C Release --output-on-failure
```

Missing headers/runtime are a blocker to report; the project downloads nothing.
Headless checks use software WARP and synthetic pixels, not a capture session,
hardware adapter, game, window or VR initialization.

## Desktop proof

Use this only in a coordinated desktop turn, after the native DolphinXR first
boot. Start Dolphin with a fresh isolated profile (1280 x 720, auto-size off),
then provide its explicit PID. No SteamVR initialization occurs in this mode:

```powershell
& .local/build-theatre/Release/acvr-dolphin-theatre.exe --pid <DolphinPID> --seconds 15 --capture-only --proof-dir .local/capture-example
```

The output directory must be fresh, under .local with existing unlinked parents.
Two client frames at least 1 second apart in compositor time must differ in RGB.
Identical grey or black frames and alpha-only noise do not pass. Resizing during
that comparison is an error. The two PNGs and capture.json remain private even
when the gate fails. Inspect the PNGs for the actual game and its SBS halves:
frame differences alone cannot establish either. Capture size is bounded;
unknown window/frame geometry is refused instead of guessing a crop. A failed
live attempt gets one diagnosis and one retry, then stop and report. Close every
program started for the turn. Do not retry on an observation timeout.

## Theatre, in a later headset sitting

Use a SteamVR headset only when that sitting is separately authorized:

```powershell
& .local/build-theatre/Release/acvr-dolphin-theatre.exe --pid <DolphinPID> --seconds 120 --head-file .local/theatre-head.json
```

The same WGC capture and two-frame gate precede overlay initialization. Captured
packed BGRA bytes are uploaded at full client size through the reviewed D3D11
BGRA8/Gamma texture path on SteamVR's nominated adapter. Hardware interoperability
and headset output remain live gates. No raw upload or alternative capture is
used. The fixed plane is 2 m away, 3.2 m wide and 1.8 m high. Ctrl+Alt+C publishes
its exact HMD matrix/revision for bridge calibration. The inherited SBS aspect
factor is provisional until its proportions are checked in a headset image.
The second proven frame is submitted before the overlay is shown. After the
startup gate, an empty frame pool leaves that last submitted texture
visible while the loop continues to handle recenter, SteamVR quit and the run
deadline. Stop or deadline exits normally. Failure to prove the two fresh,
different startup frames remains an error. The headless tests cover this loop
decision and synthetic pixels; earlier live attempts failed to establish an
accepted game SBS capture or headset image.
All runs are bounded to 3-600 seconds. Capture sessions, frame pools, device
references, overlay resources and temporary hotkeys are released on exit.

Sources: [Microsoft HWND capture](https://learn.microsoft.com/en-us/windows/win32/api/windows.graphics.capture.interop/nf-windows-graphics-capture-interop-igraphicscaptureiteminterop-createforwindow),
[free-threaded frame pool](https://learn.microsoft.com/en-us/uwp/api/windows.graphics.capture.direct3d11captureframepool.createfreethreaded),
[compositor frame time](https://learn.microsoft.com/en-us/uwp/api/windows.graphics.capture.direct3d11captureframe.systemrelativetime).
