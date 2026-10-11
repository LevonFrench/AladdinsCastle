// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include "core.h"
#include <algorithm>
#include <optional>
#include <locale>
#include <sstream>
#include <vector>

namespace theatre {
enum class FailureStage { SelectWindow, CaptureStart, FirstCandidate, CompareChange };
enum class CapturePoll { NotPolled, NoFrame, PoolRecreated, Frame, AcquisitionError, ReadbackError };
struct CaptureObservation {
    CapturePoll kind=CapturePoll::NotPolled;
    unsigned sourceWidth=0,sourceHeight=0;
    Rect crop{};
    bool cropValid=false;
};
template<class Acquire> auto acquireCaptureFrame(CaptureObservation& observation,Acquire acquire) {
    // A throwing acquisition is not an empty poll. Preserve this state until
    // the API actually returns a frame or an explicit null frame.
    observation.kind=CapturePoll::AcquisitionError;
    auto frame=acquire();
    observation.kind=frame?CapturePoll::ReadbackError:CapturePoll::NoFrame;
    return frame;
}
struct DiagnosticSample {
    unsigned width=0,height=0;
    std::int64_t timeMs=0;
    std::uint64_t observation=0;
    std::vector<std::uint8_t> pixels;
    explicit DiagnosticSample(const FrameView& f,std::uint64_t sequence)
        : width(f.width),height(f.height),timeMs(f.timeMs),observation(sequence),
          pixels(f.pixels,f.pixels+f.bytes) {}
};
inline std::string jsonText(const std::string& text) {
    static constexpr char hex[]="0123456789abcdef";
    std::string result="\"";
    for(unsigned char c:text) {
        if(c=='"' || c=='\\') { result+='\\'; result+=static_cast<char>(c); }
        else if(c<32) { result+="\\u00"; result+=hex[c>>4]; result+=hex[c&15]; }
        else result+=static_cast<char>(c);
    }
    return result+'"';
}

// Opt-in only; no graphics, window discovery, clock polling or file I/O.
// Exactly two owned pixel buffers: first raw (replaced by first candidate), latest.
class FailureDiagnostic {
public:
    unsigned startupSeconds,comparisonSeconds;
    FailureStage stage=FailureStage::SelectWindow;
    std::uint64_t pid=0,hwnd=0,polls=0,noFrames=0,recreates=0,acquisitionErrors=0,readbackErrors=0,rawFrames=0;
    std::uint64_t candidates=0,rejections=0,timestampAdvances=0,timestampRegressions=0;
    std::uint64_t tooSoon=0,identicalRgb=0,differentRgb=0;
    unsigned clientWidth=0,clientHeight=0;
    bool selected=false,titleMatch=false,hasCandidate=false;
    CaptureObservation capture;
    SpatialStatistics spatial;
    std::optional<std::int64_t> firstTime,lastTime,minTime,maxTime;
    std::optional<DiagnosticSample> anchor,latest;

