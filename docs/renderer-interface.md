# Renderer interface v0.2

Published 2026-10-10. Normative ABI: `libacvr/include/acvr.h`; lifecycle and
graphics synchronization: `docs/libacvr-contract.md`. The shared core is described in `docs/libacvr-runtime-core.md`; the XR provider is pending.

The board implements `acvr_backend_query` and `acvr_backend_api`. Query opens
no game files and creates no graphics or XR session. Required callbacks remain
`game_open`, `game_close`, `game_pause`, `game_set_inputs`, `game_step`,
`game_release_frame`, `game_camera`, `game_draw_eye`, `game_poll_outputs`.
True-3D gun backends also require `ACVR_CAP_RAYCAST` and `game_raycast`.

1. Open reports native rational rate, players/guns, semantic controls and positive
   `scene_units_per_metre`. Calibrated scale is per game; provisional values must
   be labelled. Canonical space is right-handed, +X right, +Y up, -Z forward.
2. Inputs are copied before one native `game_step`. Step yields one immutable
   frame lease. Release before the next step. Drawing either eye, replaying at a
   newer display pose, raycast and camera queries never advance simulation,
   consume an input edge or publish new game output.
3. Runtime gives `game_draw_eye` one `acvr_eye`: composed scene-to-eye view,
   off-axis projection, viewport, layer; plus target and predicted display time.
   Matrices are column-major, column-vector multiplication. Projection already
   uses the target API's clip/depth convention. Never toe-in or multiply by the
   native game projection again. Runtime owns target acquisition/presentation.
4. The leased frame retains each original camera. Raycast returns nearest
   eligible geometry, scene position/distance and original camera ID. The v0.1
   `acvr_hit` tail supplies normalized full-native-raster `screen_x/screen_y`
   with `ACVR_HIT_GUN_COORDS`. Coordinates outside [0,1] remain unclamped so
   runtime can mark off-screen. A miss clears flags, found=0, camera=NO_CAMERA.
   If the tail is absent, runtime projects using `game_camera` as before.
   Cabinet ADC/register conversion remains in `game_set_inputs`, owned by the
   board lane. Do not put raw ADC values in normalized coordinates.
5. Separate HUD is optional: advertise `ACVR_CAP_SEPARATE_HUD` only with
   `game_draw_hud`; mark frames `ACVR_FRAME_HUD_VALID` with raster dimensions.
   It draws premultiplied RGBA with no views and no world geometry. Otherwise
   the world pass keeps the merged layers. No blanket classification of sprites
   as HUD. Runtime composites at its configured HUD distance.
6. Runtime owns predicted head/hand poses, anchor and scale composition. A host
   may inspect `acvr_runtime_get_tracking` after tick; callbacks receive matrices
   and timing directly and must not recursively call runtime exports.

The first stereo lane target is a setup-local CPU capture bridge consuming the
same `acvr_eye` and immutable frame/camera/raycast machinery. It writes only
synthetic images in automated tests. This bridge is not a shared graphics API:
advertise supported_graphics=0 until a real graphics adapter exists, and return
UNSUPPORTED from `game_draw_eye` before drawing. Never fabricate GL handles.

Acceptance without hardware: cube reconstruction; asymmetric frusta; translated
origins; center/corner/miss aim; camera association; repeated eyes do not mutate
simulation or output queues; lease rejection; HUD separation; guarded old/small
and oversized structs. A Time Crisis capture and headset play remain separate
owner-approved checks. No game content belongs in tests, reports or commits.

## v0.2 shared depth and desktop compatibility

The first gun/world integration needs the world depth to remain available after
the backend returns. A backend with `ACVR_CAP_REQUIRES_SHARED_DEPTH` requires a
matching runtime-owned depth attachment and rejects a missing one before draw.
For GL this is attached to the supplied complete framebuffer; `depth_image` and
`depth_format` are nonzero. Do not detach, delete or clear it after drawing the
world. Runtime draws guns against that same depth and projection convention.
Configured gun rendering also requires shared depth on a real graphics host.
Backends without this flag retain v0.1's private-depth fallback, but that fallback
alone cannot establish correct gun/world occlusion.

`ACVR_DEVICE_GL_COMPATIBILITY` marks a desktop compatibility-profile context.
It cannot be combined with `ACVR_DEVICE_GLES`. A fixed-function backend checks
this flag and rejects core-only/GLES devices during open, before drawing. This
does not promise a GL version: the provider still checks backend/driver/OpenXR
requirements. Vulkan support and native texture/composition parity are separate.

This minor contract revision adds constants only; struct sizes, ABI major and
v0.1 tails are unchanged. The control-set data format remains version 0.1.
