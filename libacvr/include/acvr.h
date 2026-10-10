/* SPDX-License-Identifier: MIT  (libacvr/ is MIT-licensed: see libacvr/LICENSE; the rest of AladdinsCastle is GPL-3.0)
 * Contract v0.1, 2026-10-10. Declarations only; no runtime implementation yet.
 * Normative semantics and source evidence: docs/libacvr-contract.md.
 */
#ifndef ACVR_H_INCLUDED
#define ACVR_H_INCLUDED

#include <stdint.h>
#include <stddef.h>

#if defined(_WIN32)
#define ACVR_CALL __cdecl
#else
#define ACVR_CALL
#endif
#ifndef ACVR_API
#define ACVR_API /* setup chooses static linkage or its DLL export decoration */
#endif
#ifdef __cplusplus
extern "C" {
#endif

#define ACVR_ABI_VERSION 1u
#define ACVR_CONTRACT_MAJOR 0u
#define ACVR_CONTRACT_MINOR 1u
#define ACVR_STRUCT_VERSION 1u
#define ACVR_INIT(p) do { (p)->size = (uint32_t)sizeof(*(p)); \
                         (p)->version = ACVR_STRUCT_VERSION; } while (0)
#define ACVR_NO_CAMERA UINT32_MAX

typedef struct acvr_runtime acvr_runtime;
typedef struct acvr_backend acvr_backend;
typedef struct acvr_frame acvr_frame;
typedef int32_t acvr_result;
#define ACVR_OK             ((acvr_result)0)
#define ACVR_MORE           ((acvr_result)1) /* poll has additional queued events */
#define ACVR_STOPPED        ((acvr_result)2) /* tick: session ended normally */
#define ACVR_ERROR          ((acvr_result)-1)
#define ACVR_BAD_ARGUMENT   ((acvr_result)-2)
#define ACVR_BAD_VERSION    ((acvr_result)-3)
#define ACVR_UNSUPPORTED    ((acvr_result)-4)
#define ACVR_BAD_STATE      ((acvr_result)-5)

/* All tags and flags have uint32_t storage; no enum, bool, long or bitfields. */
#define ACVR_GRAPHICS_GL       1u /* desktop OpenGL or GLES; device flags say which */
#define ACVR_GRAPHICS_VULKAN   2u
#define ACVR_GRAPHICS_D3D11    3u
#define ACVR_GRAPHICS_BIT(api) (1u << (api))
#define ACVR_DEVICE_GLES       1u

#define ACVR_CAP_MULTIVIEW     0x0001u
#define ACVR_CAP_SEPARATE_HUD  0x0002u
#define ACVR_CAP_RAYCAST       0x0004u
#define ACVR_CAP_OUTPUTS       0x0008u
#define ACVR_CAP_PERSISTENCE   0x0010u

#define ACVR_CONTROL_AXIS     1u
#define ACVR_CONTROL_BUTTON   2u
#define ACVR_CONTROL_GUN      3u /* semantic is the zero-based gun slot */
#define ACVR_AXIS_STEERING     1u /* -1 left .. +1 right */
#define ACVR_AXIS_ACCELERATOR  2u /* 0 released .. 1 fully applied */
#define ACVR_AXIS_BRAKE        3u
#define ACVR_AXIS_LEAN         4u /* -1 left .. +1 right */
#define ACVR_AXIS_PEDAL_SPEED  5u /* 0..1; profile maps physical cadence */
#define ACVR_AXIS_STICK_X      6u /* -1..1 */
#define ACVR_AXIS_STICK_Y      7u /* -1 down .. +1 up */
#define ACVR_AXIS_LEVER        8u /* 0..1 */
#define ACVR_AXIS_COVER_PEDAL  9u /* 1 exposed/pedal depressed; 0 covered */
#define ACVR_AXIS_REAR_BRAKE  10u
#define ACVR_BUTTON_GEAR_LOW   1u
#define ACVR_BUTTON_GEAR_HIGH  2u
#define ACVR_BUTTON_GEAR_N     3u
#define ACVR_BUTTON_GEAR_R     4u
#define ACVR_BUTTON_GEAR_1     5u
#define ACVR_BUTTON_GEAR_2     6u
#define ACVR_BUTTON_GEAR_3     7u
#define ACVR_BUTTON_GEAR_4     8u
#define ACVR_BUTTON_GEAR_5     9u
#define ACVR_BUTTON_GEAR_6    10u
#define ACVR_BUTTON_SHIFT_UP  11u
#define ACVR_BUTTON_SHIFT_DOWN 12u
#define ACVR_BUTTON_VIEW     13u
#define ACVR_BUTTON_START    14u
#define ACVR_BUTTON_COIN     15u
#define ACVR_BUTTON_HANDBRAKE 16u
#define ACVR_BUTTON_VIEW_1   17u
#define ACVR_BUTTON_VIEW_2   18u
#define ACVR_BUTTON_VIEW_3   19u
#define ACVR_BUTTON_VIEW_4   20u
#define ACVR_SEMANTIC_EXTENSION_BASE 0x10000u /* setup-local IDs, declared in info */
#define ACVR_INPUT_HELD       1u
#define ACVR_INPUT_PRESSED    2u
#define ACVR_INPUT_RELEASED   4u
#define ACVR_GUN_TRACKED      1u
#define ACVR_GUN_OFFSCREEN    2u
#define ACVR_GUN_RELOAD       4u /* explicit one-native-tick request, not a trigger */

