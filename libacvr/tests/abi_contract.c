/* SPDX-License-Identifier: MIT
 * Header ABI/layout checks, not a mock runtime or headset acceptance test. */
#include "acvr.h"
#include <string.h>
#ifdef __cplusplus
#define CHECK(c) static_assert(c, #c)
#else
#define CHECK(c) _Static_assert(c, #c)
#endif

typedef struct old_hit {
    uint32_t size, version, found, camera_id;
    float position_scene[3], distance_scene;
} old_hit;
typedef struct old_config {
    uint32_t size, version, graphics_api, anchor_mode;
    float requested_refresh_hz, fallback_aim_distance_m;
    uint32_t max_catchup_ticks;
    const char *game_id_utf8, *content_root_utf8, *storage_root_utf8;
    const char *merged_controls_path_utf8, *backend_options_utf8;
} old_config;

CHECK(ACVR_ABI_VERSION == 1u);
CHECK(sizeof(float) == 4);
CHECK(sizeof(acvr_result) == 4);
CHECK(sizeof(old_hit) == ACVR_HIT_V1_SIZE);
CHECK(sizeof(old_config) == ACVR_RUNTIME_CONFIG_V1_SIZE);
CHECK(offsetof(old_hit, distance_scene) == offsetof(acvr_hit, distance_scene));
CHECK(offsetof(old_config, backend_options_utf8) == offsetof(acvr_runtime_config, backend_options_utf8));
CHECK(offsetof(acvr_backend_api, game_open) == 16);
CHECK(offsetof(acvr_gun_slot_config, size) == 0);
CHECK(offsetof(acvr_tracking, version) == 4);
CHECK(ACVR_AXIS_COVER_PEDAL == 9u);

/* Compile the exact existing callback signature from both C and C++. */
static acvr_result ACVR_CALL draw(acvr_backend *b, const acvr_frame *f,
                                  const acvr_draw_info *d) {
    (void)b; (void)f; (void)d;
    return ACVR_UNSUPPORTED;
}
int main(void) {
    acvr_backend_api api;
    acvr_runtime_config config;
    acvr_gun_slot_config slots[2];
    memset(&api, 0, sizeof(api)); ACVR_INIT(&api);
    memset(&config, 0, sizeof(config)); ACVR_INIT(&config);
    memset(slots, 0, sizeof(slots)); ACVR_INIT(&slots[0]); ACVR_INIT(&slots[1]);
    ACVR_INIT(&config.gun_policy);
    api.game_draw_eye = draw;
    config.gun_slots = slots;
    config.gun_slot_count = 2;
    config.gun_slot_stride = (uint32_t)sizeof(slots[0]);
    slots[0].hand = ACVR_HAND_RIGHT;
    slots[1].slot = slots[1].player = 1;
    slots[1].hand = ACVR_HAND_LEFT;
    if (api.size != sizeof(api) || api.version != 1 ||
        config.gun_policy.size != sizeof(acvr_gun_policy) ||
        (const unsigned char *)&slots[1] - (const unsigned char *)config.gun_slots != config.gun_slot_stride)
        return 1;
    return api.game_draw_eye(NULL, NULL, NULL) == ACVR_UNSUPPORTED ? 0 : 1;
}
