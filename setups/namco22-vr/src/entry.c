/* SPDX-License-Identifier: GPL-3.0-only */
#include "n22_source_hooks.h"
#include <setjmp.h>
#if defined(_MSC_VER)
#define N22_TLS __declspec(thread)
#else
#define N22_TLS _Thread_local
#endif
static N22_TLS const acvr_ss22_hooks *bound;
static N22_TLS jmp_buf entry_stop;
static N22_TLS int active;

/* The jump target and lifted call stack are C only. Every C++ hook returns
 * before we jump; never jump across C++ destructors, locks or stack objects.
 * No stop utility, engine authentication, process exit or thread cancellation.
 */
acvr_result acvr_ss22_bind_hooks(const acvr_ss22_hooks *hooks) {
    if(active || (bound && hooks)) return ACVR_BAD_STATE;
    bound=hooks;return ACVR_OK;
}
static void next_permit(void) {
    int result=bound->begin_tick(bound->user);
    if(result<=0) longjmp(entry_stop,result==0?1:2);
}
acvr_result acvr_ss22_run_entry(void (*entry)(void)) {
    int result;
    if(active || !entry || !bound || !bound->begin_tick || !bound->publish_tick || !bound->stop_requested)
        return ACVR_BAD_STATE;
    active=1;
    result=setjmp(entry_stop);
    if(result==0) {
        next_permit();
        entry();
        active=0;return ACVR_BAD_STATE; /* a lifted entry must not fall through */
    }
    active=0;
    return result==1?ACVR_STOPPED:ACVR_ERROR;
}
void acvr_ss22_check_stop(void) {
    if(active && bound->stop_requested(bound->user)) longjmp(entry_stop,1);
}
void acvr_ss22_frame_boundary(void) {
    if(!active) return;
    acvr_ss22_check_stop();
    if(bound->publish_tick(bound->user)<=0) longjmp(entry_stop,2);
    next_permit();
}
void acvr_ss22_video_snapshot(const void *regs) {
    if(!active) return;
    acvr_ss22_check_stop();
    if(!bound->video_snapshot || bound->video_snapshot(bound->user,regs)<=0) longjmp(entry_stop,2);
}
void acvr_ss22_output_bits(uint16_t bits) {
    if(bound && bound->output_bits) bound->output_bits(bound->user,bits);
}
void acvr_ss22_capture_quad(const void *quad,const void *view) {
    if(bound && bound->capture_quad) bound->capture_quad(bound->user,quad,view);
}
