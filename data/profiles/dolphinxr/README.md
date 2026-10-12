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

Evidence is limited to the fixed first-mission scene, 1x internal resolution,
and both-eye desktop mirrors using SteamVR's null driver. This is not headset,
head-motion, gun tracking, aiming, performance, other-region, or later-scene
acceptance. Controls and any measured calibration are separate from this native
rendering profile. Private saves and captures remain outside the repository.

UnitsPerMeter = 10 carries the measured human-scale candidate. Both-eye disparity,
actual 63 mm null-driver eye spacing and mirror-axis correction give an estimated
standing-figure height of 1.64–1.82 m. This is compatible with an approximately
1.75 m reference; pose and occlusion prevent an exact stature claim. Remaining
UI states, 4x quality, native aim and owner headset acceptance are still open.
Keep the profile in upstream-native INI format; an upstream offer follows owner
headset review. Existing installed tuning is preserved by create-only seeding.
