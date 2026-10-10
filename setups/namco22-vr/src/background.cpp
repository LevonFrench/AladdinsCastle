// SPDX-License-Identifier: GPL-3.0-only
#include "n22_background.hpp"
#include "n22_worker.hpp"
namespace n22 {
acvr_result copy_super22_background(const VideoSnapshot &source,BackgroundState &out) {
    if(!source.tick || source.banks[Mixer].size()!=bank_sizes[Mixer]) return ACVR_BAD_ARGUMENT;
    BackgroundState state;state.policy=BackgroundPolicy::Super22Mixer;state.tick=source.tick;
    for(size_t c=0;c<3;++c) state.rgb[c]=source.banks[Mixer][0x08+c];
    out=state;return ACVR_OK;
}
acvr_result validate_background(const BackgroundState &state,uint64_t tick) {
    if(state.policy==BackgroundPolicy::Absent) return ACVR_OK;
    if(state.policy!=BackgroundPolicy::Super22Mixer) return ACVR_UNSUPPORTED;
    return tick && state.tick==tick?ACVR_OK:ACVR_BAD_ARGUMENT;
}
uint32_t background_rgb(const BackgroundState &state) {
    if(state.policy==BackgroundPolicy::Absent) return 0;
    return (uint32_t(state.rgb[0])<<16)|(uint32_t(state.rgb[1])<<8)|state.rgb[2];
}
}
