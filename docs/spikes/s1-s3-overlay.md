# S1–S3: Qt 6.8 dashboard overlay spike

Status: host foundation implemented; current headset acceptance and hardware measurements remain open. Updated 2026-10-10. Diagnostic source `9038515`, published in [PR28](https://github.com/LevonFrench/AladdinsCastle/pull/28) at `d6f011f`, passed independent build and software checks; integration awaits hosted CI. Instructions for the new diagnostic scene below require that source. The spike uses original synthetic artwork and 50 synthetic cards.

## What is implemented

- `OverlayHost` initializes OpenVR as `VRApplication_Overlay`, creates a dashboard tab and its thumbnail, configures mouse scale to 1280 × 800 and width to 2 m, polls main/thumbnail/system events, and acknowledges `VREvent_Quit` before Qt exits.
- `QuickTextureRenderer` uses the Qt 6.8.3 single-threaded example's external OpenGL context, offscreen surface, unshown `QQuickWindow`, `QQuickRenderControl`, and `fromOpenGLTexture`. Each frame runs polish/begin/sync/render/end and `glFlush` before submitting the GL texture. Compositor references are cleared before the GL resources are destroyed.
- One `HubRoot.qml` file has two instances in `--overlay --window`. They share one QML engine and a small `SpikeState` model (text, clicks, effect toggles). Each presentation has its own Qt rendering infrastructure; it does not share a single QRhi across concurrent frames. OpenGL context sharing uses Qt's global share context.
- Rendering and texture submission run only while the dashboard overlay is visible and dirty, capped at one frame per 16 ms. Hidden input polling continues at 50 ms. The overlay scene is hidden while its tab is hidden, pausing its glow animations. A visibility transition forces a fresh frame, and changes requested during rendering remain pending.
- Laser events become Qt mouse events with pressed-button state, left/right/middle buttons, releases on hide/focus loss, and wheel events retaining fractional deltas. Input Y defaults to `height - y` per the OpenVR header's bottom-left contract; the visible **Flip mouse Y** toggle permits a hardware comparison. The GL submission uses reversed V texture bounds. Actual pixel and cursor orientation remains an S1 check.
- In the reviewed PR28 scene, **Open SteamVR keyboard** explicitly requests the keyboard; focusing the field alone does not. The button is disabled in desktop presentation. Minimal + modal mode delivers UTF-8 input as Qt key events. The shared `OverlayKeyboard` component retains token/overlay matching, Unicode/backspace handling and session cleanup. Stale or wrong-overlay packets cannot target a newer session. All 39 on-panel keys and Done remain available; text stays transient and unlogged.
- PR28 adds four corner hit counters, a last-dispatched Qt pointer marker, bounded per-raw-cursor numeric observations and visible list `contentY`. Smooth/discrete counts and raw/Qt deltas are separate. These observe existing routing: button packets still supply coordinates, pointer/button/wheel-remainder state is global, and both scroll kinds still use ×120. Invalid packets and synthetic cancellation releases are outside the observation counters.
- The portable `resources/aladdinscastle.vrmanifest` and `overlay-thumbnail.png` are copied with the binary. `--register-overlay` and `--unregister-overlay` are explicit manifest operations; normal startup never calls them. They do not enable auto-launch. The PNG is original and reproducible using `tools/generate_overlay_thumbnail.py`.

The deliberate per-card `MultiEffect` glow is a stress case for this spike. Production cards should follow the UI spec's cheaper art/border path.

## Verification and acceptance

Current diagnostic source: independent offline build and five affected suites passed (43.47 s): smoke, full UI, synthetic overlay, real QML scene and integration. Tests hit all four corners and all 39 keys plus Done by geometry under both Y settings, move the actual list, and exercise matching/stale/wrong/closed keyboard sessions and two independent software scenes. No runtime or graphics context is initialized. This establishes software behavior only; it does not prove the earlier headset input problems are fixed. The table and packaging receipt below describe the earlier host baseline.

| Check | Local result | Owner hardware result |
|---|---|---|
| Release compilation, Qt 6.8.3 / MSVC 19.44 / OpenVR 2.15.6 | Passed with `/W4 /WX` | N/A |
| Mouse math, button/drag/release, smooth wheel, Unicode and special keys | Passed, hardware-free Qt tests | Cursor placement at dashboard distance pending |
| Hidden dirty-frame gating, dirty-during-render retention, quit acknowledgement | Passed, hardware-free Qt tests | Hide/reopen and SteamVR quit pending |
| Stale keyboard session token / wrong overlay rejection | Passed, hardware-free Qt regression test | Native keyboard delivery pending |
| Two scene instances, translated click/text and shared state | Passed, software/offscreen Qt integration test | S3 desktop + GL overlay pending |
| Portable manifest and thumbnail inclusion | Passed, portable ZIP + clean-PATH headless smoke | Explicit registration/removal pending |
| S1 GL texture accepted and buttons clickable in VR | Not executed | **Pending owner click** |
| S2 SteamVR keyboard | Not executed | **Pending**; in-scene fallback available |
| S3 simultaneous window + overlay | No hardware run | **Pending** |

Local result: all three CTest suites passed (23 Qt test results including setup/cleanup). Portable packaging completed; a clean-PATH help run and catalog count (413) passed, and the manifest, original PNG and QtQuick.Effects DLL/plugin were verified in the portable folder. Qt's Windows offscreen platform warns that its own font directory is absent; the tests use Basic controls and still pass. This is a software-test limitation, not a hardware-render result.

No OpenVR initialization, manifest registration, dashboard launch, or SteamVR setting change was performed during the automated build/test run. CI tests require neither a headset nor game content. The software/offscreen tests establish Qt input and QML behavior, not GPU rendering or compositor acceptance.

## Device acceptance

Earlier interaction feedback recorded in [the controls design](../ui/06-vr-menu-controls.md) includes unreliable whole-panel clicks, failed on-panel letters and missing stick scrolling. No current diagnostic build has headset acceptance or measured GPU performance.

## Owner run procedure

Use a build containing the reviewed diagnostic source above with matching Qt DLLs. These are acceptance instructions, not authorization to run them. GPU/SteamVR/headset use and manifest writes require their own specific owner approval. Start SteamVR/ALVR through the owner's normal workflow; no account or runtime-selection operation is part of this procedure.

Executable: `<HubDir>/aladdinscastle-hub.exe` in the selected portable folder (or `<BuildDir>/bin/aladdinscastle-hub.exe` for development).

1. Start that executable with `--overlay --window --spike`. Choose the **AladdinsCastle** dashboard tab. The spike flag selects diagnostics; without it the normal Hub opens. Registration is optional for a directly started process.
2. Hit all four corner targets with each controller and compare counters and the Qt marker. Compare **Flip mouse Y** if a hit misses; record raw cursor ID, raw mouse position and dispatched Qt position. Check labels and the thumbnail are upright.
3. Focus the text field and verify that no keyboard opens automatically. Press **Open SteamVR keyboard**, type, backspace and Done. Separately test every on-panel letter/digit, Space, hyphen, backspace and Done. Record the failing route/key, never typed payload text.
4. Scroll the list and compare `contentY`, smooth/discrete counters, raw deltas and Qt angle deltas for each controller. Existing ×120/global-position behavior is diagnostic evidence, not a completed routing fix. Test glows, hide/reopen and animation resumption separately.
5. For S3 measurement, compare `--overlay --spike` with `--overlay --window --spike` under the same state. Record measurement source, duration, GPU memory and GPU frame cost; CPU submission telemetry alone is insufficient.
6. To test persistence explicitly, close the process, invoke the same executable with `--register-overlay`, then launch the tab using the owner's SteamVR UI. After the test, invoke `--unregister-overlay`. Moving a registered portable folder requires removing its old manifest before registering the new location. No automatic launch is enabled by the Hub.
7. Quit SteamVR normally and verify the Hub acknowledges quit and exits. Record crashes or GL/OpenVR error strings.

`--spike --quit-after-ms 1000` loads the synthetic scene on the desktop without initializing OpenVR. `--quit-after-ms` can also bound an explicitly requested overlay run. CLI help/version and normal desktop mode do not initialize OpenVR.

## Measurements (unverified)

| Measurement | Value / evidence |
|---|---|
| Qt / OpenVR SDK | Qt 6.8.3 / OpenVR 2.15.6 (build inputs) |
| SteamVR runtime version | Not measured |
| Headset / transport | PCVR headset / streamed transport; no accepted run recorded |
| Mouse Y orientation and click accuracy | Not measured; header specifies bottom-left |
| Offscreen CPU render time | Not measured on a GL run |
| Offscreen GPU time | Not measured |
| GPU memory, overlay-only and dual | Not measured |
| Actual overlay update rate | Not measured |
| Crashes / driver issues | No hardware run; none claimed |
| Screenshots / owner confirmation | Pending |

The host logs actual submitted-frame counts, elapsed wall intervals, and mean/max CPU time for polish/sync/render/end/flush when it runs. These are CPU submission measurements, not GPU timings. The RGBA8 color texture is 1280 × 800 × 4 bytes by construction; that excludes Qt depth/stencil, effects, driver, desktop and compositor allocations and must not be reported as measured total GPU memory.

## Primary API sources

- [Qt 6.8 QQuickRenderControl](https://doc.qt.io/qt-6.8/qquickrendercontrol.html): external-context lifecycle, frame boundaries and input delivery.
- [Qt 6.8.3 single-threaded OpenGL example](https://github.com/qt/qtdeclarative/blob/v6.8.3/examples/quick/rendercontrol/rendercontrol_opengl/window_singlethreaded.cpp): pinned source read before implementation.
- [Qt 6.8 QQuickRenderTarget](https://doc.qt.io/qt-6.8/qquickrendertarget.html): native GL texture and pixel-ratio contract.
- [OpenVR 2.15.6 header](https://github.com/ValveSoftware/openvr/blob/v2.15.6/headers/openvr.h): dashboard API, GL input origin, keyboard flags/events, manifest operations and quit acknowledgement.
- Project design notes: dashboard-overlay and Qt/Steam-library references named in the lane-B brief; the primary API links above support the implementation.
