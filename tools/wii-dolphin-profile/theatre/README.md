# Dolphin SBS capture and theatre

The Windows helper uses Windows.Graphics.Capture for one explicit Dolphin.exe
PID and one explicitly admitted visible unowned window. In capture-only mode,
its caption must contain the exact selected disc header ID in parentheses;
the initial Qt main window is not accepted just because it is the only HWND.
It crops the captured window to the
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

For a coordinated input-free R3 turn, `desktop_capture.py` supervises both
processes, creates a fresh private User directory and retains logs, proof PNGs
and cleanup receipts. It performs no mouse/keyboard input, Computer Use,
SteamVR initialization or system-setting changes. Scripted DSU input is supplied
separately by the lead. A system prompt stops the item for the owner; this
harness cannot detect unrelated prompts or authorize a runtime slot.

Supply a private JSON configuration under this checkout's `.local` containing
absolute `dolphin`, `capture` and `disc` paths, exact `dolphin_sha256` and
`capture_sha256` hashes, and `disc_header_sha256` (the first 32 bytes). The
ordinary-Dolphin profile currently admits only the two scoped Chronicles ISO
headers. No collection traversal or content copy occurs. Keep this JSON and
all output outside Git. `prepare` validates inputs and writes fresh settings
without launching; `run` requires a different fresh output directory and the
lead's runtime slot:

Optional `dsu_profile` and `dsu_profile_sha256` select the canonical generated
`flat-dolphin/split-right.ini`. Its shared manifest must match exactly. The
supervisor copies those bytes to its fresh User, enables background input and
clears its own named game-profile overrides. The lead's separately supervised
scripted DSU server must start before Dolphin; it is not launched by this tool.

```powershell
python tools/wii-dolphin-profile/desktop_capture.py prepare --config .local/capture-inputs.json --run-dir .local/capture-prepared
python tools/wii-dolphin-profile/desktop_capture.py run --config .local/capture-inputs.json --run-dir .local/capture-run --seconds 15
```

The supervisor waits up to 30 seconds for an owned visible window, then starts
the helper. The helper has a separate 1–90 second startup bound for game-window
selection and the first spatial candidate, followed by the requested 3–60
seconds for a changing second frame. The supervisor allows 5 seconds for exit.
Optional `--settle-seconds 0..30` adds a bounded delay before starting the helper;
elapsed time alone never establishes game readiness. Cleanup receipts distinguish
an already exited child from terminate/kill, and report `normal_exit` separately.
Forced cleanup never proves a normal game exit. No automatic retry occurs. An ambiguous owned
window set or timeout fails without input. A successful RGB gate still leaves
`live_proof` false and `sbs_visual_check` required in the private result: inspect
the PNGs for actual game/SBS content before claiming desktop proof. Headset
acceptance is a separate owner sitting. The new synthetic harness tests cover
identity rejection, fresh output isolation, failed/hung capture cleanup and
reaping a real CPU-only child when the second spawn fails.

Use this only in a coordinated desktop turn, after the native DolphinXR first
boot. Start Dolphin with a fresh isolated profile (1280 x 720, auto-size off),
then provide its explicit PID. No SteamVR initialization occurs in this mode:

```powershell
& .local/build-theatre/Release/acvr-dolphin-theatre.exe --pid <DolphinPID> --seconds 15 --startup-seconds 90 --game-id <disc-header-ID> --capture-only --proof-dir .local/capture-example
```

The output directory must be fresh, under .local with existing unlinked parents.
Two client frames at least 1 second apart in compositor time must differ in RGB.
Uniform white, grey or black frames and alpha-only noise do not pass. The
spatial filter ignores a five-percent perimeter and rejects an interior with
over 99 percent one RGB colour, so rounded startup-window corners cannot qualify.
Resizing during comparison is an error. The two candidate PNGs and capture.json
remain private; a startup/content timeout may produce logs without PNGs.
Inspect the PNGs for the actual game and its SBS halves:
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