    FailureDiagnostic(unsigned startup,unsigned comparison)
        : startupSeconds(startup),comparisonSeconds(comparison) {}
    void select(std::uint64_t process,std::uint64_t window,unsigned w,unsigned h,bool matched) {
        pid=process; hwnd=window; clientWidth=w; clientHeight=h;
        selected=true; titleMatch=matched;
    }
    void observePoll(const CaptureObservation& observation) {
        ++polls; capture=observation;
        if(observation.kind==CapturePoll::NoFrame) ++noFrames;
        if(observation.kind==CapturePoll::PoolRecreated) ++recreates;
        if(observation.kind==CapturePoll::AcquisitionError) ++acquisitionErrors;
        if(observation.kind==CapturePoll::ReadbackError) ++readbackErrors;
    }
    void observeFrame(const FrameView& f,const SpatialStatistics& statistics,bool candidate) {
        if(!f.pixels || f.bytes!=imageBytes(f.width,f.height))
            throw std::runtime_error("Diagnostic requires exact selected-client pixels");
        ++rawFrames; spatial=statistics;
        if(lastTime) {
            if(f.timeMs>*lastTime) ++timestampAdvances;
            if(f.timeMs<*lastTime) ++timestampRegressions;
        } else firstTime=minTime=maxTime=f.timeMs;
        minTime=std::min(*minTime,f.timeMs); maxTime=std::max(*maxTime,f.timeMs);
        lastTime=f.timeMs;
        if(!anchor) anchor.emplace(f,rawFrames);
        if(candidate) {
            ++candidates;
            if(!hasCandidate) { anchor.emplace(f,rawFrames); hasCandidate=true; }
        } else ++rejections;
        latest.emplace(f,rawFrames);
    }
    void comparison(bool enoughTime,bool changed=false) {
        if(!enoughTime) ++tooSoon;
        else if(changed) ++differentRgb;
        else ++identicalRgb;
    }
    bool separateLatest() const {
        return anchor && latest && anchor->observation!=latest->observation;
    }
    const char* anchorFile() const { return hasCandidate?"first-candidate.png":"first-raw.png"; }
    std::string json(const std::string& error) const {
        std::ostringstream out;
        out.imbue(std::locale::classic());
        const char* name=stage==FailureStage::SelectWindow?"select-window":
            stage==FailureStage::CaptureStart?"capture-start":
            stage==FailureStage::FirstCandidate?"first-candidate":"compare-change";
        out << "{\"schema\":\"ac-wgc-failure-diagnostic/1\",\"accepted\":false,\"stage\":" << jsonText(name)
            << ",\"deadline_kind\":" << jsonText(stage==FailureStage::CompareChange?"comparison":"startup")
            << ",\"error\":" << jsonText(error)
            << ",\"bounds\":{\"startup_seconds\":" << startupSeconds << ",\"comparison_seconds\":" << comparisonSeconds << "}"
            << ",\"selection\":{\"selected\":" << (selected?"true":"false")
            << ",\"pid\":\"" << pid << "\",\"hwnd\":\"" << hwnd << "\",\"title_match\":" << (titleMatch?"true":"false")
            << ",\"client_width\":" << clientWidth << ",\"client_height\":" << clientHeight << "}"
            << ",\"capture\":{\"source_width\":" << capture.sourceWidth << ",\"source_height\":" << capture.sourceHeight
            << ",\"crop_valid\":" << (capture.cropValid?"true":"false")
            << ",\"crop\":[" << capture.crop.left << ',' << capture.crop.top << ',' << capture.crop.right << ',' << capture.crop.bottom << "]}"
            << ",\"counts\":{\"polls\":" << polls << ",\"no_frames\":" << noFrames << ",\"pool_recreations\":" << recreates
            << ",\"acquisition_errors\":" << acquisitionErrors << ",\"readback_errors\":" << readbackErrors
            << ",\"raw_frames\":" << rawFrames << ",\"spatial_candidates\":" << candidates << ",\"spatial_rejections\":" << rejections
            << ",\"timestamp_advances\":" << timestampAdvances << ",\"timestamp_regressions\":" << timestampRegressions
            << ",\"too_soon\":" << tooSoon << ",\"identical_rgb\":" << identicalRgb << ",\"different_rgb\":" << differentRgb << "}"
            << ",\"last_spatial\":{\"majority_vote_rgb\":" << spatial.majorityVoteRgb << ",\"interior_pixels\":" << spatial.interiorPixels
            << ",\"other_rgb_pixels\":" << spatial.otherRgbPixels << "},\"compositor_time_ms\":{\"first\":";
        auto time=[&](const std::optional<std::int64_t>& value) { if(value) out << *value; else out << "null"; };
        time(firstTime); out << ",\"last\":"; time(lastTime); out << ",\"min\":"; time(minTime); out << ",\"max\":"; time(maxTime);
        out << "},\"sample_export\":\"requested-on-failure-only\",\"samples\":[";
        auto sample=[&](const char* file,const DiagnosticSample& image) {
            out << "{\"file\":" << jsonText(file) << ",\"accepted\":false,\"observation\":" << image.observation
                << ",\"width\":" << image.width << ",\"height\":" << image.height << ",\"compositor_time_ms\":" << image.timeMs << '}';
        };
        if(anchor) sample(anchorFile(),*anchor);
        if(separateLatest()) { out << ','; sample("latest-raw.png",*latest); }
        return out.str()+"]}\n";
    }
};
}