typedef struct acvr_control_desc {
    uint32_t size, version;
    uint32_t kind, semantic, player; /* players and gun slots are zero-based */
    float minimum, maximum;        /* canonical range, never cabinet ADC counts */
    const char *name_utf8;          /* borrowed until game_close */
} acvr_control_desc;

typedef struct acvr_gun_input {
    uint32_t size, version;
    uint32_t slot, player, flags, trigger; /* trigger uses ACVR_INPUT_* */
    uint64_t aim_frame_id;          /* source frame, may precede consuming step */
    uint32_t camera_id;             /* frame-local ID; NO_CAMERA if no projection */
    float screen_x, screen_y;       /* full native raster: (0,0) top-left, (1,1) bottom-right */
} acvr_gun_input;

typedef struct acvr_axis_input {
    uint32_t size, version;
    uint32_t semantic, player;
    float value;                   /* finite, already curved/deadzoned by runtime */
} acvr_axis_input;

typedef struct acvr_button_input {
    uint32_t size, version;
    uint32_t semantic, player, state; /* ACVR_INPUT_*; edges once per native tick */
} acvr_button_input;

/* Arrays use byte stride, never implicit sizeof across an ABI boundary. */
typedef struct acvr_inputs {
    uint32_t size, version;
    uint64_t tick_id;
    int64_t sample_time_ns;         /* runtime monotonic domain; not raw XrTime */
    uint32_t gun_count, gun_stride;
    const acvr_gun_input *guns;
    uint32_t axis_count, axis_stride;
    const acvr_axis_input *axes;
    uint32_t button_count, button_stride;
    const acvr_button_input *buttons;
} acvr_inputs;

/* Handles are process-local: GL names widen directly; pointer handles cast via
 * uintptr_t; Vulkan non-dispatchable handles encode their 64-bit value.
 * Native API headers intentionally stay outside the ABI. */
typedef struct acvr_graphics_device {
    uint32_t size, version;
    uint32_t api, flags;
    uint64_t instance, physical_device, device, context, queue;
    uint32_t queue_family;
    void *proc_user;
    void *(ACVR_CALL *get_proc)(void *user, const char *name_utf8);
} acvr_graphics_device;

typedef struct acvr_open_info {
    uint32_t size, version;
    const char *game_id_utf8;
    const char *content_root_utf8;  /* owner-provided local path; never acquired by runtime */
    const char *storage_root_utf8;  /* writable settings/NVRAM root */
    const char *backend_options_utf8; /* opaque merged setup options, copied if retained */
    acvr_graphics_device graphics;
} acvr_open_info;

typedef struct acvr_backend_info {
    uint32_t size, version;
    uint32_t capabilities;
    uint32_t native_rate_num, native_rate_den; /* native ticks/second; positive */
    uint32_t player_count, gun_count;
    float scene_units_per_metre;   /* positive; reconstructed metre-space uses 1 */
    uint32_t control_count, control_stride;
    const acvr_control_desc *controls; /* immutable borrowed declarations until close */
} acvr_backend_info;

typedef struct acvr_step_info {
    uint32_t size, version;
    uint64_t tick_id;               /* consecutive from 1, including catch-up ticks */
    int64_t simulation_time_ns;     /* starts at 0; rational-rate accumulation */
} acvr_step_info;

