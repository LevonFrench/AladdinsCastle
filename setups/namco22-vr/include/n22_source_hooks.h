/* SPDX-License-Identifier: GPL-3.0-only
 * Private build-time engine hooks; never part of libacvr's public ABI.
 * void pointers borrow ss22_regs/geo_quad/geo_view only during the callback.
 */
#ifndef N22_SOURCE_HOOKS_H
#define N22_SOURCE_HOOKS_H
#include "acvr.h"
#ifdef __cplusplus
extern "C" {
#endif
typedef struct acvr_ss22_hooks {
    void *user;
    int (*begin_tick)(void *);       /* 1 permit, 0 stop, -1 failure */
    int (*publish_tick)(void *);     /* 1 snapshot published, -1 failure */
    int (*stop_requested)(void *);
    int (*video_snapshot)(void *,const void *regs);
    void (*output_bits)(void *,uint16_t);
    void (*capture_quad)(void *,const void *quad,const void *view);
} acvr_ss22_hooks;
acvr_result acvr_ss22_bind_hooks(const acvr_ss22_hooks *);
acvr_result acvr_ss22_run_entry(void (*entry)(void));
void acvr_ss22_frame_boundary(void);
void acvr_ss22_check_stop(void);
void acvr_ss22_video_snapshot(const void *regs);
void acvr_ss22_output_bits(uint16_t bits);
void acvr_ss22_capture_quad(const void *quad,const void *view);
#ifdef __cplusplus
}
#endif
#endif
