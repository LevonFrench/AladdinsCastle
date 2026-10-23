# Wii 3D mode â€” DolphinXR first checkpoint

Umbrella Chronicles first, then Darkside. System22 is parked. The target is the
[three things](../../docs/3d-mode-target.md): steady HUD panel, wide world frame,
gun at the hand with scene depth. This checkpoint prepares the real application;
it does not claim that any game passes those requirements.

## Offline measurement review â€” 2026-10-23

The current review records each view and scene before changing game settings.
[Scorecards](scorecard/README.md) keep native, 72 Hz and 90 Hz experiments
separate, retain immutable receipts and leave unsupported claims unknown.
Two-layer XFB source counts do not establish distinct content frames or headset
delivery. E2 is offline evidence; only the owner can provide E3 headset acceptance.

The 23 October audit records all 19 launcher games at native rate and in separate
72/90 Hz experiments. Several routes stop at menus or unverified scenes. No game
has a complete acceptance result for stereo, 2D placement, scale, headset delivery
and controls together. Galaxy 2, Umbrella and Kirby HUD trials remain rejected or
inconclusive. The [experimental XFB patch](xfb-stereo-preservation/README.md) is
default-off: single-core Metroid without it produced the cleaner source-count
result, but clipped geometry remains. A matched Mario Kart 3x/4x pair recorded
equal source counts; retain 3x/MSAA 4 until visual quality and runtime cost are
accepted. Physical world scale remains uncalibrated.

The launcher now has 20 games and 28 views after adding
[Crossbow](crossbow-training/README.md) as a mono theatre candidate. Its timed
target-practice scene is visible offline; the open-frame trial hid targets behind
a blank panel and was rejected. Original game/view route keys and runtime settings
are retained. Labels and control/status notes now state the measured limits.
All 19 default-profile button probes reported inputs; Excitebike retained FIFO
errors. Twelve aim observation sets did not establish numeric calibration.

[Draw discovery](draw-discovery/README.md) records bounded shader identities,
projection/render state and texture metadata to prepare individual placement
trials. A matching shader or orthographic projection does not identify a HUD by
itself. [Bounded replay input](motion-controls/input-probe/APPEND.md) supports
navigation observations through the game's configured input path. Neither tool
creates a per-game acceptance result or authorizes a live settings change.

Use only a separately reviewed synthetic-eye probe, with the owner's headset-use
marker checked before execution. Keep private builds, game media, captures and
measurement receipts under the invoking checkout's ignored `.local` directory.
The older release and runtime checkpoints below remain historical context.

## Release and desktop configuration