#define ACVR_FRAME_CAMERA_CUT  1u
#define ACVR_FRAME_HUD_VALID   2u
typedef struct acvr_frame_info {
    uint32_t size, version;
    uint64_t frame_id;             /* equal to producing tick_id */
    uint32_t flags, camera_count, primary_camera_id;
    uint32_t hud_width, hud_height; /* preferred raster when separated; otherwise 0 */
} acvr_frame_info;

#define ACVR_CAMERA_VIEW_LOCAL 1u /* reconstructed camera space; view is identity */
#define ACVR_CAMERA_RETAINED   2u /* no current geometry; last valid projection */
typedef struct acvr_game_camera {
    uint32_t size, version;
    uint32_t camera_id, flags;
    uint32_t raster_width, raster_height;
    float focal_x_px, focal_y_px, centre_x_px, centre_y_px;
    float viewport_px[4];          /* left, top, width, height in native raster */
    float view_from_scene[16];     /* column-major; canonical RH +X right,+Y up,-Z forward */
    float near_scene, far_scene;   /* positive; far=0 denotes infinite */
} acvr_game_camera;

typedef struct acvr_ray {
    uint32_t size, version;
    float origin_scene[3], direction_scene[3]; /* direction unit length */
    float max_distance_scene;      /* positive finite */
} acvr_ray;

typedef struct acvr_hit {
    uint32_t size, version;
    uint32_t found, camera_id;      /* no hit: found=0, camera_id=NO_CAMERA */
    float position_scene[3], distance_scene;
    /* v0.1 optional tail. Read/write only when size covers the entire tail. */
    uint32_t flags;
    float screen_x, screen_y;       /* normalized native raster, NOT cabinet ADCs */
} acvr_hit;
#define ACVR_HIT_V1_SIZE ((uint32_t)offsetof(acvr_hit, flags))
#define ACVR_HIT_GUN_COORDS 1u      /* hit coordinates valid; may be outside [0,1] */

typedef struct acvr_render_target {
    uint32_t size, version;
    uint32_t api, width, height, array_layers, sample_count;
    int64_t colour_format, depth_format; /* native API format IDs; depth=0 if absent */
    uint64_t colour_image, depth_image, colour_view, depth_view, framebuffer;
    uint32_t colour_state, depth_state; /* VkImageLayout for Vulkan; 0 otherwise */
} acvr_render_target;

typedef struct acvr_eye {
    uint32_t size, version;
    uint32_t eye_index, array_layer;
    int32_t rect_x, rect_y;
    uint32_t rect_width, rect_height;
    float view_from_scene[16];      /* latest pose + anchor + scene scale, already composed */
    float projection_from_view[16]; /* target API clip/depth convention, off-axis XR frustum */
} acvr_eye;

typedef struct acvr_draw_info {
    uint32_t size, version;
    uint64_t frame_id, display_id;
    int64_t predicted_display_time_ns; /* same runtime monotonic domain as input */
    acvr_render_target target;
    uint64_t command_buffer;       /* recording primary VkCommandBuffer, 0 on GL/D3D11 */
    uint32_t view_count, view_stride;
    const acvr_eye *views;          /* draw_eye:1; multiview:2 sharing one target */
} acvr_draw_info;

#define ACVR_OUTPUT_SOLENOID   1u
#define ACVR_OUTPUT_LAMP       2u
#define ACVR_OUTPUT_FFB        3u
#define ACVR_OUTPUT_SCORE      4u
#define ACVR_OUTPUT_STATE      5u
#define ACVR_FFB_CONSTANT      1u /* signed strength -1..1 */
#define ACVR_FFB_SPRING        2u /* 0..1 */
#define ACVR_FFB_FRICTION      3u /* 0..1 */
#define ACVR_FFB_VIBRATION     4u /* 0..1 */
#define ACVR_FFB_STOP          5u /* strength=0; cancel all effects on this axis */
#define ACVR_STATE_BOOT        1u
#define ACVR_STATE_ATTRACT     2u
#define ACVR_STATE_PLAYING     3u
#define ACVR_STATE_GAME_OVER   4u
typedef struct acvr_output_event {
    uint32_t size, version;
    uint64_t sequence, tick_id;     /* FIFO sequence, never generated by replay draws */
    uint32_t kind, player, channel, effect; /* FFB channel = axis semantic */
    float strength;                /* lamp/solenoid level 0..1, or FFB strength */
    uint32_t duration_ms;          /* 0 = level persists until next event/stop */
    int64_t value;                 /* score or state ID; other kinds use 0 */
} acvr_output_event;

