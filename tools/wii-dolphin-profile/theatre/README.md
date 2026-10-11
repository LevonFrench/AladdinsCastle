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

An optional **per-run** `--cursor-overlay-pid <observedPID>` acknowledges the
computer-use tool's cursor layer. It excludes an intersecting window only when
its PID matches that explicit value, its class is exactly
`CodexComputerUseCursorOverlay`, and its current extended styles include all of
WS_EX_LAYERED, WS_EX_TRANSPARENT and WS_EX_NOACTIVATE. No opt-in means no cursor
exception; class alone never exempts a window. Every check before/after capture
revalidates the predicates. The first allowed case is logged with PID/class,
bounds and styles; a mismatching PID/class/style keeps capture blocked and a
matching opted PID with wrong class/styles reports why. These styles identify
the tool's convention; they do not prove every cursor pixel is invisible.
No process/window is hidden, killed or altered. The capture DC remains the
selected Dolphin client; no other window's DC or pixels are queried.

GDI BitBlt copies to a top-down32-bit BGRA DIB. Those same guarded pixels are
uploaded without channel/row/size changes to a shared BGRA8 D3D11 texture and
submitted through SetOverlayTexture (DirectX, Gamma) with the parallel-SBS flag.
The device uses SteamVR's GetDXGIOutputInfo adapter index, CreateDXGIFactory1
and D3D11CreateDevice with UNKNOWN driver type and BGRA support. An invalid
adapter fails explicitly; there is no arbitrary/default adapter fallback.
Texture dimensions remain the full client size. Client dimensions must have even
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
standing tracking space and the same level-yaw basis. The integrated bridge
consumes this file through its `--head-file` argument; give both processes the
same fresh path. Its separate centre button is disabled in this mode. Independently
pressing two recenter controls cannot guarantee a match. No continuous head
following or hidden automatic recenter is used. Matching calibration is covered
by software checks; actual image and aiming alignment still need the sitting.

CPU tests cover invalid numbers, allocation bounds, fixed/yaw/vertical poses,
SBS aspect and black pixels. Windows DLL tests load/unload present and absent
DLL paths and inspect an export; they never call VR_Init. These checks do not
verify actual GDI game capture, headset stereo, controller signs or gameplay.

The Windows upload regression uses **software WARP only**, with no hardware
adapter enumeration, windows, GDI capture or VR initialization. It verifies
full640x480/800x600 synthetic BGRA buffers by staging-texture readback, including
every channel/row/alpha byte, same-size resource reuse, resize and rejection
cases. WARP proves the upload implementation, not SteamVR interoperability.
Production clears the overlay resource before resize, and hides/clears/destroys
the overlay before releasing its texture, context and device on exit. OpenVR
errors include the API's enum name and numeric value; D3D errors include HRESULT.

The original raw path returned RequestFailed23 on a full client buffer after
nonblack preflight. The cause is unknown. The checked OpenVR header documents
an unspecified raw-byte upper bound; that does not establish a size cause.
The raw path is replaced, without repeated retries or downscaling. Official
references: [Valve texture submission](https://github.com/ValveSoftware/openvr/wiki/IVROverlay%3A%3ASetOverlayTexture),
[Valve adapter selection](https://github.com/ValveSoftware/openvr/wiki/IVRSystem%3A%3AGetDXGIOutputInfo),
[Microsoft device creation](https://learn.microsoft.com/en-us/windows/win32/api/d3d11/nf-d3d11-d3d11createdevice).
