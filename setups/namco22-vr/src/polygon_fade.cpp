// SPDX-License-Identifier: GPL-3.0-only
#include "n22_polygon_fade.hpp"
#include "n22_worker.hpp"
namespace n22 {
acvr_result copy_super22_polygon_fade(const VideoSnapshot &source,PolygonFadeState &out) {
    if(!source.tick || source.banks[Mixer].size()!=bank_sizes[Mixer]) return ACVR_BAD_ARGUMENT;
    PolygonFadeState state;state.policy=PolygonFadePolicy::Super22InputFold;state.tick=source.tick;
    for(size_t c=0;c<3;++c) state.rgb[c]=source.banks[Mixer][c];
    out=state;return ACVR_OK;
}
acvr_result validate_polygon_fade(const PolygonFadeState &state,uint64_t tick) {
    if(state.policy==PolygonFadePolicy::Absent) return ACVR_OK;
    if(state.policy!=PolygonFadePolicy::Super22InputFold) return ACVR_UNSUPPORTED;
    return tick && state.tick==tick?ACVR_OK:ACVR_BAD_ARGUMENT;
}
bool polygon_fade_active(const PolygonFadeState &state) {
    return state.policy==PolygonFadePolicy::Super22InputFold && state.rgb!=std::array<uint8_t,3>{255,255,255};
}
std::array<double,3> polygon_fade_factors(const PolygonFadeState &state) {
    if(!polygon_fade_active(state)) return {1,1,1};
    return {state.rgb[0]/256.,state.rgb[1]/256.,state.rgb[2]/256.};
}
}
