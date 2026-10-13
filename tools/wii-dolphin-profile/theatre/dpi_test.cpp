// SPDX-License-Identifier: GPL-3.0-only
#include "native_dpi.h"
#include <iostream>
int main() {
    const auto original=GetThreadDpiAwarenessContext();
    try {
        { PhysicalPixels pixels;
          if(!AreDpiAwarenessContextsEqual(GetThreadDpiAwarenessContext(),DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2)) return 1; }
        if(!AreDpiAwarenessContextsEqual(GetThreadDpiAwarenessContext(),original)) return 1;
        std::cout << "Physical-pixel thread context set/restored; no windows or VR initialized\n";
        return 0;
    } catch(const std::exception& e) { std::cerr << e.what() << '\n'; return 1; }
}
