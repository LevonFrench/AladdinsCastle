# Gun room source checkpoint

This is an opt-in Windows desktop-GL/OpenXR room using libacvr's public
`acvr_runtime_create`. The XR path draws a floor, three shootable targets and
the current muzzle ray into real stereo swapchain images. libacvr overlays an
existing authored gun GLB, using its lit gun renderer and the selected tracked
hand. `--hand right|left` selects the primary hand (right by default) and its
saved grip angle. `--model` selects any existing model with a matching GLB and TOML.

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

For controller calibration, launch with `--xr --calibrate-grip --hand right|left`.
The selected hand must have a focused, tracked grip and aim, active B/Y,
thumbstick axes and trigger, and the trigger must be released. Start with B/Y
released. Hold B (right) or Y (left) with a centered stick for 0.8 seconds and
release to edit. Stick X changes yaw (right moves the muzzle right); stick Y
changes pitch (up moves the muzzle up), at up to 30 degrees per second. The
resolution-relative panel in the central lower field of both eyes shows the
hand, live angles and status. Hold B/Y for 1.2
seconds with a centered stick, then release to save an actual change; a short
tap cancels. Focus, tracking, action or fresh-sample loss cancels and restores
the previous angle. Calibration does not suppress game firing, so keep the
trigger released throughout. The selected hand is fixed for that launch.

Saved angles live in strict per-model/per-hand JSON under
`<root>/.local/gun-room/calibration`. A normal XR room launch applies a
matching saved calibration. Explicit legacy `--angle-file` and degree options
retain their prior yaw-only behavior and take precedence. Controller editing
cannot be combined with capture or legacy angle options. B/Y and stick action
availability depends on the active OpenXR interaction profile; unsupported
profiles show `UNAVAILABLE` and cannot save. This source and CPU path does not
establish headset visibility, controller binding, or grip fit; those require a
separately authorized native XR run.
The libacvr gun-angle setter also serves recorded/model hosts without a
controller-sample provider; only the room's controller edit requires native
calibration samples.
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

For a Hub-owned room process, opt in with `--xr --control-stdin` and pass the
child read end of a private inherited Windows `CreatePipe` as standard input.
The Hub keeps the write end and sends exactly `close\n` (`close\r\n` is also
accepted) to request an orderly runtime destroy and process exit. Closing the
write end has the same effect. The room checks that stdin is a pipe before XR
creation, verifies it is a readable pipe end, duplicates the read handle for
its lifetime, and polls once before XR creation and before each runtime tick
without waiting for input. Malformed commands, trailing
bytes in the same read, and commands longer than seven bytes exit nonzero
after runtime destroy; capture mode records `control_stdin` as the error
receipt reason when possible. Pipe type alone cannot prove that the launcher
created an anonymous pipe, so the private `CreatePipe` ownership is a Hub
contract. A valid cooperative close exits zero even before an eye capture;
that exit code alone is not capture evidence. Only a successful capture
receipt with real eye images establishes a captured frame. Polling cannot
interrupt an OpenXR call already blocked inside the runtime; the Hub still
needs an external timeout for a hung native process.

For the owner's null-driver model review, add `--fixed-poses` to an XR eye
capture. It places the selected gun and authored hands at fixed poses frozen
from the first valid head sample (18 cm to each side, 18 cm below, 55 cm ahead).
The eyes still come from OpenXR; these controller poses are authored fixtures,
not tracking measurements. Trigger/reload/controller levels are neutral in this
mode, and grip calibration is refused. The receipt says
`controller_pose_source=fixed-authoring-poses` and never claims verified
controller tracking. This option requires `--xr --capture-eyes`; ordinary room
launches explicitly clear its process flag and retain native tracked behavior.

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
before XR initialization. The checked-in assemblies include these selectors.
CPU checks exercised the exact source data for nine authored models; no XR eye
capture or headset fit has been observed. Do not mark the room accepted from
CPU evidence.
