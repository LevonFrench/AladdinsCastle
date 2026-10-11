// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include <array>
#include <cmath>
#include <cstdint>
#include <limits>
#include <sstream>
#include <iomanip>
#include <stdexcept>
#include <string>

namespace theatre {
constexpr double width = 3.2, height = 1.8, distance = 2.0;
using Matrix = std::array<float, 12>;
struct Rect { long left,top,right,bottom; };
inline bool occludes(bool visible,bool minimized,bool cloakKnown,bool cloaked,
                     const Rect& candidate,const Rect& client) {
    if(!visible || minimized || (cloakKnown && cloaked)) return false;
    return candidate.left<candidate.right && candidate.top<candidate.bottom &&
           client.left<client.right && client.top<client.bottom &&
           candidate.left<client.right && candidate.right>client.left &&
           candidate.top<client.bottom && candidate.bottom>client.top;
}
inline std::string occlusionDiagnostic(std::uint32_t pid,const std::wstring& className,
        const Rect& window,const Rect& client,bool cloakKnown,unsigned cloak,bool frameKnown) {
    std::ostringstream output;
    output << "Selected Dolphin client is occluded: PID=" << pid << " class=\"";
    for(wchar_t character:className) {
        if(character>=32 && character<=126 && character!=L'"' && character!=L'\\')
            output << static_cast<char>(character);
        else output << "\\u" << std::hex << std::setw(4) << std::setfill('0')
                    << static_cast<std::uint32_t>(character) << std::dec;
    }
    const auto rectangle=[&](const Rect& value) {
        output << '[' << value.left << ',' << value.top << ',' << value.right << ',' << value.bottom << ']';
    };
    output << "\" rect="; rectangle(window);
    output << " client="; rectangle(client);
    output << " DWM-cloaked=";
    if(cloakKnown) output << cloak; else output << "unknown";
    output << " bounds=" << (frameKnown?"DWM-frame":"GetWindowRect")
           << " pixels=physical; uncover it before capture (no title/content queried)";
    return output.str();
}
inline bool eligible(std::uint32_t pid,std::uint32_t selected,bool visible,bool owned) {
    return selected != 0 && pid == selected && visible && !owned;
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
inline bool containsColour(const std::uint8_t* pixels, std::size_t bytes) {
    if (!pixels || bytes < 4 || bytes % 4) return false;
    for (std::size_t i=0;i<bytes;i+=4)
        if (pixels[i] > 4 || pixels[i+1] > 4 || pixels[i+2] > 4) return true;
    return false;
}
}
