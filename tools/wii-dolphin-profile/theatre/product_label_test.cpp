// SPDX-License-Identifier: GPL-3.0-only
#include "product_fixture.h"
#include <cstring>
#include <iostream>
int main() {
    // UTF-8 bytes survive Python JSON -> generated C++ -> compiled viewer label,
    // regardless of the compiler's source codepage. No runtime/window created.
    const char* expected="Synthetic \"Gallery\" \xf0\x9f\x8e\xae 3D theatre";
    if(std::strcmp(theatre::productTitle,expected)!=0) return 1;
    if(std::strcmp(theatre::productOverlayKeyPrefix,"synthetic-gallery.dolphin.theatre.")!=0) return 2;
    std::cout << "compiled product title and overlay key bytes passed\n";
    return 0;
}
