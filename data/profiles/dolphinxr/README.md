# Native DolphinXR game profiles

`GameSettingsVR/RBUP08.ini` is an Umbrella Chronicles first-mission candidate for
the pinned DolphinXR v0.4 source (85c4266, shader-family format version 2).
It uses DolphinXR's own INI format. With Dolphin closed, back up the matching
file in the selected Dolphin User directory's `GameSettingsVR` folder, then
copy this profile there. Restore the backup to undo it. No game assets are
included.

Ordered experiments at one saved post-tutorial train scene found that disabling
Virtual Screen removed the inner window and displaced bands in both eyes.
Changing internal resolution, EFB copy settings, the shipped settings bundle,
or the other tested VR switches did not remove that defect. Disabling automatic
effect exclusion added ghosting.

Seven shader families were identified through the native shader hunter's skip
preview. Screen overrides put the HUD labels, health fill, weapon/grenade icons,
ammunition digits, and both reticle components back on the panel. A fresh run
with hunting disabled showed a continuous world and a coherent smaller HUD.

Measured evidence uses the fixed first-mission scene at 1x and 4x internal
resolution, selected UI states, and both-eye desktop mirrors using SteamVR's
null driver. The separate owner-reported live sitting is recorded below.
Controls and any measured calibration are separate from this native rendering
profile. Private saves and captures remain outside the repository.

UnitsPerMeter = 10 carries the measured human-scale candidate. Both-eye disparity,
actual 63 mm null-driver eye spacing and mirror-axis correction give an estimated
standing-figure height of 1.64–1.82 m. This is compatible with an approximately
1.75 m reference; pose and occlusion prevent an exact stature claim. Complete
UI composition, quiet timing, other-region/later-scene coverage and systematic
aim/tracking measurements remain open. Keep the profile in upstream-native INI
format and carry these evidence limits with any upstream offer. Existing
installed tuning is preserved by create-only seeding.

ScreenDistance = 4 and ScreenSize = 4 retain the previous angular HUD size while
moving its fixed plane from 1.5 m to 4 m. In fresh paused both-eye comparisons,
reticle disparity fell from 17.45 to 6.57 pixels; the standing figure stayed at
6.40–6.43 pixels and nearby wood at 19.70 pixels. The reticle's binocular depth
mismatch with that figure therefore fell from about 11 pixels to under 0.2 pixels.
It was over the ceiling, so this does not establish target alignment or hit accuracy.

The reticle remained visible over nearer ceiling geometry in the sampled state;
adjacent clean ceiling patches measured 10.30–10.55 pixels of disparity.
Default ExactScreenDepth preserves the game's depth ordering, so moving the plane
does not impose physical foreground masking. Native layer offsets are retired;
element_depth is a legacy ordering value, not a per-reticle distance in metres.
At this distance, captured title, save prompt, loading and movie/subtitle screens
are readable. Scenario/chapter/character/equipment menus retain a panel/background
seam. The flat reference is coherent; native hunting confirms reuse of mission
HUD shader families, while individual texture isolation remains pending.
The mission's 4x, 4x MSAA and 16x filtering comparison is captured; quantitative
performance remains pending quiet timing. The published 4m profile retains the
measured evidence and remaining menu/scene coverage limits.

On 2026-10-12 the owner reported that the live Default/SharpAA sitting passed:
the inner window was gone, depth felt right, shots landed at the visible reticle,
the image was clear and smooth, and the HUD was steady and readable. This is a
bounded owner-reported headset observation for that sitting. It supplies no
numerical frame-time, tracking, hit-rate or later-scene measurements.

On the pinned D3D backend, `D3DOpenXR::SupportsDetachedFrameLoop()` returns false.
Lead's live log reports an inline frame loop without a pacing thread.
`EagerHeartbeat=True` therefore does not establish distinct detached heartbeat
behavior on this renderer. Eager sitting conclusions must retain that backend
limitation; no backend or emulator pin change is included here.
