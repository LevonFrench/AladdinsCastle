// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include <windows.h>
#include <stdexcept>
// Only this thread changes awareness; no display/system setting is modified.
struct PhysicalPixels {
    DPI_AWARENESS_CONTEXT previous{};
    PhysicalPixels() {
        previous=SetThreadDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
        if(!previous) throw std::runtime_error("Cannot establish physical-pixel capture coordinates");
        if(!AreDpiAwarenessContextsEqual(GetThreadDpiAwarenessContext(),DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2)) {
            SetThreadDpiAwarenessContext(previous);
            throw std::runtime_error("Capture thread DPI context is inconsistent");
        }
    }
    PhysicalPixels(const PhysicalPixels&)=delete;
    PhysicalPixels& operator=(const PhysicalPixels&)=delete;
    ~PhysicalPixels() { if(previous) SetThreadDpiAwarenessContext(previous); }
};
