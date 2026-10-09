# M1 lane B: spike: QML into a SteamVR dashboard overlay (S1-S3)

**Branch:** `m1/b-overlay-spike`. **Depends on:** A. **Read:** [hub-architecture.md](../../hub-architecture.md) §7-§8 and §13, wiki notes `J:/projects/.wiki/topics/steamvr-development/wiki/concepts/steamvr-dashboard-overlay.md` and `wiki/references/qt6-overlay-and-steam-library.md`.

## Goal

Prove, on the owner's PC (Windows, SteamVR active OpenXR runtime, Quest 3 via ALVR), that the same QML scene can be:
- **S1:** rendered offscreen with `QQuickRenderControl` → `QQuickRenderTarget::fromOpenGLTexture` → `IVROverlay::SetOverlayTexture` on a `CreateDashboardOverlay` tab, with laser clicks landing on the right QML item;
- **S2:** typed into from the SteamVR keyboard (`ShowKeyboardForOverlay` → QML `TextInput`), or failing that, from an in-scene QML keyboard;
- **S3:** shown in a desktop window **and** the overlay at once from one process (one `HubRoot` in two presentations), with the GPU cost measured.

## Build

- `hub/src/overlay/`: `OverlayHost` (VR_Init as `VRApplication_Overlay`, dashboard overlay + thumbnail, texture submit only while visible, `PollNextOverlayEvent` → `QMouseEvent`/`QWheelEvent` via `sendEvent`, keyboard events, `VREvent_Quit` → `AcknowledgeQuit_Exiting`).
- `aladdinscastle-hub --overlay` runs the overlay only; `--overlay --window` runs both (S3).
- Test scene: `qml/spike/OverlayTest.qml` with buttons, a grid of 50 dummy cards with MultiEffect glow, and a `TextInput`.
- Overlay manifest `hub/resources/aladdinscastle.vrmanifest` registered with `AddApplicationManifest` (user-triggered, removable).

## Measure and record (in `docs/spikes/s1-s3-overlay.md`)

- Works: yes/no per item; screenshots are allowed (no game art in them).
- Mouse Y orientation (bottom-left vs top-left), click accuracy at dashboard distance, frame time of the offscreen render, GPU memory, overlay update rate.
- Any crash or driver issue, with the Qt and SteamVR versions.

## Acceptance

S1 passes on the owner's headset (the owner confirms by clicking a button in VR). S2 and S3 each have a clear result (pass, or a documented fallback). The spike code stays in `hub/src/overlay/` as the M2 starting point.
