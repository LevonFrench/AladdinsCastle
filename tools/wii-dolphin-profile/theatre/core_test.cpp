// SPDX-License-Identifier: GPL-3.0-only
#include "core.h"
#include <iostream>
#include <vector>

static void require(bool value) { if (!value) throw std::runtime_error("Regression failed"); }
template<class F> static void rejects(F operation) {
    bool failed=false; try { operation(); } catch(const std::exception&) { failed=true; }
    require(failed);
}
int main() {
    using namespace theatre;
    try {
        require(number(L"123",1000)==123);
        require(eligible(12,12,true,false));
        require(!eligible(13,12,true,false));
        require(!eligible(12,12,false,false));
        require(!eligible(12,12,true,true));
        require(!eligible(0,0,true,false));
        for (const auto* text : {L"0",L"-1",L"1.2",L"12x",L"1001",L"99999999999999999999999"})
            rejects([&]{ number(text,1000); });
        require(imageBytes(1280,720)==1280u*720u*4u);
        for (auto dimensions : {std::array<unsigned,2>{0,720},{4098,720},{1281,720},{1280,2161}})
            rejects([&]{ imageBytes(dimensions[0],dimensions[1]); });
        Matrix head={1,0,0,5, 0,1,0,1.7f, 0,0,1,4};
        auto fixed=plane(head);
        require(fixed[3]==5 && std::abs(fixed[7]-1.7f)<.0001 && fixed[11]==2);
        head[3]=40; require(fixed[3]==5); // Plane never follows later head movement.
        Matrix yaw={0,0,1,0, 0,1,0,2, -1,0,0,0};
        auto rotated=plane(yaw);
        require(rotated[3]==-2 && rotated[11]==0 && rotated[8]==-1);
        Matrix vertical={1,0,0,0, 0,0,1,0, 0,-1,0,0};
        rejects([&]{ plane(vertical); });
        head[0]=std::numeric_limits<float>::quiet_NaN(); rejects([&]{ plane(head); });
        require(std::abs(texelAspect(1280,720)-2)<.0001);
        require(std::abs(texelAspect(2560,720)-1)<.0001);
        std::vector<std::uint8_t> black(128,0); require(!containsColour(black.data(),black.size()));
        black[3]=255; require(!containsColour(black.data(),black.size()));
        black[0]=20; require(containsColour(black.data(),black.size()));
        std::cout << "theatre CPU safety/plane/aspect/black-capture checks passed\n";
        return 0;
    } catch(const std::exception& e) { std::cerr << e.what() << '\n'; return 1; }
}
