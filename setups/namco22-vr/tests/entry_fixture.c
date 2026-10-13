/* SPDX-License-Identifier: GPL-3.0-only */
#include "n22_source_hooks.h"
void n22_fixture_entry(void) {
    uint32_t seed=0;
    for(;;) {
        acvr_ss22_check_stop();
        ++seed;
        acvr_ss22_output_bits((uint16_t)seed);
        acvr_ss22_video_snapshot(&seed);
        acvr_ss22_frame_boundary();
    }
}
void n22_fixture_poll_forever(void) {for(;;) acvr_ss22_check_stop();}
void n22_fixture_bad_video(void) {acvr_ss22_video_snapshot(0);}
void n22_fixture_return(void) {}