typedef struct acvr_outputs {
    uint32_t size, version;
    uint32_t capacity, stride;      /* caller-owned initialized event slots */
    acvr_output_event *events;
    uint32_t count;                /* written; <= capacity */
    uint64_t dropped;              /* cumulative overflow count since open; never hidden */
} acvr_outputs;

/* Runtime -> backend. All calls serialized on the runtime/graphics owner thread.
 * Mandatory through game_poll_outputs (which may return an empty batch).
 * Later callbacks are optional and gated by capabilities. No exceptions cross C.
 * Exactly one frame lease at a time: release it before the next game_step.
 */
typedef struct acvr_backend_api {
    uint32_t size, version;
    uint32_t abi_version, supported_graphics; /* ACVR_GRAPHICS_BIT mask */
    acvr_result (ACVR_CALL *game_open)(const acvr_open_info *, acvr_backend **,
                                     acvr_backend_info *);
    void (ACVR_CALL *game_close)(acvr_backend *);
    acvr_result (ACVR_CALL *game_pause)(acvr_backend *, uint32_t paused);
    acvr_result (ACVR_CALL *game_set_inputs)(acvr_backend *, const acvr_inputs *);
    acvr_result (ACVR_CALL *game_step)(acvr_backend *, const acvr_step_info *,
                                     acvr_frame **, acvr_frame_info *);
    void (ACVR_CALL *game_release_frame)(acvr_backend *, acvr_frame *);
    acvr_result (ACVR_CALL *game_camera)(acvr_backend *, const acvr_frame *,
                                       uint32_t camera_id, acvr_game_camera *);
    acvr_result (ACVR_CALL *game_draw_eye)(acvr_backend *, const acvr_frame *,
                                         const acvr_draw_info *);
    acvr_result (ACVR_CALL *game_poll_outputs)(acvr_backend *, acvr_outputs *);
    acvr_result (ACVR_CALL *game_draw_multiview)(acvr_backend *, const acvr_frame *,
                                               const acvr_draw_info *);
    acvr_result (ACVR_CALL *game_raycast)(acvr_backend *, const acvr_frame *,
                                        const acvr_ray *, acvr_hit *);
    acvr_result (ACVR_CALL *game_draw_hud)(acvr_backend *, const acvr_frame *,
                                         const acvr_draw_info *);
    acvr_result (ACVR_CALL *game_flush_persistent)(acvr_backend *);
} acvr_backend_api;

/* Setup factory: fills only caller-supported table bytes, validates ABI version.
 * Multiple statically linked backends use setup-local factories of this signature.
 */
typedef acvr_result (ACVR_CALL *acvr_backend_query_fn)(uint32_t, acvr_backend_api *);
ACVR_API acvr_result ACVR_CALL acvr_backend_query(uint32_t abi_version,
                                                acvr_backend_api *out_api);

#define ACVR_ANCHOR_RAIL       1u
#define ACVR_ANCHOR_COCKPIT    2u

#define ACVR_HAND_RIGHT 0u
#define ACVR_HAND_LEFT  1u
#define ACVR_TWO_GUNS_OFF     0u
#define ACVR_TWO_GUNS_ON_JOIN 1u
#define ACVR_TWO_GUNS_ALWAYS  2u
#define ACVR_LASER_OFF  0u
#define ACVR_LASER_LINE 1u
#define ACVR_LASER_DOT  2u

typedef struct acvr_pose {
    uint32_t size, version;
    float position_m[3], orientation_xyzw[4]; /* unit quaternion; RH, +Y up, -Z forward */
} acvr_pose;

typedef struct acvr_gun_slot_config {
    uint32_t size, version;
    uint32_t slot, player, hand;
    const char *model_id_utf8, *model_path_utf8, *metadata_path_utf8;
    const char *grip_node_utf8, *muzzle_node_utf8; /* required named glTF nodes */
    float body_rgba[4], accent_rgba[4]; /* linear RGBA, each channel 0..1 */
    float angle_xyzw[4];            /* unit quaternion, calibrated gun-in-grip rotation */
    uint32_t show_gun, laser_mode;   /* show_gun: 0/1; laser independent of model visibility */
} acvr_gun_slot_config;

