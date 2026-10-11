# Ordinary Dolphin SBS theatre

Original Windows C++ helper, using an explicit existing OpenVR SDK. This is a
fixed virtual screen, **3D theatre**. It does not put the player or gun inside
the game's scene. No SDK download, game loading or application launch is
performed by the build/tests.

```powershell
cmake -S tools/wii-dolphin-profile/theatre -B .local/build-theatre -DOPENVR_SOURCE=<existing-sdk>
cmake --build .local/build-theatre --config Release
ctest --test-dir .local/build-theatre -C Release --output-on-failure
```

During a lead-coordinated sitting, use the newly started Dolphin process's PID:

```powershell
& .local/build-theatre/Release/acvr-dolphin-theatre.exe --pid <DolphinPID> --seconds 120 --head-file .local/theatre-head.json
```

The head-file parent must already exist, without linked ancestors, and the
filename must be fresh. Capture is limited to the single visible unowned
client window belonging to that explicitly selected Dolphin.exe. No desktop,
other process, alternate HWND, PrintWindow or alternate capture fallback is
used. Multiple windows, hidden/minimized/occluded clients, closed process or
stale HWND stop with a clear error. Keep Dolphin visible and uncovered. A
process handle remains open, so process exit is detected despite PID reuse.
The bounded run stops after1–600 seconds or Ctrl+C/SteamVR quit; cleanup hides
and destroys its overlay and unregisters the hotkey. Capture buffers stay in
memory; no game screenshots are saved.

An occlusion error reports only the blocking window's PID, class and rectangle,
selected client rectangle, DWM cloak state and bounds source. No window title
or other window's pixels are queried. Positively DWM-cloaked windows are skipped
because DWM does not render them; failed cloak queries retain the guard. DWM's
extended frame bounds exclude invisible resize margins; GetWindowRect is the
conservative fallback. Transparent or otherwise suspected non-rendering windows
are not silently ignored. Diagnostics establish the blocker for the next lead
attempt; no screenshot or live VR acceptance is implied.

The capture thread temporarily uses per-monitor-v2 DPI awareness, restored on
exit, so client and DWM rectangles use physical pixels. This is an app-thread
context change, not a system/display setting. [Microsoft's GetWindowRect
documentation](https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-getwindowrect)
explains why DPI-virtualized window rectangles must not be compared directly
with unadjusted DWM frame bounds. The headless Windows regression verifies the
thread context is set and restored without opening windows or initializing VR.

GDI BitBlt copies to a top-down32-bit BGRA DIB. Pixels are submitted through
SetOverlayRaw with the parallel-SBS flag. Client dimensions must have even
width and fit4096x2160; no unbounded allocation. Preflight rejects black capture
before VR initialization. Two seconds of continuous black pixels also stop a
run; a dark game scene can therefore cause a conservative false stop. Nonblack
pixels only establish that the client returned pixels, **not** that the actual
GPU game render was captured. Inspect the real headset frame for game content,
eye ordering, orientation, aspect and colour. If GDI returns a black game
surface, record that result before selecting any alternative capture route.

The initial HMD yaw defines a level, world-fixed plane2m away,3.2m wide,1.8m
high. It stays fixed as the head moves. Texel aspect compensates for the SBS
eye halves and the actual capture dimensions. Ctrl+Alt+C recentres explicitly.
The adjacent OpenVR DLL is probed with an absolute native path before any
delay-loaded OpenVR call. Missing DLL returns a clear error.

## Shared calibration with the input bridge

On initial centre and each Ctrl+Alt+C, the helper atomically publishes JSON:
schema1, increasing revision, width3.2, height1.8, distance2.0, head_valid=true
and head=[12 row-major HMD matrix floats]. The bridge must consume that exact
head matrix and call Mapper.recenter(head) for each new revision. Both use
standing tracking space and the same level-yaw basis. Bridge integration belongs
to the lead. Until it consumes the file, matching screen/pointer calibration
remains open; independently pressing two recenter controls cannot guarantee a
match. No continuous head following or hidden automatic recenter is used.

CPU tests cover invalid numbers, allocation bounds, fixed/yaw/vertical poses,
SBS aspect and black pixels. Windows DLL tests load/unload present and absent
DLL paths and inspect an export; they never call VR_Init. These checks do not
verify actual GDI game capture, headset stereo, controller signs or gameplay.
