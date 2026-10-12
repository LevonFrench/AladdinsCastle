// SPDX-License-Identifier: MIT
#pragma once
#include <cstddef>
#include <cstdint>
#include <initializer_list>

namespace acvr {
// Only formats supported by GunGlRenderer. Preserve linear RGBA8 where offered;
// SteamVR may instead advertise sRGB8-alpha8, which requires sRGB rendering.
inline int64_t select_gl_swapchain_format(const int64_t *formats, std::size_t count) {
    for (const int64_t supported : {int64_t{0x8058}, int64_t{0x8c43}})
        for (std::size_t i = 0; i < count; ++i)
            if (formats[i] == supported) return supported;
    return 0;
}
}
