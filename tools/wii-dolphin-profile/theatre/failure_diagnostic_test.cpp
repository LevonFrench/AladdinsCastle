// SPDX-License-Identifier: GPL-3.0-only
#include "failure_diagnostic.h"
#include <iostream>

using namespace theatre;
static void require(bool value) { if(!value) throw std::runtime_error("Failure diagnostic CPU assertion"); }
static FrameView view(const std::vector<std::uint8_t>& pixels,std::int64_t time) {
    return {100,100,pixels.data(),pixels.size(),time};
}
static void interior(std::vector<std::uint8_t>& pixels,unsigned count) {
    for(unsigned i=0;i<count;++i) pixels[(static_cast<std::size_t>(5+i/90)*100+5+i%90)*4]=1;
}
static void observe(FailureDiagnostic& diagnostic,const std::vector<std::uint8_t>& pixels,std::int64_t time) {
    SpatialStatistics stats;
    const auto f=view(pixels,time);
    const bool candidate=spatialCandidate(f,&stats);
    diagnostic.observeFrame(f,stats,candidate);
}
int main() {
    try {
        std::vector<std::uint8_t> uniform(imageBytes(100,100),0);
        SpatialStatistics statistics;
        // Independent boundary oracle: 90*90 interior, 81 changed is exactly 1%.
        require(!spatialCandidate(view(uniform,0),&statistics));
        require(statistics.interiorPixels==8100 && statistics.otherRgbPixels==0);
        auto boundary=uniform; interior(boundary,81);
        require(!spatialCandidate(view(boundary,0),&statistics));
        require(statistics.otherRgbPixels==81);
        interior(boundary,82);
        require(spatialCandidate(view(boundary,0),&statistics));
        require(statistics.otherRgbPixels==82);
        require(spatialCandidate(view(boundary,0))==spatialCandidate(view(boundary,0),&statistics));
        auto alpha=uniform;
        for(std::size_t i=3;i<alpha.size();i+=4) alpha[i]=static_cast<std::uint8_t>(i%256);
        require(!spatialCandidate(view(alpha,0),&statistics));
        auto border=uniform;
        for(unsigned y=0;y<100;++y) for(unsigned x=0;x<100;++x)
            if(x<5 || x>=95 || y<5 || y>=95) border[(static_cast<std::size_t>(y)*100+x)*4]=255;
        require(!spatialCandidate(view(border,0),&statistics));

        FailureDiagnostic empty(40,15);
        empty.observePoll({CapturePoll::NoFrame});
        empty.observePoll({CapturePoll::PoolRecreated});
        empty.observePoll({CapturePoll::ReadbackError});
        require(empty.polls==3 && empty.noFrames==1 && empty.recreates==1 && empty.pollErrors==1);
        require(empty.rawFrames==0 && !empty.anchor && !empty.latest);
        require(empty.json("no frames").find("\"samples\":[]")!=std::string::npos);
        require(empty.json("no frames").find("\"first\":null")!=std::string::npos);

        FailureDiagnostic diagnostic(40,15);
        diagnostic.select(12,99,100,100,true);
        diagnostic.stage=FailureStage::CaptureStart;
        require(diagnostic.json("start").find("\"stage\":\"capture-start\"")!=std::string::npos);
        diagnostic.stage=FailureStage::FirstCandidate;
        observe(diagnostic,uniform,0);
        require(!diagnostic.hasCandidate && diagnostic.rejections==1);
        require(std::string(diagnostic.anchorFile())=="first-raw.png" && !diagnostic.separateLatest());
        observe(diagnostic,boundary,100);
        require(diagnostic.hasCandidate && diagnostic.candidates==1 && diagnostic.anchor->observation==2);
        require(std::string(diagnostic.anchorFile())=="first-candidate.png");
        diagnostic.stage=FailureStage::CompareChange;
        diagnostic.comparison(false);
        diagnostic.comparison(true,false);
        diagnostic.comparison(true,true);
        require(diagnostic.tooSoon==1 && diagnostic.identicalRgb==1 && diagnostic.differentRgb==1);
        require(!framesDiffer(view(boundary,100),view(boundary,1100)));
        observe(diagnostic,boundary,1100);
        observe(diagnostic,uniform,1100); // Raw frames arrive, spatial gate rejects.
        observe(diagnostic,uniform,1000); // Stalled and regressing time are distinguishable.
        require(diagnostic.timestampAdvances==2 && diagnostic.timestampRegressions==1);
        require(diagnostic.rawFrames==5 && diagnostic.candidates==2 && diagnostic.rejections==3);
        require(diagnostic.firstTime==0 && diagnostic.lastTime==1000 && diagnostic.minTime==0 && diagnostic.maxTime==1100);
        const auto first=diagnostic.anchor->pixels;
        // Many observations retain only the immutable first candidate and latest.
        for(unsigned i=0;i<200;++i) observe(diagnostic,uniform,1200+i);
        require(diagnostic.anchor->pixels==first && diagnostic.anchor->timeMs==100);
        require(diagnostic.anchor->pixels.size()+diagnostic.latest->pixels.size()==2*imageBytes(100,100));
        require(diagnostic.latest->pixels==uniform && diagnostic.separateLatest());
        const auto report=diagnostic.json("failure \"quoted\"\n\\end");
        require(report.find("\"accepted\":false")!=std::string::npos);
        require(report.find("\"deadline_kind\":\"comparison\"")!=std::string::npos);
        require(report.find("\\\"quoted\\\"\\u000a\\\\end")!=std::string::npos);
        require(report.find("latest-raw.png")!=std::string::npos);
        require(report.find("capture.json")==std::string::npos && report.find("frame-1.png")==std::string::npos);
        require(report.find("title\"")==std::string::npos); // Boolean match only, no window title.
        bool rejected=false;
        try { diagnostic.observeFrame({100,100,nullptr,0,0},{},false); }
        catch(const std::runtime_error&) { rejected=true; }
        require(rejected);
        // Emitted JSON is parsed independently by the Python test wrapper.
        std::cout << report;
        return 0;
    } catch(const std::exception& error) { std::cerr << error.what() << '\n'; return 1; }
}