typedef struct acvr_gun_policy {
    uint32_t size, version;
    uint32_t two_guns, p1_hand, hand_switch; /* hand_switch: 0 off, 1 other trigger */
    uint32_t shared_view;           /* 0 forces one gun, even if policy requests two */
    float unjoined_alpha;          /* 0..1; faint off-hand model, never active input */
} acvr_gun_policy;

#define ACVR_POSE_POSITION_VALID    1u
#define ACVR_POSE_ORIENTATION_VALID 2u
typedef struct acvr_hand_tracking {
    uint32_t size, version;
    uint32_t hand, grip_flags, aim_flags;
    acvr_pose grip, aim;            /* stage-space metres, predicted for this display */
} acvr_hand_tracking;

typedef struct acvr_tracking {
    uint32_t size, version;
    uint64_t display_id;
    int64_t sample_time_ns, predicted_display_time_ns;
    uint32_t head_flags;
    acvr_pose head;
    acvr_hand_tracking right, left; /* same prediction/time domain as acvr_draw_info */
} acvr_tracking;

#define ACVR_GUN_EVENT_RECOIL 1u
#define ACVR_GUN_EVENT_BUTTON 2u
#define ACVR_GUN_EVENT_AXIS   3u
typedef struct acvr_gun_event {
    uint32_t size, version;
    uint64_t sequence, tick_id;     /* monotonically increasing per slot; never per-eye */
    uint32_t slot, kind;
    const char *node_utf8;          /* named motion node in model metadata */
    float value;                   /* normalized motion 0..1; recoil amplitude */
    uint32_t duration_ms;           /* recoil pulse; 0 for button/axis levels */
} acvr_gun_event;

typedef struct acvr_runtime_config {
    uint32_t size, version;
    uint32_t graphics_api, anchor_mode;
    float requested_refresh_hz;    /* 0 = runtime default; request, never promise */
    float fallback_aim_distance_m; /* positive ray distance, not DR-89's fixed-Z plane */
    uint32_t max_catchup_ticks;    /* positive; bounded work per XR display frame */
    const char *game_id_utf8, *content_root_utf8, *storage_root_utf8;
    const char *merged_controls_path_utf8; /* runtime owns TOML ghost/binding parsing */
    const char *backend_options_utf8;
    /* v0.1 optional tail; old prefix selects no models and one gun. */
    uint32_t gun_slot_count, gun_slot_stride;
    const acvr_gun_slot_config *gun_slots;
    acvr_gun_policy gun_policy;
} acvr_runtime_config;
#define ACVR_RUNTIME_CONFIG_V1_SIZE ((uint32_t)offsetof(acvr_runtime_config, gun_slot_count))

/* Proposed libacvr exports, declarations only. Create owns XR/device initialization
 * and calls game_open; tick handles one XR frame (0..N native ticks, 0..2 draws);
 * destroy releases the frame, flushes if supported, closes backend then graphics.
 * All exports are owner-thread-only, including pause; destroy accepts NULL.
 */
ACVR_API acvr_result ACVR_CALL acvr_runtime_create(const acvr_runtime_config *,
                                                  const acvr_backend_api *,
                                                  acvr_runtime **);
ACVR_API acvr_result ACVR_CALL acvr_runtime_tick(acvr_runtime *);
ACVR_API acvr_result ACVR_CALL acvr_runtime_set_paused(acvr_runtime *, uint32_t paused);
/* Host-side inspection after tick; BAD_STATE before a valid tracking sample.
 * Backend callbacks must not recursively call these exports. */
ACVR_API acvr_result ACVR_CALL acvr_runtime_get_tracking(acvr_runtime *, acvr_tracking *);
/* Optional host preview/test injection, not required from a board backend.
 * Runtime normally derives these events from controls and backend outputs. */
ACVR_API acvr_result ACVR_CALL acvr_runtime_gun_event(acvr_runtime *, const acvr_gun_event *);
/* Frees the runtime even on flush failure; returns that failure for reporting. */
ACVR_API acvr_result ACVR_CALL acvr_runtime_destroy(acvr_runtime *);

#ifdef __cplusplus
}
#endif
#endif
