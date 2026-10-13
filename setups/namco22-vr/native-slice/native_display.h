/* SPDX-License-Identifier: GPL-3.0-only */
#ifndef ACN22_NATIVE_DISPLAY_H
#define ACN22_NATIVE_DISPLAY_H
#include "pose_stage.h"
typedef struct {
    uint64_t frame, display, target_fingerprint;
    int64_t predicted_ns;
    acvr_eye eye; /* copied value only: no pointer or target handle */
} acn22_display_key;
typedef struct {
    acn22_display_key key[2];
    acn22_pose_fragment packets[2][4];
    uint32_t count[2];
    int prepared, next_eye;
} acn22_display_pair;
acvr_result acn22_display_stage(const geo_quad *,uint32_t,uint64_t,
    uint32_t,uint32_t,const acvr_draw_info *,acn22_display_pair *);
acvr_result acn22_display_match(const acn22_display_pair *,const acvr_draw_info *);
#endif
