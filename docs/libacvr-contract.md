# libacvr backend contract — v0.1

> **Source references:** `namco22-decompile/...`, `time-crisis-vr/...`, `Supermodel/...` and `Rea-Virtua-Cop-2-VR/...` paths are relative to [namco22-decompile](https://github.com/spacestate1/namco22-decompile), [time-crisis-vr](https://github.com/DR-89/time-crisis-vr), [Supermodel](https://github.com/trzy/Supermodel), [Rea-Virtua-Cop-2-VR](https://github.com/NeuralF/Rea-Virtua-Cop-2-VR) as cloned on 2026-10-08 (shallow clones; line numbers may drift). Other paths are in this repo.

Initial design: 2026-10-08; v0.1 published 2026-10-10. Public header: `libacvr/include/acvr.h`. This is a proposed in-process ABI, not an implemented runtime or an assertion that either reference already supports it. libacvr code and public header are MIT; board adapters outside libacvr and the Hub are GPL-3.0-only unless explicitly licensed otherwise. The namco22 and DR-89 reference projects are MIT; Supermodel is GPL. No source was copied and no game content is included.

## Evidence and scope

The local sources, rather than online release descriptions, are the authority for these integration plans. File:line references below belong to these checked-out commits:

| Reference root | Commit inspected |
|---|---|
| `namco22-decompile` | `a9f1a0a1f9de62387a7138006fa36933fd477123` |
| `time-crisis-vr` | `0333e7b457ad3165229a0be63a01467d86609bea` |
| `Supermodel` | `8b4de239bf3e0fd921f4383edcd3c21c2e62067b` |
| `Rea-Virtua-Cop-2-VR` | `d99da1e6795ead74ac1bd9f24eec1a89e414ca62` |

DR-89's development document pins its engine to `6aaa90b4cbc7a23733e1c9f5f9a5e772a19fe23d`, whereas our separate namco22 clone is newer. Its empty `upstream` submodule was not populated. A DR-89 patch is evidence of a technique, not a patch guaranteed to apply to this engine revision.

Read first: `docs/architecture.md` §2, `docs/controls.md` §1.1 and §2, `docs/config-spec.md:83`, `games/raverace/setup/controls.toml`, and `docs/emulators.md`. Their intended order is sound: prove one gun and one racer on the decompiled engine, then the same ABI on an emulator. Concrete source checks reveal three qualifications:

1. Time Crisis uses `ss22_*`; Rave Racer currently uses separate `rr_*` host, CPU and video code, despite sharing geometry/raster modules.
2. Supermodel's simulation and rendering are coupled; New3D rendering also changes the emulated line-of-sight results. A per-eye projection replacement alone is insufficient.
3. Lost World uses analog guns, while other Supermodel games use its light-gun serial registers. The adapter must select the route from the loaded game's input flags.

M2 lane contract entrypoints: `docs/renderer-interface.md` and `docs/control-set-format.md`. These supersede older draft examples where stated; no runtime implementation is claimed.

## Roles and ABI rules

The setup supplies `acvr_backend_query`; libacvr calls the returned `game_*` function pointers. Thus game state/camera/output data flows backend → runtime, but callback invocation flows runtime → backend. The setup entry point drives `acvr_runtime_create`, `acvr_runtime_tick`, pause and destroy. The backend owns emulation, prepared geometry, input register conversion, persistence and native audio. libacvr owns OpenXR, graphics device selection, XR images, predicted poses, world anchor, guns, ghost interactions, comfort and shared overlays. Neither backend owns a second XR loop.

`acvr_runtime`, `acvr_backend` and `acvr_frame` are opaque pointers. Private C++ classes are legal behind them, but exceptions, STL objects, SDL types and native API structs never cross the ABI. There is no public wire/shared-memory format. This follows the in-process DR-89 host model; VC2VR's separate seqlocks (`Rea-Virtua-Cop-2-VR/src/vc2_share.h:13` and `:82`) belong in a later transport adapter. Copying this pointer-bearing ABI into shared memory would be invalid.

Every concrete struct starts with `uint32_t size, version`. Initialize the **entire** struct to zero, then use `ACVR_INIT`; that macro sets only the prefix. All input and output slots, nested graphics/target records and callback tables carry their own prefix. V1 requires the complete original record for existing types; the v0.1 compatibility section below defines the accepted original sizes and appended tails. New v0.1 types require their complete record. A future append-only extension may accept larger sizes under version 1; it must neither read nor write beyond the advertised size. Unknown versions return `ACVR_BAD_VERSION`. Factory/API major mismatch also returns `ACVR_BAD_VERSION` before any callback is used. For output structs the caller supplies size/version; the writer preserves them, writes known payload fields only, and leaves unknown tail bytes unchanged. Query initializes unknown optional pointers in its supported payload to NULL.

Array strides are bytes, at least the supported element size, aligned for that element and at least each element's advertised size. Elements may have larger tails. Iterate by byte arithmetic and stride, never `array[i]` when the caller's stride differs from `sizeof`. Count 0 permits NULL; a nonzero count requires a pointer. Validate count × stride for overflow before accessing it. No packing pragmas: standard platform C layout, the same pointer width, endianness, alignment and calling convention on both sides. Supported builds need 32-bit IEEE binary32 `float`, 64-bit native-handle storage and pointers no wider than 64 bits. Windows functions/callbacks use `__cdecl`; platform builds separately negotiate desktop GL versus GLES. This is a C99 source interface and a same-platform ABI, not a universal binary across x86, x64 and ARM64.

`acvr_result` is signed 32-bit. `ACVR_OK` succeeds, `ACVR_MORE` is only an output-drain continuation, `ACVR_STOPPED` is normal runtime-tick completion, and negative values report argument, version, support, state or general failure. Runtime terminates the session cleanly on a failed step/draw; it never presents half a stereo pair. Factory/open/create set their output handle to NULL before starting; failure unwinds partial allocations. Runtime copies the callback table and required config strings during create; callback code must remain loaded until destroy. Backend close and frame release cannot fail. Persistent-save failure is surfaced by destroy even though destruction completes.

All callbacks and exports run serially on the runtime's owner thread, with the selected graphics context/device available. A backend may have internal audio/emulation workers, but publication waits for them and draw/raycast/camera never access changing emulated RAM. Callbacks do not recursively call runtime exports. `acvr_runtime_tick` may block on XR pacing; UI input and pause are processed on this same thread. Cross-thread stop/pause APIs are deferred.

## Inventory: every public type

The table explains each type and its concrete seam. Struct field semantics in the following sections are normative proposed requirements; they are not claims of existing upstream ABI compatibility.

| Type | Purpose and real source mapping |
|---|---|
| `acvr_result` | Explicit failures replace backend-specific booleans and termination; host-open boolean at `namco22-decompile/engine/ss22_host.h:30`, engine `exit` paths at `namco22-decompile/engine/ss22_run.c:369`. |
| `acvr_runtime` | Owns the unified XR loop and calibrated origin represented today by DR-89 host state and `ss22_host_frame` at `time-crisis-vr/quest/quest_host.c:585`. |
| `acvr_backend` | Owns a private game instance; wraps Time Crisis initialization at `namco22-decompile/engine/ss22_run.c:516` or a private `CModel3` initialized at `Supermodel/Src/Model3/Model3.cpp:3128`. namco22 globals mean initially one instance per process. |
| `acvr_frame` | Lease over an immutable prepared frame; `namco22-decompile/engine/ss22_gl.h:41` and Supermodel GPU snapshot boundary at `Supermodel/Src/Model3/Model3.cpp:2150`. Requires adapter changes, especially for Supermodel. |
| `acvr_control_desc` | Declares supported semantic/player combinations and ranges; cabinet tables at `namco22-decompile/engine/ss22_input.h:18` and `Supermodel/Src/Inputs/Inputs.cpp:114`. Prevents silently applying a six-gear profile to a four-gear game. |
| `acvr_gun_input` | N slots, projected coordinates, aim-frame/camera provenance, trigger edges, reload and off-screen; board gun ports at `namco22-decompile/engine/ss22_board.c:26`, DR-89 tick input at `time-crisis-vr/quest/quest_host.c:681`, Supermodel distinct gun families at `Supermodel/Src/Inputs/Inputs.cpp:184` and `:211`. |
| `acvr_axis_input` | A finite canonical scalar plus semantic/player; `namco22-decompile/engine/ss22_input.h:33` and `Supermodel/Src/Inputs/InputTypes.h:51` establish cabinet-specific scaling. |
| `acvr_button_input` | Held/rising/falling state independent of device; shifter state at `Supermodel/Src/Inputs/InputTypes.cpp:162` and active-low RR bits at `namco22-decompile/raverace/src/rr_host.c:731`. |
| `acvr_inputs` | Complete tick-indexed snapshot of stride-aware gun/axis/button arrays; input boundary at `namco22-decompile/engine/ss22_run.c:358` and poll-before-frame at `Supermodel/Src/OSD/SDL/Main.cpp:1055`. |
| `acvr_graphics_device` | Borrows runtime-selected API/device/context/procedure resolver; DR-89 context setup and XR image initialization at `time-crisis-vr/quest/quest_host.c:394` and `:529`. Vulkan/D3D11 fields are proposed future implementations; these clones demonstrate GL only. |
| `acvr_open_info` | Game/content/storage/options plus selected graphics device; Time Crisis main initialization at `namco22-decompile/engine/ss22_run.c:437`, board persistence at `namco22-decompile/engine/ss22_board.c:446`. Paths point to user-supplied local inputs. |
| `acvr_backend_info` | Native rational rate, scale, supported controls, players/guns and optional capabilities; board pacing at `namco22-decompile/engine/eng_vsync.h:1`, Supermodel selectable pacing at `Supermodel/Src/OSD/SDL/Main.cpp:1547`, DR-89 geometry scale at `time-crisis-vr/quest/quest_scene.c:37`. |
| `acvr_step_info` | One native tick, not one display refresh; deadline arithmetic at `time-crisis-vr/quest/quest_clock.h:8` and sliced CPU boundary at `namco22-decompile/engine/ss22_run.c:317`. |
| `acvr_frame_info` | Frame ID, primary camera, camera count, cut signal and HUD dimensions; scene camera retention/selection at `time-crisis-vr/quest/quest_scene.c:14` and `:90`, Supermodel multiple viewport nodes at `Supermodel/Src/Graphics/New3D/New3D.cpp:1101`. Cut identification is a proposed adapter hint. |
| `acvr_game_camera` | Original native focal/centre, viewport, canonical view, clipping and retained/local flags; `namco22-decompile/engine/geo_hw.h:75`, projection at `namco22-decompile/engine/geo_hw.c:255`, Model 3 frustum extraction at `Supermodel/Src/Graphics/New3D/New3D.cpp:1111`. |
| `acvr_ray` | Runtime controller ray transformed into the backend's canonical scene; origin/direction flow at `time-crisis-vr/quest/quest_host.c:379` and `:385`. |
| `acvr_hit` | Nearest scene point with the originating camera ID; triangle-held camera and nearest intersection at `time-crisis-vr/quest/quest_scene.c:10` and `:119`. Supermodel needs a new geometry or ID/depth query. |
| `acvr_render_target` | Borrowed colour/depth attachments with API formats, dimensions, layers and layouts; acquire/wait/release at `time-crisis-vr/quest/quest_host.c:633`, and multiview array attachment at `time-crisis-vr/quest/quest_gl.c:108`. |
| `acvr_eye` | Latest view and off-axis projection, eye/layer and target rectangle; DR-89 per-eye submission at `time-crisis-vr/quest/quest_host.c:630`, Model 3 model/projection uploads at `Supermodel/Src/Graphics/New3D/R3DShader.cpp:365` and `:391`. |
| `acvr_draw_info` | Binds one immutable simulation frame to a display ID, time, target and one/two eye views; DR-89 replay comment at `time-crisis-vr/quest/quest_host.c:589` and multiview fallback at `:624`. |
| `acvr_output_event` | FIFO changes/pulses with tick, sequence, channel, effect and score/state value; MCU changes/recoil at `namco22-decompile/engine/ss22_out.h:3`, lamp changes at `Supermodel/Src/OSD/Outputs.cpp:95`, four FFB modes at `Supermodel/Src/Model3/DriveBoard/WheelBoard.cpp:581`. Score extraction remains per-game work. |
| `acvr_outputs` | Caller-owned bounded drain buffer with visible overflow; change callbacks above motivate a queue instead of sampling only the final lamp state after several catch-up ticks. This queue is new libacvr adapter behavior. |
| `acvr_backend_api` | Versioned factory-produced callback table; extends host-table precedent at `namco22-decompile/engine/ss22_host.h:12`, while isolating C++ `CModel3` methods behind C. |
| `acvr_backend_query_fn` | Factory signature for a loadable or statically linked setup, replacing compile-time selection represented by `ss22_main(..., g)` at `namco22-decompile/engine/ss22_run.c:437`. Export naming/loader is proposed, not upstream. |
| `acvr_runtime_config` | Selects API, rail/cockpit anchor, requested refresh, fallback aim distance, bounded catch-up and merged controls; DR-89 origin/cover/refresh flow at `time-crisis-vr/quest/quest_host.c:345` and `:499`; ghost file schema at `docs/config-spec.md:83`. |

## Lifecycle, callbacks and timing

| Function | Contract and integration evidence |
|---|---|
| `acvr_backend_query` | Validates requested ABI and caller table, returns supported API mask and callbacks without creating devices or opening content. Setup supplies this new factory, as described above. Both real adapters advertise GL initially. |
| `game_open` | Called once after XR-compatible graphics creation. Initializes backend; reports immutable `acvr_backend_info`; first frame is not yet available. Adapt namco22 initialization at `namco22-decompile/engine/ss22_run.c:516` and Supermodel `Init`/attachment at `Supermodel/Src/Model3/Model3.cpp:3069` and `:3128`. Copies retained open strings. |
| `game_close` | Stops backend workers/audio and frees its resources while graphics is still alive; accepts an opened backend only, called exactly once. Existing cleanups at `namco22-decompile/engine/ss22_run.c:377` and DR-89 host cleanup at `time-crisis-vr/quest/quest_host.c:716`. Does not close the runtime device. |
| `game_pause` | Idempotent 0/1; freezes simulation and native audio appropriately, clears queued input transitions and emits FFB-stop/solenoid-off. Lease remains drawable. Does not pump a private display loop. Supermodel thread pause at `Supermodel/Src/Model3/Model3.cpp:2282`, DR-89 focus neutralization at `time-crisis-vr/quest/quest_host.c:305`. |
| `game_set_inputs` | Copies a complete snapshot for the next tick before returning; no retained input pointers. Validates declarations, slots, ranges and IDs. No desktop polling overwrites it. MCU route at `time-crisis-vr/quest/quest_host.c:694`, RR assignments at `namco22-decompile/raverace/src/rr_host.c:731`, Supermodel input-register consumption at `Supermodel/Src/Model3/Model3.cpp:585`. |
| `game_step` | Advances exactly one reported native tick and publishes one frame lease/info with matching tick ID. Returns `BAD_STATE` when paused or a lease is outstanding. Preparation occurs once; may do private GPU preparation and native-camera hardware LOS work. No XR acquisition, pacing, presentation or process exit. Split the loops at `namco22-decompile/engine/ss22_run.c:368`, `namco22-decompile/raverace/src/rr_main.c:253`, and `Supermodel/Src/Model3/Model3.cpp:1947`. |
| `game_release_frame` | Ends the sole lease; no further camera/raycast/draw on it. Adapter hook is new: named preparation functions above currently keep static buffers until the next preparation. |
| `game_camera` | Copies camera ID 0..count−1 from the lease, never live RAM. Primary ID is one of them, or `NO_CAMERA` when count=0. Uses per-quad projection capture at `time-crisis-vr/tools/patch_upstream.py:32` and per-viewport parameters at `Supermodel/Src/Graphics/New3D/New3D.cpp:1101`. |
| `game_draw_eye` | Mandatory even with multiview. Draws one supplied view, clears only its target rectangle/layer, leaves final colour in supplied target. Draws world plus merged HUD unless HUD separation is advertised. Can be called zero or many times on a lease; no CPU advance, input edges, output events, LOS publication or swap. Underlying `ss22_draw` declaration at `namco22-decompile/engine/ss22_gl.h:45`; Supermodel requires split of `Supermodel/Src/Graphics/New3D/New3D.cpp:442`. |
| `game_poll_outputs` | Drains oldest queued events up to capacity and returns `MORE` while events remain; OK when empty. Must be callable without outputs capability (empty). Writes count/dropped and supported event payloads only. Does not regenerate persistent events on each poll. `ss22_out_poll` at `namco22-decompile/engine/ss22_run.c:362`, Supermodel change sink at `Supermodel/Src/OSD/Outputs.cpp:95`. |
| `game_draw_multiview` | Optional capability; exactly two views, same target rectangle dimensions, distinct array layers in one target. `UNSUPPORTED` may be returned **before any recording/drawing**, allowing eye fallback. Other failures terminate the frame. DR-89 conditional route at `time-crisis-vr/quest/quest_host.c:624` and `time-crisis-vr/quest/quest_gl.c:100`. |
| `game_raycast` | Optional for racers; required capability for a true-3D gun setup. Finds nearest visible shootable scene geometry within ray range, returning original camera ID and canonical hit position; misses return OK/found=0. Replaces private `qvr_aim` intersection step at `time-crisis-vr/quest/quest_scene.c:119`. No generic shared mesh ABI is needed. |
| `game_draw_hud` | Optional separate-HUD capability. Receives target and timing with view_count=0/views=NULL, draws premultiplied RGBA, transparent outside HUD, no world/cabinet/UI. Cache once per frame, composite separately per display. Text and mixed sprites at `namco22-decompile/engine/ss22_gl.c:505` and `:530`; Supermodel tile layers at `Supermodel/Src/Model3/Model3.cpp:2171` and `:2173`. Classification needs new work. |
| `game_flush_persistent` | Optional, after pausing/releasing lease, before close; saves only backend settings/NVRAM under the open storage root, reporting failures. EEPROM at `namco22-decompile/engine/ss22_board.c:461`, RR EEPROM at `namco22-decompile/raverace/src/rr_hw.c:192`, Model 3 NVRAM at `Supermodel/Src/Model3/Model3.cpp:1917`. Arbitrary save-state ABI is deferred. |
| `acvr_runtime_create` | Proposed runtime export: validate table/config, choose an API supported by the backend, create XR/device, call open, validate capabilities and controls, start unpaused. Mirrors XR initialization responsibility at `time-crisis-vr/quest/quest_host.c:394`. A missing required callback/capability fails creation. |
| `acvr_runtime_tick` | Proposed export: handles one display iteration, pacing/poses/input, 0..N native ticks and outputs, replay draw, submit or zero layers, return. Based on `time-crisis-vr/quest/quest_host.c:585` and deadline helper `time-crisis-vr/quest/quest_clock.h:8`. No runtime code exists yet. |
| `acvr_runtime_set_paused` | Proposed export forwards neutralization and pause, resets deadline on resume; based on focus/pause paths at `time-crisis-vr/quest/quest_host.c:305` and `:325`. |
| `acvr_runtime_destroy` | Proposed export: pause, drain/cancel haptics, release lease, optional flush, close backend, destroy graphics/XR. NULL succeeds; returns first failure while finishing cleanup. Mirrors DR-89 cleanup at `time-crisis-vr/quest/quest_host.c:716`. |

Optional capabilities require their matching non-NULL callbacks; a callback alone does not enable a capability. New unknown capability bits are ignored. No global maximum N guns is baked into the header: open reports the actual count and controls list; runtime can reject an unsupported count with `UNSUPPORTED`. Time Crisis reports one, Lost World two. Slot-to-player mapping is fixed by `ACVR_CONTROL_GUN` declarations.

### One frame, several displays

1. On first active tick, start at native tick 1/simulation time 0, apply neutral or sampled inputs, step and obtain a lease. No callback accesses a nonexistent initial frame; initial unprojectable guns are off-screen with `NO_CAMERA`.
2. Convert XR predicted time into the runtime monotonic domain. Compute due ticks with an integer rational accumulator from `native_rate_num/native_rate_den`, retaining fractional nanoseconds. Time Crisis/RR initially report 59906/1000. Do not round to 60 or derive game speed from headset refresh.
3. Locate/sync newest poses/actions. While the current lease is valid, raycast/project gun aim. Queue short digital transitions since the last native tick. These screen coordinates intentionally describe the last displayed/prepared frame; carry its frame/camera IDs as provenance. Backend consumes the coordinates directly, never reprojects using a new camera.
4. For each due tick up to the configured budget: build a complete input snapshot, consume each queued edge once, release the previous lease, set inputs, step, read new cameras and drain outputs. Reuse latest analog levels on catch-up ticks; recompute aim against each newly published lease if needed. All source pointers from a lease cease to be valid on release. Backend may retain copied camera history only for diagnostics; it must not require old leases to apply gun coordinates.
5. Render the latest lease to both acquired eye targets with freshly located views, or to an array target through multiview. It remains unchanged across the pair and all display replays. Advance display ID once per attempted XR iteration, even with zero layers; repeated display frames share frame ID.
6. When shouldRender is false, skip graphics but keep native timing if focus/tracking is otherwise valid. On pause/focus or required tracking loss, stop stepping, clear edges, neutralize controls, stop haptics and reset the deadline. Continue XR event processing/empty submissions or rendering the frozen frame plus pause overlay as allowed by session state. On resume use current controls, not queued pre-pause taps.

At 120 Hz, roughly every other display repeats a prepared native frame. At 45 Hz, some displays require two native ticks. No extra XR frame is submitted for catch-up. A >250 ms interruption resets the wall-time deadline without inventing skipped simulation ticks, following `time-crisis-vr/quest/quest_clock.h:10`. If the catch-up budget is exhausted, preserve backlog for the next display and expose it in future runtime diagnostics; if backlog grows past that interruption threshold, reset only the wall-time deadline. Game time may slow under sustained overload; never silently accelerate physics or pretend that requested 120 Hz was achieved.

Supermodel currently offers 60 Hz and true 57.524 Hz pacing (`Supermodel/Src/OSD/SDL/Main.cpp:1547` and `:2187`). Report the selected emulation mode explicitly, initially 60000/1000 or 57524/1000. The internal board/audio budgets must be audited before calling either a hardware-exact rate. Requested XR refresh is independent and may fall back, as DR-89 enumerates/requests at `time-crisis-vr/quest/quest_host.c:499`.

## Cameras, coordinates and aiming

The ABI scene uses right-handed +X right, +Y up, −Z forward; all matrices are column-major multiplying column vectors. `view_from_scene` maps the scene to this canonical camera space. Eye view includes anchor, physical eye translation and `scene_units_per_metre`, so backend code must not apply scale or head offset a second time. `projection_from_view` is a rendering matrix in the negotiated API's clip/depth convention. It is **not** the original game camera used for gun projection.

For a hit H, compute C = game_camera.view_from_scene × (H,1), d = −C.z. For d > 0:

```
sx = centre_x_px + focal_x_px * C.x / d
sy = centre_y_px - focal_y_px * C.y / d
nx = sx / raster_width
ny = sy / raster_height
```

Focal/centre and viewport are native pixels, never eye-target pixels. Use the camera associated with the hit; all camera IDs are dense and frame-local. A polygon-camera association stays private in the backend raycast structure. No public per-polygon array is required. On miss take H = ray.origin + ray.direction × configured_far_distance_in_scene_units and use the primary camera. This deliberately implements the project controls document's far-along-ray fallback; DR-89 instead intersects a fixed Z=−2.5 m plane (`time-crisis-vr/quest/quest_scene.c:125`). The chosen default distance is an owner question.

Off-screen is true for no valid camera, no tracking, non-finite/behind-camera result, a point outside native raster, or outside that camera's native viewport. Use inclusive raster/viewport edges in the shared math; device-count adapters clamp and round at the last boundary. DR-89 uses strict interior comparisons at `time-crisis-vr/quest/quest_scene.c:131`; hardware-edge acceptance needs calibration tests. Preserve unclamped finite nx/ny for diagnostics and off-screen determination, then clamp only on device conversion. For invalid projection send finite (0.5,0.5), `NO_CAMERA` and OFFSCREEN; no NaN crosses the ABI.

System 22 source projection is captured **before** eye conversion: f = zoom_mant × 2^(−zoom_shift), cx=320+vx, cy=240+vy (`namco22-decompile/engine/geo_hw.c:255`). `geo_vert` stores sx/sy at 1/16 pixel and view-space Z (`namco22-decompile/engine/geo_hw.h:12`). Reconstruct X=(sx−cx)Z/f and Y=(cy−sy)Z/f; put canonical Z=−Z. Scale into metres if choosing scene_units_per_metre=1. `geo_view.m` includes object transforms, `viewq` is separate lighting rotation, and its translation is already view-space (`namco22-decompile/engine/geo_hw.h:75`). Do not claim that matrix is a complete global game-world camera. A DR-89-style adapter reports VIEW_LOCAL and identity view because its reconstructed vertices already live in view-local scene space. Rail anchor naturally follows that game camera; cockpit controls anchor to the same seat reference. Global car pose/velocity still requires game-specific extraction.

Retain the last valid primary camera on polygon-free title/menu frames; mark RETAINED. Before the first valid camera, Time Crisis may use the measured setup default f=772.5625, centre=(320,240), raster=640×480, as DR-89 does at `time-crisis-vr/quest/quest_scene.c:15`. It is not a generic System 22 constant. DR-89's `time-crisis-vr/docs/AIM-PROJECTION.md` records why using f=500 for flat overlays broke alignment. Multiple cameras are supported even though that particular replay observed one.

For Supermodel preserve native angles from viewport records before widescreen correction. With l,r,b,t from `Supermodel/Src/Graphics/New3D/New3D.cpp:1127`, native viewport origin (vx,vy) and dimensions (w,h), derive fx=w/(r−l), cx=vx−l·fx, fy=h/(t−b), cy=vy+t·fy; native full raster is 496×384. This assumes the conventional perspective relationship X/d ∈ [l,r], Y/d ∈ [b,t]; validate against native flat rendering, including asymmetric/subviewports. Matrix #0 plus coordinate compensation is not automatically a uniquely identifiable world camera. Initially expose view-local canonical scene space after those transforms, with identity view; add game-world views only where positively extracted.

World, mirrors, cockpit polygons, direct screen-space quads and unrelated viewports need classification. Mirrors/HUD cannot become raycast targets merely because they contain triangles. If a backend cannot supply accurate hit-camera association and calibration, it cannot claim true-3D gun support; creation must fail for that requested mode. Rendering stereo alone is an insufficient gate.

## Inputs and ghost controls

Control sets stay runtime-owned. Parse merged TOML, build/position the ghost shapes and apply grip pickup/soft lock, two-hand wheel math, shifter detents, springs, analog trigger curves and comfort there. Send semantic values only. Seat-space positions and held-hand/grab state are not emulator inputs. `docs/config-spec.md:83` and `games/raverace/setup/controls.toml` map wheel→STEERING, accel→ACCELERATOR, brake→BRAKE, low/high→GEAR_LOW/HIGH, view→VIEW. Unknown TOML keys remain in the config layer; unknown element types warn/skip as the spec states. A known output semantic unsupported by the backend warns and disables that element rather than silently feeding another axis.

STEERING/LEAN/STICK_X/STICK_Y range −1..1; accelerator/brake/rear-brake/lever/pedal-speed/cover-pedal range 0..1. Pedal speed is normalized against the configured full-scale cadence; it is not physically measured rpm in the ABI. Extension semantics start at 0x10000 and are declared by this setup, with explicit range/name. Semantic IDs are separate namespaces for axes/buttons/gun slots. Duplicate (kind,semantic,player) declarations or snapshot entries are invalid. Unsupported entries return `UNSUPPORTED`. Absent entries in a complete snapshot mean zero/released/untracked; a runtime normally sends all declared controls every tick.

HELD is a level. PRESSED/RELEASED are edges emitted only on the consuming native tick. A press has HELD|PRESSED; a release has RELEASED without HELD. To retain a tap between native ticks, the runtime queues press then release on distinct native ticks, never collapses both into one state. Catch-up cannot repeat PRESSED. Opposing H-gate gear states and low/high cannot be active together. Gears remain selected after releasing the grip, while wheel spring returns are computed by libacvr. Sequential up/down, coin and view gestures are at least one-native-tick pulses; low/high and H-gate are latched levels. On controller handoff while firing, queue a released tick before a new press as DR-89 does (`time-crisis-vr/quest/quest_host.c:685`). Reload is a one-tick **request**; each backend translates it to its own off-screen/trigger sequence and must not apply a second auto-trigger state machine accidentally.

TRACKED means a valid controller ray was available, independently of whether projection is off-screen. Loss clears trigger edges, sends untracked/off-screen and neutral axes/buttons. `COVER_PEDAL=1` means depressed/exposed, not ducked. Owner decision D43 supersedes the historical physical-ducking reference: cover is button-only, never head height. One gun defaults to either grip; two guns use each player's own grip. Hold = 1/exposed, release = 0/covered; invert/toggle are explicit options. Untracked input yields 0/covered. Cover works per player; Time Crisis converts at its existing >0.55 threshold.

### Time Crisis device mapping

Use the existing board gun globals, not a mouse or RawInput:

- x = floor(68 + clamp(nx,0,1) × 626), range 68..694.
- y = floor(43 + clamp(ny,0,1) × 241), range 43..284.
- `g_ss22_gun_off = OFFSCREEN || !TRACKED || reload_sequence_offscreen`.
- Assemble pressed MCU bits: trigger 0x0010, pedal/exposed 0x0020, coin 0x0001; send `ss22_snd_inputs(bits, 0x200, 0, 0)`.

Evidence: `namco22-decompile/engine/ss22_board.c:26` and `:32` return zero on off-screen port reads; `namco22-decompile/timecris/src/tc_game.c:66` defines bits; `time-crisis-vr/quest/quest_host.c:690` performs the conversion above. A reload request needs an off-screen trigger edge, including a released tick first if trigger is already down. Cover reload behavior also remains the game's logic. There is no dedicated Start bit in this TC table: declare coin and trigger/pedal, do not invent a Start port. An optional setup UI start action can drive an explicitly defined menu sequence later.

### Rave Racer device mapping

This clone's Rave Racer is System 22 and does not call `ss22_snd_inputs`. Its input state is `g_hw` in `rr_*` code. Set:

- steer = round(0x800 + clamp(steering,−1,1) × 0x580), clamped 0x280..0xD80.
- gas = round(clamp(accel,0,1) × 0x610); brake likewise.
- Preserve cabinet/DIP/idle fields in `g_hw.inputs`; clear active-low 0x0001 for GEAR_LOW and 0x0002 for GEAR_HIGH; release them by setting the bit. VIEW clears 0x0040. Player-1 coin clears 0x1000; optional player-2 coin clears 0x0200.

Evidence: ranges/idle=0xFEFF at `namco22-decompile/raverace/src/rr_game.c:19`, active-low helper at `namco22-decompile/raverace/src/rr_host.c:390`, the two shifter bits at `:736`, steering/pedals at `:745`. Source action names are SHIFT_DOWN/UP; mapping the requested low/high **latched** lever to those bits follows the project's control set, but actual gearbox behavior and required pulse/hold semantics need a bounded gameplay check. If the bits are sequential rather than low/high, advertise SHIFT_DOWN/UP and correct the profile in a separately authorized change. No Start bit appears in this host block either.

Let `rr_hw_vblank`/existing MCU I/O apply steer+32, gas+992, brake+3008 once; never add those in the adapter (`namco22-decompile/raverace/src/rr_hw.c:133`). Disable host keyboard/pad overwrites while libacvr owns inputs. Force feedback can tap decoded motor byte `rr_hw_motor_byte` at `:260`, plus later full-resolution torque capture; do not present keyboard ramp logic as the VR wheel spring.

### Supermodel input feed without RawInput

Add an adapter-owned path to the CInputs values used by CModel3, keeping desktop UI event pumping separate. The simplest initial route is a new `CInputs::ApplyAcvrSnapshot` that updates `prevValue` and `value` once per native step after UI-only polling, bypassing gameplay `Poll`. These fields are public (`Supermodel/Src/Inputs/Input.h:111`). Native Poll would overwrite them or advance gear/reload state (`Supermodel/Src/Inputs/Inputs.cpp:760`, `Supermodel/Src/Inputs/InputTypes.cpp:162` and `:203`). A custom CInputSource is an alternative, but it still requires clear poll timing and FFB interception; don't fabricate OS devices.

| Semantic | Feed and evidence |
|---|---|
| Steering | Piecewise map −1..0 to 0..128, 0..1 to 128..255; `steering->value`. Default CAxis range is 0/0x80/0xFF at `Supermodel/Src/Inputs/InputTypes.h:88`; ADC at `Supermodel/Src/Model3/Model3.cpp:585`. |
| Accelerator / brake | round(255×value), fields created at `Supermodel/Src/Inputs/Inputs.cpp:119`; ADCs at `Supermodel/Src/Model3/Model3.cpp:586`. |
| H-gate / sequential | Set `gearShift4` to 0/N or 1..4; process queued SHIFT_UP/DOWN once. Model3 consumes it at `Supermodel/Src/Model3/Model3.cpp:347`. Do not advertise gears 5/6/R without a game-specific route. |
| View | VIEW uses `viewChange`; VIEW_1..4 uses corresponding `vr[]` per loaded flags, `Supermodel/Src/Inputs/Inputs.cpp:134`. A four-view cabinet needs a setup policy for a single view-cycle ghost button. |
| Start / coin | `start[player]` / `coin[player]`, `Supermodel/Src/Inputs/Inputs.cpp:71`. All emitted switches are active-high at the CInputs boundary; CModel3 handles device polarity. |
| Light-gun family | round(150+501×nx), round(80+385×ny) into gunX/Y; raw trigger and offscreenValue in CTriggerInput. Ranges at `Supermodel/Src/Inputs/Inputs.cpp:190`, serial output at `Supermodel/Src/Model3/Model3.cpp:698` and `:722`. Adapter owns explicit reload sequence or feeds the original CTriggerInput state machine, never both. |
| Analog-gun family | X=round(255×nx), Y=round(255×(1−ny)), matching inverted analogGunY range at `Supermodel/Src/Inputs/Inputs.cpp:217`. Set analog trigger-left/right only where declared, no universal light-gun offscreen register. |

The requested four game profiles in `Supermodel/Config/Games.xml` declare: scud vehicle/shift4/vr4 at `:1418`; daytona2 the same at `:173`; srally2 vehicle/shift4/handbrake/viewchange at `:2026`; lostwsga analog_gun1/analog_gun2 at `:1109`. Lost World's read path additionally reorders axes and inverts Y again (`Supermodel/Src/Model3/Model3.cpp:610`). Match CInputs' expected Y convention first, then validate observed native hit position; do not remove that inversion based on comments. Off-screen/reload behavior for its analog-gun hardware is an explicit unknown. A setup must report unsupported reload or a proven per-game sequence rather than reuse the light-gun serial flags.

## Graphics ownership and HUD

Graphics records use **borrowed process-local handles**, never handles for another process. GL object names widen directly; pointers/dispatchable handles encode `(uint64_t)(uintptr_t)handle` and decode through uintptr_t; Vulkan non-dispatchable handles use their native 64-bit value. Native API casts stay in adapters. `api` must match the selected device and target. All native fields irrelevant to an API are zero. Unknown API/format combinations return `UNSUPPORTED` before recording work. `get_proc` is required for GL, optional for other APIs using their native loaders; proc_user remains valid until close. Function-address casts are platform loader conventions, outside portable C99 math.

| API | Device record | Target and command contract |
|---|---|---|
| GL/GLES | context is native current context, other native-device fields 0; GLES flag differentiates API family. Backend inherits the runtime's current context. | colour_image/depth_image are texture names (optional depth=0), framebuffer is runtime-created complete FBO with required layers attached, views 0, formats GL internal formats, states 0. Runtime binds FBO before draw; backend may use private intermediate FBOs but must finish in supplied FBO and restore it. Per-eye matrices use GL clip convention. |
| Vulkan | instance, physical_device, device and queue are native handles; queue_family explicit, context=0. Device/extensions must be XR-compatible before open. | images are VkImage, views VkImageView, formats VkFormat, states VkImageLayout, framebuffer=0. Runtime supplies a begun primary command buffer **outside a render pass**, with attachments already transitioned. Backend records and ends its own compatible render pass/dynamic-rendering block, returns outside it with layouts unchanged. No submit/wait/reset/end-command-buffer or queue ownership transfer by backend. Runtime finishes/submits and handles XR synchronization. |
| D3D11 | device=ID3D11Device pointer, context=immediate ID3D11DeviceContext pointer; other handles 0. | images=ID3D11Texture2D pointers, views=RTV/DSV pointers, formats=DXGI_FORMAT, framebuffer/states/command_buffer=0. Runtime supplies array-slice-compatible views; backend records on the same immediate context. Runtime owns references; backend takes none unless it explicitly AddRefs/releases a private retained device reference before close. No private Present or deferred command-list submission. |

Targets exist only for the callback. Backend may cache its own resources but cannot retain XR textures/RTVs/FBOs/command buffers beyond callback lifetime. Runtime acquires/waits XR images before draw, submits required GPU work safely before release/presentation, and owns final image layout/state. Call completion means work was recorded, not GPU completion. GPU resources referenced by work remain alive until runtime synchronization permits reuse/destruction; release of a CPU frame lease alone cannot free an in-flight VBO. Single-queue/owner-context sequencing permits updates for the next native frame in order. Backend sets all graphics state it needs; runtime reestablishes its own state before shared overlays. Format negotiation, sRGB writes and arcade gamma must avoid double correction. These are required future implementation checks; the supplied clones only prove the GL seams.

`acvr_eye` rectangles use lower-left GL origin, upper-left Vulkan/D3D target origin. Runtime provides matching projection, viewport and layer selection; backend must not apply another Y flip or viewport centre correction. Depth is optional in the ABI; if its renderer needs depth and none is supplied, the backend creates a private matching attachment. No compositor depth submission capability is claimed. New3D uses reversed depth (clear 0, GREATER: `Supermodel/Src/Graphics/New3D/New3D.cpp:519` and `:530`); the adapter must produce a matching eye projection and clear/test policy internally, or explicitly convert to the supplied projection/depth convention. It cannot reuse FrustumRZ while the eye matrix uses an incompatible convention.

Both adapters initially support GL single-eye draws. Multiview is negotiated independently and is an optimization, not an ABI requirement. DR-89 renders to an internal multiview array then blits each eye (`time-crisis-vr/quest/quest_gl.c:100`, `time-crisis-vr/quest/quest_host.c:640`). A runtime with separate XR swapchains can similarly supply an intermediate array target, then copy/blit; a shared XR array can be supplied directly. This contract does not promise Vulkan/D3D11 ports of GL-only backends.

Separate HUD is opt-in. Without it, backend keeps screen-space graphics merged at a projection-aligned depth; libacvr does not invent a HUD texture by screen capture. With it, world eye draws exclude exactly the classified HUD content; `game_draw_hud` runs into a runtime-owned transparent texture, normally only when frame_id changes and HUD_VALID is set. Invalid HUD clears the cached layer. Libacvr places it at user-configured comfortable depth, either head-locked or cockpit/cabinet-locked; shared pause UI/guns/ghost controls remain runtime overlays. World shot decals, mirror geometry, sky/fog, full-screen fades and text-priority masks must preserve composition. “All sprites are HUD” is wrong: namco22 interleaves sprites and polygons by depth (`namco22-decompile/engine/ss22_gl.c:505`), and RR masks text with polygon priorities (`namco22-decompile/raverace/src/rr_gl.c:430`). Supermodel bottom tile layer is not automatically HUD just because it is 2D. Advertise separation only after classification/blend/gamma tests pass; otherwise keep the merged path.

## Outputs and persistence

SOLENOID/LAMP events are normalized level changes on setup-declared channels. A 0-duration value persists until the next change; a timed pulse automatically ends. Time Crisis recoil channel 1 follows MCU bit 1, recoil_mask=0x0002 (`namco22-decompile/timecris/src/tc_game.c:207`). Queue the real rise/fall once per native tick; runtime synthesizes haptics on rising edges. DR-89's 45 ms rumble at `time-crisis-vr/quest/quest_host.c:709` is a reference profile, not a fixed ABI duration. Replaying geometry cannot fire another solenoid pulse.

FFB channel is the axis semantic (normally STEERING); effect distinguishes constant signed force, spring, friction, vibration and stop. A change replaces that effect for that player/axis; STOP cancels all of them. Supermodel emits separate effects at `Supermodel/Src/Model3/DriveBoard/WheelBoard.cpp:581`, `:601`, `:622`, `:641`. Preserve those distinctions; controller vibration cannot faithfully reproduce wheel torque. Runtime policy maps magnitude/effect to bounded haptics and does not support physical accessories (D39). On pause, device/session loss and close it cancels all active effects independently of backend events.

SCORE carries a signed 64-bit value and per-game channel; STATE carries a standard boot/attract/playing/game-over ID or setup extension ID. Unknown state is absence of an event, not a fabricated “playing.” TC has comments for mode/lives/timer at `namco22-decompile/timecris/src/tc_game.c:146`; that is limited evidence, not a shared scoring API. Supermodel COutputs explicitly describes mainly driving outputs (`Supermodel/Src/OSD/Outputs.h:35`); gun recoil/score is not assumed to exist. Add per-game verified readers before advertising events.

Output sequences start at 1 and increase within an open backend, including pause-induced stops. tick_id identifies the producing tick; pause events use the last completed tick (0 before first step). Poll buffers have capacity>0 and initialized slots, and overflow drops the oldest queued event while incrementing cumulative dropped. On any change in dropped, runtime cancels effects and records the loss. Queue adapters keep a separate latest-level map and, after draining the FIFO, return those persistent levels as fresh sequence-numbered events in bounded batches until state converges. Never re-enqueue an entire level map into an already-full queue; return MORE while deferred resynchronization remains. A missed short recoil pulse cannot be reconstructed and is reported as lost. Poll/drain after every native step reduces overflow risk. Persistent-save hook includes scores only when the game itself stores them; no generic high-score RAM schema or arbitrary save-state blob is promised. Existing periodic EEPROM saves can remain, but their paths must be confined to storage_root and error handling must be made observable.

## Namco22 integration plan

1. Pin a chosen engine commit in a future setup-owned working/build copy. Keep `(local reference clones)` read-only. Add a setup-local factory/C adapter and private game selection. Do not call the existing `ss22_main` as a nested blocking function and call it an implementation of `game_step`.
2. Time Crisis: extract initialization/cleanup from `namco22-decompile/engine/ss22_run.c:437`; retain board/program/DSP/sound/video initialization and native audio, replace window host/input/output symbols with adapter stubs. `ss22_host_open` must accept an already-selected runtime GL context, not create XR/window/pacing. `ss22_video_prepare` calls `ss22_prepare` at `namco22-decompile/engine/ss22_video.c:50` and `:66`. Return failures instead of exit/restart.
3. The lifted `g->entry()` never returns (`namco22-decompile/engine/ss22_run.c:546`). Initial implementation option: a private worker runs that stack, blocks at the native frame boundary, and receives a one-tick permit from game_step. Input latches happen before CPU resumes; it publishes frozen video registers/display-list data and blocks before the next tick. Move GL preparation to the owner thread. Audit pointers in ss22_regs; copy every referenced mutable bank or keep the producer blocked while preparing. This is a real scheduler change, not an existing callable tick: `rr_tick()` is a budget callback inside the generated CPU execution, not a way to advance the game independently. A resumable dispatcher is an alternative if investigated later. Avoid longjmp across live C++ frames.
4. Capture each emitted quad's original focal/centre and stable camera ID at geometry emission, before sorting. DR-89 demonstrates guarded additions in `time-crisis-vr/tools/patch_upstream.py:32`, scene collection at `:34`, and vertex reconstruction in quad_gl at `:40`. A richer camera record can also carry native clip/window and canonical view-local metadata. Direct screen-space quads retain a separate flat route; do not fake depth for them. Build a nearest-hit acceleration structure once during preparation, retaining each triangle's camera ID.
5. `game_step` prepares exactly once and issues a frame lease; `game_draw_eye` configures target/view/projection then calls the patched renderer's `ss22_draw`. The unpatched ss22_draw is flat GL 2.1 rendering, **not** ready-made stereo merely because it is repeatable. Implement the reconstruction/GL compatibility shim or equivalent shader path. `game_raycast` queries prepared triangles; `game_camera` copies captured records. Retain native projection on empty frames. Keep gun recoil model animation from changing the aim ray.
6. Supply TC input conversion above, intercept `ss22_out_poll` and decoded motor hooks before SDL rumble/network output, queue events and expose flush-persistent. `ss22_host_frame` no longer submits/paces; its adapter boundary serves the worker rendezvous. Existing host callbacks `input_update`, `input_neutral`, `snd_set_output`, `paused_tick` at `namco22-decompile/engine/ss22_host.h:17` are reusable concepts, not the full new ABI.
7. Rave Racer: implement a separate private adapter around `namco22-decompile/raverace/src/rr_main.c:228` and nonreturning entry at `:531`. Its preparation/draw seam is `rr_gl_prepare`/`rr_gl_draw` (`namco22-decompile/raverace/src/rr_gl.c:291` and `:363`), with input and EEPROM/motor through rr_hw. Reuse geometry capture in shared geo_hw/quad_gl, but add collection hooks to rr_gl as well as ss22_gl. Preserve RR direct-polygons, fog/gamma and priority text behavior. Do not substitute the Super 22 register path for RR MCU/shared-RAM updates.
8. Keep future guarded patches under the setup's own build tooling, not existing shared tools in this task. Require a known base commit, exact expected old fragments with occurrence counts, per-edit markers and idempotent verification; abort on drift rather than fuzzy replacement. DR-89 `edit` at `time-crisis-vr/tools/patch_upstream.py:5` checks old text and markers, but multiple edits to one file share markers; stronger per-edit receipts prevent silently skipping later additions. Its TC explosion dispatcher patch at `:17` is game-correctness work, not required by the ABI; audit separately before porting it.
9. Verify CPU frame counts/inputs/output edges first with a synthetic/no-content fixture. When the owner supplies a real local game run, compare native register aim at centre/corners, translated gun origins, per-camera hits, missed rays, button-only cover and short taps. Compare paired/replayed eye pixels against a single-view reference and confirm native logic/NVRAM are unaffected by display cadence. Headset refresh/performance/comfort remain separate hardware acceptance gates.

## Supermodel integration plan

1. Add a C wrapper around private CModel3/CInputs/COutputs and a New3D adapter in a future GPL setup fork. Initialize/load owner-supplied local content through its existing initialization path, attach inputs/outputs (`Supermodel/Src/Model3/Model3.cpp:3069` and `:3079`), keep GL resources on the runtime owner context, disable native window swaps/frame throttle.
2. Split simulation from presentation at `Supermodel/Src/Model3/Model3.cpp:1947`. Initial implementation uses single-threaded main-board mode to make publication explicit: apply input, advance board, SyncGPUs, perform exactly one native-camera LOS pass if required, advance sound/drive/net once in the existing relative order, prepare a stable frame. Preserve hardware latency of LOS publication. `RunFrame` currently includes RenderFrame and in multithreaded mode renders the prior snapshot before later synchronization; changing that order is a correctness risk. Threading optimization follows a passing single-thread baseline, not a renamed RunFrame call.
3. Separate New3D prepare from replay at `Supermodel/Src/Graphics/New3D/New3D.cpp:452` through `:498`: model traversal, dynamic mesh upload and texture/cache updates once per native snapshot. Retain nodes, original native projections, per-model matrices and immutable texture references for the lease. Restore per-eye mutable render state without mutating this snapshot. GPU/TileGen snapshot source remains stable; release/fences govern private GPU resources.
4. Preserve native LOS separately. `RenderFrame` swaps/clears LOS buffers at `Supermodel/Src/Graphics/New3D/New3D.cpp:445`, calls ProcessLos at `:527`, reads depth/stencil at `:1720`, and exposes results at `:1693`. Run that path once at native camera/raster/depth semantics per native tick, in a private offscreen target. Eye replay must neither publish nor recompute emulation LOS. Returning left-eye depth to emulated hardware would change gameplay.
5. Export original cameras during viewport traversal at `Supermodel/Src/Graphics/New3D/New3D.cpp:1101`. Treat `CalcViewport` at `:1618` as the split point: keep original frustum/viewport for hardware LOS and gun projection; select XR per-eye projection only for world eye replay. Replace desktop viewport/scissor with supplied eye rectangle as appropriate while preserving native subviewport masks, priorities, mirrors and UI. Do not multiply the game and XR projection matrices together. R3DShader projection upload at `Supermodel/Src/Graphics/New3D/R3DShader.cpp:365` receives the selected eye projection for that pass.
6. Preserve coordinate compensation and matrix #0 in `InitMatrixStack` (`Supermodel/Src/Graphics/New3D/New3D.cpp:1014`). Initially cache original model→canonical-native-view matrices; compose eye.view_from_scene × original model matrix at the model upload seam `Supermodel/Src/Graphics/New3D/R3DShader.cpp:391`, with scene anchored in that canonical view-local basis. Do not patch game RAM or reinterpret m_modelMat as a pure camera. A later global-world profile can factor known view separately to avoid double camera application. Include eye translation/IPD and head rotation; projection-only stereo has no correct 6DoF parallax.
7. Audit native frustum/LOD culling in viewport/node traversal (native planes at `Supermodel/Src/Graphics/New3D/New3D.cpp:1116`). Use conservative union-of-eyes bounds or traversal for the full desired XR frustum before caching geometry, and per-eye raster/depth afterward. Frozen native culling can hide geometry newly revealed by head motion; no contract can restore polygons the game never submitted. Keep lighting/fog/sorting in their intended spaces; transforms and reverse-depth helpers require tests.
8. `game_draw_eye` replays cached scene into backend-private New3D layer FBOs, then composites into the supplied target. New3D currently binds its own layered FBO and optional AA target and otherwise falls back to framebuffer 0 (`Supermodel/Src/Graphics/New3D/New3D.cpp:501`, `:550`, `:559`); replace final default-backbuffer assumptions with the callback target and per-eye sized intermediate resources. `CModel3::RenderFrame` currently invokes OSD presentation and TileGen bottom/GPU/top at `Supermodel/Src/Model3/Model3.cpp:2165`; bypass native swap and preserve composition. Add multiview only after the repeated single-eye path works.
9. Implement raycast from the prepared transformed scene, attaching original viewport camera IDs to eligible triangles; or an API-specific ID/depth query with an exact canonical-position reconstruction. Plain left-eye depth cannot identify the original camera in overlapping viewports. Test Lost World aims against the analog input path above. Culling, translucent surfaces, sky, decals and mirrors need defined hit policy before advertising RAYCAST.
10. Feed gameplay inputs through ApplyAcvrSnapshot with no RawInput requirement. Translate analog guns separately from light guns, preserve existing device-level inversion, and verify reload policy per game. Add COutputs sink via SendOutput/GetValue (`Supermodel/Src/OSD/Outputs.h:149` and `:118`, `Supermodel/Src/OSD/Outputs.cpp:95`); intercept steering SendForceFeedbackCmd before a physical-device backend consumes it.
11. Verify sim counts and native LOS values remain identical for one, two and repeated eye draws of the same snapshot; test input mapping against loaded game flags and native calibration screens. Validate asymmetrical projections, head translation, cockpit/mirror viewports and layer blending on no-content geometry before owner-operated game/headset tests. Model3 score/gun-recoil extraction and save-state compatibility remain explicit future work.

## Suggested architecture changes, gaps and owner questions

Suggested changes to `docs/architecture.md` are recorded here only: replace the void draft callbacks with negotiated lifecycle/frame leases; add gun raycast/camera-ID association; distinguish RR's rr_* route from TC's ss22_*; state that Supermodel needs CPU/render/LOS separation; document native-camera metadata separately from XR eye matrices; treat HUD classification and graphics bindings as capabilities rather than universally available features.

Known gaps and risks:

- No libacvr runtime, engine patches, C++ wrapper, OpenXR backend, build system or in-headset verification was implemented in this scoping task. Header compilation proves syntax only, not binary interoperability or stereo behavior.
- Persistent-frame CPU scheduling for the nonreturning lifted namco22 execution needs implementation/proof. RR has a distinct renderer and MCU path. Existing DR-89 guarded edits target a different upstream revision.
- Prepared-frame replay must be side-effect-free for game state. Supermodel LOS is the clearest violation in current code; texture caches, matrix state, asynchronous audio and GPU resource lifetimes need the same audit.
- Per-polygon native projection is available to capture; global world/cockpit camera and physical scale are not universally extracted. A VIEW_LOCAL baseline is honest but limits car-relative motion/comfort features and camera-cut detection.
- Lost World analog-gun calibration, Y inversion and reload are unproven; RR's requested low/high shifter versus source shift-up/down behavior is unproven. No analog-gun off-screen register is invented.
- Original off-frustum geometry, sprite depth, HUD priority masks, mirror subviewports, reversed-Z and transparent layers can break 6DoF reconstruction or gun targets. Preserve a merged HUD until a separated layer is verified.
- Vulkan and D3D11 represent contract capacity, not implemented renderer ports. GLES versus desktop GL and runtime-supported formats/extensions need real device negotiation tests. Requested headset refresh does not demonstrate achieved frame rate.
- Input transition queues and output queues are required new behavior; native coin pulse length, shifter latching, reload delays and channel calibration remain setup-specific. No game-specific score readers are promised.
- ABI v1 has no cross-process transport, cross-thread public control, hot device recovery, generic state serialization, scene streaming, native audio callback or output-channel enumeration API. Keep name/channel definitions in setup configuration initially; revisit only when a real integration requires them.

Owner decisions to settle before implementation:

1. Confirm GL first for both adapters, keeping Vulkan/D3D11 as reserved negotiated paths.
2. Choose the first proof pair (Time Crisis + Rave Racer) and Supermodel proof pair (Scud + Lost World), subject to the analog-gun/reload findings.
3. Choose physical world scale and no-hit far-ray distance per setup; decide whether early view-local anchoring is sufficient for racing comfort.
4. Confirm RR low/high lever semantics with a real cabinet/game check; decide the four-view-cycle mapping for Scud/Daytona and how unsupported reload/gear states should appear in setup UI.
5. Choose Supermodel's initial 60 Hz versus 57.524 Hz mode after the audio/board timing audit, and a catch-up budget (suggested initial 4, with overload diagnostics).
6. Confirm initial merged HUD and controller vibration as FFB feedback; approve separate HUD or physical wheel work only after the basic contracts are proven.

## Verification receipt

Passed: MSVC C syntax check, exit 0, warnings treated as errors. MSVC was discovered through the installed vswhere utility outside PATH; no compiler was installed. MSVC's C11 mode was used because it has no C99 mode; the header uses C99 declarations/types and no C11-specific syntax. Validation command, run from `the repo root`:

```bat
call "<local-root>/Program Files\Microsoft Visual Studio\18\Community\VC\Auxiliary\Build\vcvars64.bat"
cl /nologo /std:c11 /Zs /TC /W4 /WX "libacvr/include/acvr.h"
```

An automated read-only citation audit checked all explicit absolute file:line references for existence/range and verified every public typedef has a named rationale entry. Source claims and callback semantics were also reviewed against the cited local seams, including reverse-Z matrix construction at `Supermodel/Src/Graphics/New3D/Mat4.cpp:185`. No runtime linking, graphics execution, binary-layout comparison, engine patch application or headset acceptance was tested. No reference-source edits, engine launches, game-content access, push or main-branch commit occurred. The optional fake backend is omitted; it would not prove the real scheduling/LOS/stereo seams.


## v0.1 additions and compatibility

The ABI major and struct version remain 1. All existing field offsets and
callback signatures are unchanged. `ACVR_HIT_V1_SIZE` and
`ACVR_RUNTIME_CONFIG_V1_SIZE` identify the complete original records. Accept
these original sizes, the complete new size, or larger future tails; reject
partial v0.1 tails with BAD_ARGUMENT. Never read/write beyond caller size.
A reader of the old prefix ignores the tail; a new producer writing an old hit
returns position/camera only. An old producer leaves a new caller's zeroed hit
flags untouched, so the runtime uses camera projection. Initialize output tails
to zero before each query. New type records require their complete v0.1 size.
`ACVR_INIT` still sets only size/version, so zero the entire object first.

The runtime config tail optionally supplies stride-aware gun slots and a policy.
Count zero permits NULL and selects the single-gun, no-model compatibility
path; zero-filled tail is accepted in this case. Nonzero count requires complete
policy, unique slots and players, valid hand IDs, model strings and nodes,
finite 0..1 tints, unit quaternions, and backend-supported counts. Runtime copies
all retained strings/config during create; no array or string pointer is retained.
Models use metres and one root grip; model-to-stage is tracked grip multiplied
by calibrated angle and inverse model grip. The muzzle ray uses the bind-pose
muzzle transform and local -Z, independently of rendered recoil/pump animation.
Scale to scene units exactly once. Runtime draws the model after the world,
depth-tested, with optional laser/dot and per-slot linear body/accent tints.

Slot 0/player 0 defaults right; slot 1/player 1 defaults left. Changing p1_hand
swaps assignments. `two_guns=ON_JOIN` shows the off hand faint until a Start/join
request is confirmed by a backend player state; a button alone does not prove
joining. `ALWAYS` requests join when the game's declared inputs support it,
without inventing credits or gameplay state. Without reliable player state,
keep it unconfirmed and report the limitation. A setup may use STATE events
with player and PLAYING/GAME_OVER only when it has verified per-player readers.
`OFF`, separate views or a one-gun backend suppress slot 1. Single-gun hand
switch happens only on the other trigger's rising edge; that edge changes hands
without also firing. Two active guns disable hand switching and two-hand reload.
Start/coin are distinct bound actions. Time Crisis declares no invented Start
port: its initial control set uses existing trigger/pedal/coin behavior.

Cover is `ACVR_AXIS_COVER_PEDAL` for each player, value 0..1, using D43 above.
Lost pose validity suppresses that gun's fire/pedal, cancels its haptics, and
requires a fresh release/press before firing after recovery. Model visibility
and tracking validity are independent; hidden models can still aim when tracked.

Named-node gun events drive visual motion from metadata; button/axis values
interpolate the defined range, recoil pulses are finite. Sequences increase per
slot across all input/output-derived events. Duplicate/out-of-order host preview
injections return BAD_ARGUMENT; negative/nonfinite values or unknown nodes do
likewise. The host preview export is optional for consumers; board adapters only
publish existing output events. Runtime arbitrates output-based versus fallback
trigger recoil once, so stereo replay cannot duplicate animation or haptics.
Pause/close/loss clears motion levels and active haptics. No animated node may
change the aim ray. Motion/LOD metadata is defined in the control-set contract.

`acvr_runtime_get_tracking` copies stage-space head and both hands after a tick,
with explicit validity bits and the same monotonic prediction time used for eye
draws. The runtime preserves all output prefixes and caller-owned tail bytes.
Nested pose/hand records require their own initialized prefixes. No callback
recursion or cross-thread calls. Matrices supplied to the backend already include
these poses, the anchor and scale: do not apply them again. These exports and the
XR/gun renderer remain declarations until the lead's runtime block implements them.
