// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include <array>
#include <cmath>
#include <cstdint>
#include <limits>
#include <stdexcept>
#include <string>

namespace theatre {
constexpr double width = 3.2, height = 1.8, distance = 2.0;
using Matrix = std::array<float, 12>;
inline std::string overlayFailure(const char* operation,int code,const char* name) {
    return std::string(operation)+": "+(name?name:"UnknownOverlayError")+" ("+std::to_string(code)+")";
}
struct Rect { long left,top,right,bottom; };
inline bool eligible(std::uint32_t pid,std::uint32_t selected,bool visible,bool owned) {
    return selected != 0 && pid == selected && visible && !owned;
}
inline bool validGameId(const std::wstring& id) {
    return id.size()==6 && id.find_first_not_of(L"ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789")==std::wstring::npos;
}
inline bool gameWindowTitle(const std::wstring& title,const std::wstring& id) {
    if(!validGameId(id)) throw std::runtime_error("Expected six-character game header ID");
    return title.find(L"("+id+L")")!=std::wstring::npos;
}
inline void selectedWindow(bool processLive,bool exists,std::uint32_t actual,std::uint32_t selected,bool minimized) {
    if(!processLive || !exists || !selected || actual!=selected)
        throw std::runtime_error("Selected Dolphin process/window closed or became stale");
    if(minimized) throw std::runtime_error("Selected Dolphin window is minimized");
}
inline unsigned number(const std::wstring& text, unsigned maximum) {
    if (text.empty() || text.find_first_not_of(L"0123456789") != std::wstring::npos)
        throw std::runtime_error("Expected positive integer");
    auto value = std::stoull(text);
    if (!value || value > maximum) throw std::runtime_error("Integer outside permitted range");
    return static_cast<unsigned>(value);
}
inline std::size_t imageBytes(unsigned w, unsigned h) {
    if (!w || !h || w > 4096 || h > 2160 || w % 2)
        throw std::runtime_error("Client image must have even width, at most4096x2160");
    return static_cast<std::size_t>(w) * h * 4;
}
inline void captureSize(long w,long h) {
    if(w<=0 || h<=0 || w>4352 || h>2304) throw std::runtime_error("WGC window frame exceeds bounded size");
}
inline void validateCrop(const Rect& crop,unsigned w,unsigned h) {
    captureSize(w,h);
    if(crop.left<0 || crop.top<0 || crop.right<=crop.left || crop.bottom<=crop.top ||
       crop.right>static_cast<long>(w) || crop.bottom>static_cast<long>(h))
        throw std::runtime_error("Client crop escapes selected WGC window frame");
    imageBytes(static_cast<unsigned>(crop.right-crop.left),static_cast<unsigned>(crop.bottom-crop.top));
}
struct FrameView { unsigned width,height; const std::uint8_t* pixels; std::size_t bytes; std::int64_t timeMs; };
struct SpatialStatistics { unsigned majorityVoteRgb=0; std::size_t interiorPixels=0,otherRgbPixels=0; };
inline bool spatialCandidate(const FrameView& frame,SpatialStatistics* statistics=nullptr) {
    if(!frame.pixels || frame.bytes!=imageBytes(frame.width,frame.height))
        throw std::runtime_error("Content gate requires exact client pixels");
    // Ignore a five-percent perimeter, including rounded startup-window corners.
    // Reject >99% one RGB colour in the interior; alpha noise cannot qualify.
    // Remaining variation is candidate evidence only, never game/SBS acceptance.
    const auto mx=frame.width/20,my=frame.height/20;
    auto rgb=[&](unsigned x,unsigned y) {
        const auto i=(static_cast<std::size_t>(y)*frame.width+x)*4;
        return static_cast<unsigned>(frame.pixels[i]) |
            (static_cast<unsigned>(frame.pixels[i+1])<<8) |
            (static_cast<unsigned>(frame.pixels[i+2])<<16);
    };
    unsigned candidate=0; std::size_t votes=0,total=0,different=0;
    for(unsigned y=my;y<frame.height-my;++y) for(unsigned x=mx;x<frame.width-mx;++x) {
        const auto value=rgb(x,y); ++total;
        if(!votes) { candidate=value; votes=1; }
        else if(value==candidate) ++votes;
        else --votes;
    }
    for(unsigned y=my;y<frame.height-my;++y) for(unsigned x=mx;x<frame.width-mx;++x)
        if(rgb(x,y)!=candidate) ++different;
    if(statistics) *statistics={candidate,total,different};
    return different>total/100;
}
inline bool framesDiffer(const FrameView& a,const FrameView& b) {
    if(!a.pixels || !b.pixels || a.bytes!=imageBytes(a.width,a.height) || b.bytes!=imageBytes(b.width,b.height))
        throw std::runtime_error("Frame gate requires exact client pixels");
    if(a.width!=b.width || a.height!=b.height) throw std::runtime_error("Client resized during two-frame gate");
    if(a.timeMs<0 || b.timeMs<a.timeMs || b.timeMs-a.timeMs<1000)
        throw std::runtime_error("Frame gate needs compositor frames at least one second apart");
    for(std::size_t i=0;i<a.bytes;i+=4)
        if(a.pixels[i]!=b.pixels[i] || a.pixels[i+1]!=b.pixels[i+1] || a.pixels[i+2]!=b.pixels[i+2]) return true;
    return false;
}
enum class LiveFrameStep { Stop, Wait, Submit };
inline LiveFrameStep liveFrameStep(bool running,bool beforeDeadline,bool freshFrame) {
    if(!running || !beforeDeadline) return LiveFrameStep::Stop;
    return freshFrame ? LiveFrameStep::Submit : LiveFrameStep::Wait;
}
inline Matrix plane(const Matrix& head) {
    for (auto v : head) if (!std::isfinite(v)) throw std::runtime_error("Invalid head pose");
    const double fx = -head[2], fz = -head[10];
    const double length = std::hypot(fx, fz);
    if (length < .01) throw std::runtime_error("Cannot centre while looking vertically");
    const double x = fx / length, z = fz / length;
    // Columns: right, up, normal (back), translation. Same basis as bridge Mapper.
    return {static_cast<float>(-z), 0, static_cast<float>(-x), static_cast<float>(head[3]+distance*x),
            0, 1, 0, head[7],
            static_cast<float>(x), 0, static_cast<float>(-z), static_cast<float>(head[11]+distance*z)};
}
inline float texelAspect(unsigned w, unsigned h) {
    imageBytes(w,h);
    return static_cast<float>((width/height) * 2.0 * h / w);
}
}
