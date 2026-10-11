# Gun room source checkpoint

This is an opt-in Windows desktop-GL/OpenXR room using libacvr's public
`acvr_runtime_create`. The XR path draws a floor, three shootable targets and
the current muzzle ray into real stereo swapchain images. libacvr overlays an
existing authored gun GLB, using its lit gun renderer and the tracked right
hand. `--model` selects any existing model with a matching GLB and TOML.

`--preview-svg NAME.svg` writes a headless geometry diagram under
`<root>/.local/gun-room` without opening XR. Existing previews require
`--replace-preview`; angle files require `--update-angle-file` to replace.
Writes use a fresh sibling and atomic rename, and reject Windows reparse
points in the output path. The angle values must be finite degrees in
[-180, 180].
It is a source/geometry preview, **not** an eye capture. The model/hand is
absent from this diagram. A real eye capture still needs a loader DLL, active
OpenXR runtime, and an authorized native run.

Configure with existing `ACVR_OPENXR_INCLUDE_DIR`, `ACVR_JSON_INCLUDE_DIR`,
and `ACVR_TOML_INCLUDE_DIR`; there are no fetches. The provider looks for an
absolute `ACVR_OPENXR_LOADER_DLL` environment path or the System32 loader.
The room accepts `--root ABS_PATH --xr`, with `--model arc-pistol-slide` by
default. Optional `--angle-file FILE` reads `right`/`left` degree values;
`--right-angle-deg N` and `--left-angle-deg N` override and save them.
For a bounded native proof run, `--capture-eyes FRESH_NAME --seconds 15` creates
one new directory under `<root>/.local/gun-room` before XR creation. The name
is a single safe path component; existing directories and reparse paths fail.
After a rendered frame, the provider reads the actual left and right acquired
swapchain framebuffers after scene, gun, hand and label rendering and before
release, restores GL read/framebuffer/pack state, then writes `left.ppm`,
`right.ppm` and `receipt.json` exclusively. The receipt records actual eye
dimensions, whether a gun overlay was drawn in each eye, and SHA-256 of the
gun GLB, hand assembly and hand GLBs as read at startup. `status=captured`
requires successful readback and OpenXR frame submission; failure exits
nonzero after frame cleanup. The room exits after the receipt or the 5–60 s
deadline (15 s default in capture mode). A supervisory timeout is still needed
if a runtime blocks inside `xrWaitFrame`. No headset fit/feel is inferred from
the PPMs.

The authored off-hand `grip_two` anchor is connected to the native gun
overlay and ballistic ray while the off-hand grip action is held near the
anchor. The original hand assets are checked in under `assets/hands`. When
their `assembly.json` is present, the native provider
decodes the matching original right/left hand GLBs and TOML motion tables,
renders the active hand with its calibrated rest pose and trigger/grip/primary
finger drives, and renders the support hand while the second grip is captured.
Optional positive finger scale in the assembly is applied, including the
stick-grip thumb. Holding A/X in the room reveals every authored gun-part
anchor after gun motion. Each label names the part, the shared control-set's
candidate game action, and the candidate controller control. No game/backend
declaration is selected in this model-only room, so actions are marked
UNCONFIRMED; unmapped parts show UNCONFIRMED for both action and control.
The room reads `data/controls/defaults.toml` and its selected shared set when
present. Per-game/pack/user overrides and verified backend availability need
the Hub's resolved-controls object passed through a future runtime interface;
this room does not infer them. A/X is an inspection modifier here, not proof
that A/X operates any labelled action. Assembly finger-pose names must exist
in their paired hand GLB before XR initialization. Per-primary-hand support
mounts select the authored anchor for both the support hand and two-hand aim;
the required `primary_mount_by_hand` independently selects the gun's primary
mounting node and local transform, while `hands[hand]` places the glove.
Incomplete selectors, missing gun nodes and invalid transforms reject the room
before XR initialization. This checkout's older assembly must receive the
pending selector data before a native run. CPU checks exercised the exact
updated source data for nine authored models; no XR eye
capture or headset fit has been observed. Do not mark the room accepted from
CPU evidence.