[DolphinVR Redux v0.4](https://github.com/iChris4/dolphinXR/releases/tag/v0.4)
is the current Windows release checked on 2026-10-10. The downloaded ZIP is
45,691,100 bytes; its SHA-256 matches the release API digest recorded in
`upstream-pin.json`. Matching tagged source is commit
`85c4266b29eb308d173fefa0f21f5cb77df3516a`; the source checkout is clean and
submodules were not fetched. Release digest verification establishes archive
identity, not an independent reproduction of its executable from that commit.
The executable's own SHA-256 is also pinned for the inspection launcher.

Private layout in the invoking checkout:

- `.local/dolphinxr/releases/v0.4/Dolphin-Redux-Windows.zip` â€” original archive.
- `.local/dolphinxr/releases/v0.4/app/Dolphin.exe` â€” extracted executable.
- `.local/dolphinxr/source-v0.4/` â€” matching pinned source, unmodified.
- `.local/dolphinxr/users/umbrella-inspection/` â€” separate user/config directory.

From that checkout in PowerShell:

```powershell
& ./setups/dolphin-vr/open-config.ps1             # verify identity and prepare only
& ./setups/dolphin-vr/open-config.ps1 -OpenConfig # show desktop configuration
```

The launcher verifies archive/executable identity and never installs/downloads
anything. It seeds a fresh isolated user directory without replacing existing
settings, passes `--user` explicitly, provides no game argument and forces
`-C GFX.VR.EnableOpenXR=False`. No library directory is registered. Desktop
**VR** opens the actual settings pane; leave **Configure in VR** for the owner
sitting. This first checkpoint opened and inspected the real window/settings,
with Enable VR off, Virtual Screen for 2D Content on and Flat Screen off.

**For the lead's later sitting:** close this inspection instance first. Start
the same executable with `--user <isolated-directory>` and explicitly replace
the VR-off override with `-C GFX.VR.EnableOpenXR=True`; only then supply the
privately identified Umbrella disc through `--exec <private-disc>`. That launch
is outside this checkpoint and remains scheduled by the lead with the owner.
No PAL60fps code is enabled: region-specific verification is still required.
Lead owns controller feed/DSU; the separate Dolphin-profile lane owns theatre.

`FlatScreen=False` selects immersive rather than a mono cinema panel.
`VirtualScreen=True` is **the orthographic 2D-content route**, not whole-world
theatre mode. `DisableCPUCull=True` bypasses Dolphin CPUCull during OpenXR; it
cannot recover objects the emulated game did not submit. Distance/size1.5m and
source-default scale1.0 are starting settings, not calibrated Umbrella values.
Wiimote1 Source3 selects this fork's OpenXR remote; others are disabled for the
single-remote sitting. Its full motion/button mapping still needs real verification.

## Concrete base choice and seams

Use this pinned DolphinXR release/source as the **provisional first base**:
it already exposes the native per-eye projection, draw classification, OpenXR
input and desktop tools we need. Evaluate Umbrella before rewriting a renderer.
Findings below are source inspection, not headset/game acceptance. All links
refer to the exact cloned commit.

| Need | Existing seam and next constraint |
| --- | --- |
| HUD split | [GeometryShaderManager.cpp](https://github.com/iChris4/dolphinXR/blob/85c4266b29eb308d173fefa0f21f5cb77df3516a/Source/Core/VideoCommon/GeometryShaderManager.cpp#L448) routes orthographic draws to screen geometry; [GeometryShaderGen.cpp](https://github.com/iChris4/dolphinXR/blob/85c4266b29eb308d173fefa0f21f5cb77df3516a/Source/Core/VideoCommon/GeometryShaderGen.cpp#L501) reprojects that panel through the eyes. ElementsGroupManager has texture/shader draw overrides including ScreenPane. Umbrella's actual HUD/menu/effect classification and original framing are unknown until the sitting. |
| Wide frame | [OpenXRManager::GetEyeProjectionRows](https://github.com/iChris4/dolphinXR/blob/85c4266b29eb308d173fefa0f21f5cb77df3516a/Source/Core/VideoCommon/VR/OpenXRManager.cpp#L3293) builds asymmetric eye rows from XR FOV and poses. [VertexLoaderManager.cpp](https://github.com/iChris4/dolphinXR/blob/85c4266b29eb308d173fefa0f21f5cb77df3516a/Source/Core/VideoCommon/VertexLoaderManager.cpp#L435) exposes DisableCPUCull; game-side culling/FOV may require a PAL-specific profile or patch. No game patch/code is selected yet. |
| Gun depth | [Presenter eye blit](https://github.com/iChris4/dolphinXR/blob/85c4266b29eb308d173fefa0f21f5cb77df3516a/Source/Core/VideoCommon/Present.cpp#L963) copies colour to XR eyes; [D3D11 eye wrappers](https://github.com/iChris4/dolphinXR/blob/85c4266b29eb308d173fefa0f21f5cb77df3516a/Source/Core/VideoBackends/D3D/D3DOpenXR.cpp#L195) have no depth attachment. Gun drawing needs an EFB-stage hook with matching eye/depth conventions or an explicit depth transfer; a post-blit overlay alone cannot satisfy the target. Screen/HUD paths can preserve game depth rather than physical panel depth. ControllerAnchor is a useful transform seam, not proof of grip-position/aim-orientation gun integration. Lead owns the gun renderer. |
| Full precision Wii input | [Wiimote.cpp](https://github.com/iChris4/dolphinXR/blob/85c4266b29eb308d173fefa0f21f5cb77df3516a/Source/Core/Core/HW/Wiimote.cpp#L202) uses float screen-hit u/v and direct IR/accelerometer/gyro overrides, avoiding DSU8-bit transport. Current hit is aim-ray versus HUD plane, not a world-depth raycast. Remote hand selection uses global QuestLeftHanded, not a distinct hand per remote index; two remotes can choose the same hand. Resolve/test after single-remote acceptance. Do not add a second DSU server here. |

## Remaining product work

No game/disc read, renderer frame, shader/culling capture, world scale, gun depth,
shot placement, shake/knife/QTE, controller independence or chapter playthrough
has been verified. No XR session or mapper was opened. No source build was run;
upstream Windows build requires its dependencies/submodules and MSVC toolchain,
which must be addressed in a concrete patch/build slice rather than silently
fetching more dependencies. Darkside camera stabilization comes after Umbrella.
Do not expand beyond the first tier before acceptance.

## Source instrumentation checkpoint â€” 2026-10-10

The earlier release inspection above is historical. The pinned source now builds
with the existing Windows compiler and Qt 6.8.3, using the 28 selected exact
dependency commits. No tool installation or additional arbitrary dependency
download was needed. The OpenXR loader and Dolphin desktop application linked
successfully; upstream notices and local Qt runtime files are preserved with it.
This is a locally instrumented executable, separate from the pinned release ZIP.

[Original instrumentation and instructions](instrumentation/README.md) record
sampled submitted draws, raw game projections/FoV and raster state with VR off.
The real Dolphin translation unit compiled, and synthetic tests passed for
projection math, sampling, row caps, locale-safe CSV and summary frame bounds.
No geometry, game pixels, executable or log enters the repository. Build and
patch identities remain in private storage. The source boot and game-specific
HUD/world interpretation are held: a system permission prompt blocked the
coordinated desktop checks, and the lead withheld the turn pending manual
resolution. This lane launched no overnight game or desktop window. This does
not establish game performance, XR rendering, gun depth or headset acceptance.

Notices are preserved in the extracted release `LICENSES/` and source
`COPYING`, `LICENSES/` and per-file SPDX headers. Source COPYING states mostly
GPL-2.0-or-later with the aggregate compatible with GPLv3. No upstream source,
release binary, user files, captures or disc data are committed here.
