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
        require(overlayFailure("submit",23,"VROverlayError_RequestFailed")=="submit: VROverlayError_RequestFailed (23)");
        require(overlayFailure("submit",99,nullptr)=="submit: UnknownOverlayError (99)");
        require(eligible(12,12,true,false));
        require(!eligible(13,12,true,false));
        require(!eligible(12,12,false,false));
        require(!eligible(12,12,true,true));
        require(!eligible(0,0,true,false));
        require(gameWindowTitle(L"Dolphin | Synthetic (DEMO01)",L"DEMO01"));
        require(!gameWindowTitle(L"Dolphin main window",L"DEMO01"));
        require(!gameWindowTitle(L"Dolphin | Synthetic (DEMO02)",L"DEMO01"));
        rejects([]{ gameWindowTitle(L"Dolphin",L"demo01"); });
        selectedWindow(true,true,12,12,false);
        rejects([]{ selectedWindow(false,true,12,12,false); });
        rejects([]{ selectedWindow(true,false,12,12,false); });
        rejects([]{ selectedWindow(true,true,13,12,false); });
        rejects([]{ selectedWindow(true,true,12,12,true); });
        validateCrop(Rect{10,20,1290,740},1300,760);
        rejects([]{ validateCrop(Rect{-1,0,1280,720},1300,760); });
        rejects([]{ validateCrop(Rect{10,20,1400,740},1300,760); });
        rejects([]{ validateCrop(Rect{10,20,1291,740},1300,760); });
        rejects([]{ captureSize(4353,2304); });
        rejects([]{ captureSize(0,720); });
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
        std::vector<std::uint8_t> first(32,100),second=first;
        FrameView fa{4,2,first.data(),first.size(),1000},fb{4,2,second.data(),second.size(),2000};
        require(!framesDiffer(fa,fb)); // A nonblack grey panel is insufficient.
        require(!spatialCandidate(fa));
        second[3]=0; require(!framesDiffer(fa,fb)); // Ignore alpha-only noise.
        second[0]=50; require(framesDiffer(fa,fb));
        require(spatialCandidate(fb));
        std::vector<std::uint8_t> blank(100*100*4,255);
        FrameView blankView{100,100,blank.data(),blank.size(),0};
        require(!spatialCandidate(blankView)); // White then black is not game evidence.
        for(auto& channel:blank) channel=0;
        require(!spatialCandidate(blankView));
        for(auto& channel:blank) channel=127;
        require(!spatialCandidate(blankView));
        for(std::size_t i=3;i<blank.size();i+=4) blank[i]=static_cast<std::uint8_t>(i%255);
        require(!spatialCandidate(blankView)); // Alpha-only variation.
        for(unsigned y=0;y<4;++y) for(unsigned x=0;x<100;++x) blank[(y*100+x)*4]=0;
        require(!spatialCandidate(blankView)); // Border decoration cannot qualify.
        for(unsigned y=40;y<50;++y) for(unsigned x=40;x<50;++x) blank[(y*100+x)*4]=0;
        require(spatialCandidate(blankView)); // Varied candidate, not acceptance.
        fb.timeMs=1999; rejects([&]{ framesDiffer(fa,fb); });
        fb.timeMs=500; rejects([&]{ framesDiffer(fa,fb); });
        fb.timeMs=2000; fb.bytes=31; rejects([&]{ framesDiffer(fa,fb); });
        fb.bytes=32; fb.pixels=nullptr; rejects([&]{ framesDiffer(fa,fb); });
        fb.pixels=second.data(); fb.width=2; fb.height=4; rejects([&]{ framesDiffer(fa,fb); });
        require(liveFrameStep(true,true,true)==LiveFrameStep::Submit);
        // After startup proof, an empty frame pool stays in the event loop.
        // The next pass can handle recenter or quit without a new image.
        int serviced=0;
        bool active=true;
        for(int i=0;i<3;++i) {
            ++serviced;
            if(i==2) active=false; // Synthetic quit event after two empty polls.
            const auto step=liveFrameStep(active,true,false);
            require(step==(active?LiveFrameStep::Wait:LiveFrameStep::Stop));
        }
        require(serviced==3);
        require(liveFrameStep(true,false,false)==LiveFrameStep::Stop); // Bounded deadline.
        std::cout << "theatre CPU selection/crop/two-frame/plane checks passed\n";
        return 0;
    } catch(const std::exception& e) { std::cerr << e.what() << '\n'; return 1; }
}
