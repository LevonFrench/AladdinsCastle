// SPDX-License-Identifier: MIT
#pragma once
#include <array>
#include <cstddef>

namespace acvr {

struct StereoCaptureView {
    const char *name;
    const char *filename;
    std::size_t view_index;
};

// XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO returns left view first, right view second.
// This indexes xrLocateViews output, not the independently ordered hand array.
inline constexpr std::array<StereoCaptureView,2> kStereoCaptureViews{{
    {"left", "left.ppm", 0},
    {"right", "right.ppm", 1},
}};

} // namespace acvr
