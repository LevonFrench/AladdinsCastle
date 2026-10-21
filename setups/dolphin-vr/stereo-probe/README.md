# Offline stereo probe for DolphinXR (test only)

Per-eye pictures of the real DolphinXR stereo path with **no OpenXR runtime at all**: no SteamVR,
no null driver, no headset. Made on 2026-10-21 to find why stereo looked broken in the Wii games.

## What it changes (four copied source files, pin 85c4266)

`apply.py <folder>` patches copies of `VideoCommon/GeometryShaderManager.cpp`,
`VideoCommon/BPStructs.cpp`, `VideoCommon/FrameDumper.cpp` and `VideoBackends/D3D/D3DMain.cpp`.
Everything is inert unless the environment variable `AC_STEREO_PROBE` is set. With it set:

- `D3DMain.cpp` never initialises OpenXR (the OpenXR loader is not called), and logs
  `AC stereo probe: OpenXR not initialized; synthetic eyes.`
- `GeometryShaderManager.cpp` feeds the VR geometry shader a fixed synthetic head: eyes at
  ±IPD/2 (`AC_STEREO_PROBE_IPD`, default 0.064 m), symmetric FOV (`AC_STEREO_PROBE_TAN`, default
  tan 45° = 1), optional head yaw in degrees (`AC_STEREO_PROBE_YAW`, positive = turned left). The
  rows are the same formulas as `OpenXRManager::GetEyeProjectionRows` / `GetRawEyeProjectionRows`.
  Camera and controller anchors and head tracking are not simulated.
- `BPStructs.cpp` queues DolphinXR's per-frame VR EFB clear, as a running session would.
- `FrameDumper.cpp` makes every screenshot the presented XFB's two layers side by side, unscaled:
  left eye on the left, right eye on the right. If the presented XFB has only one layer, the
  screenshot falls back to Dolphin's normal (mono, display-aspect) picture, which is itself a finding.

Build by compiling the four patched files with the flags of the existing private build tree and
replacing those members in copies of `videocommon.lib` and `videod3d.lib` before relinking
`DolphinNoGUI`; the shared source and build trees stay untouched. Runs: `DolphinNoGUI -p headless`,
hidden, below normal priority, time-limited, `-C Graphics.VR.EnableOpenXR=True`, panic handlers
off, audio off (`-C "Dolphin.DSP.Backend=No Audio Output"`), `XR_RUNTIME_JSON` pointed at a missing
file as a second guard, the input probe from `../motion-controls/input-probe/` for buttons and
screenshots.

## Measuring

- `disparity.py <sbs.png> [--out annotated.png] [--json out.json]` block-matches patches of the left
  eye in the right eye. With parallel eye cameras a point at finite distance has disparity d > 0
  (pixels at eye resolution), far things approach 0, identical eyes give 0 everywhere, swapped eyes
  give d < 0. Expected for a 1920-pixel-wide eye at the default FOV: d ≈ 0.064 / distance(m) × 960,
  so about 41 px at 1.5 m and 15 px at 4 m.
- `eyes.py <folder>...` counts stereo and mono (single-layer XFB) pictures.

## Limits

The synthetic head is not a headset: no lens FOV, no tracking, no compositor, no gun layer. The
probe proves what the emulator renders into each eye; it does not prove what the headset shows.
