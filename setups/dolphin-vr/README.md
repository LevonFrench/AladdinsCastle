# Wii 3D mode — DolphinXR first checkpoint

Umbrella Chronicles first, then Darkside. System22 is parked. The target is the
[three things](../../docs/3d-mode-target.md): steady HUD panel, wide world frame,
gun at the hand with scene depth. This checkpoint prepares the real application;
it does not claim that any game passes those requirements.

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

- `.local/dolphinxr/releases/v0.4/Dolphin-Redux-Windows.zip` — original archive.
- `.local/dolphinxr/releases/v0.4/app/Dolphin.exe` — extracted executable.
- `.local/dolphinxr/source-v0.4/` — matching pinned source, unmodified.
- `.local/dolphinxr/users/umbrella-inspection/` — separate user/config directory.

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

Notices are preserved in the extracted release `LICENSES/` and source
`COPYING`, `LICENSES/` and per-file SPDX headers. Source COPYING states mostly
GPL-2.0-or-later with the aggregate compatible with GPLv3. No upstream source,
release binary, user files, captures or disc data are committed here.
