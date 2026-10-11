// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include <windows.h>
#include <d3d11.h>
#include "core.h"
#include <memory>
#include <optional>
#include <vector>
#include <filesystem>

namespace theatre {
struct CapturedFrame {
    unsigned width=0,height=0;
    std::int64_t timeMs=0;
    std::vector<std::uint8_t> pixels;
};
CapturedFrame readClientPixels(ID3D11Device* device,ID3D11DeviceContext* context,
                              ID3D11Texture2D* source,const Rect& crop,std::int64_t timeMs);
void writeFramePng(const std::filesystem::path& file,const CapturedFrame& frame);
class WindowCapture {
public:
    explicit WindowCapture(HWND window);
    ~WindowCapture();
    WindowCapture(const WindowCapture&)=delete;
    WindowCapture& operator=(const WindowCapture&)=delete;
    std::optional<CapturedFrame> take();
private:
    struct Impl;
    std::unique_ptr<Impl> m;
};
}
