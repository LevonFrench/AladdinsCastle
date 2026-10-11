// SPDX-License-Identifier: GPL-3.0-only
#include "d3d_upload.h"
#include "core.h"
#include <cstring>
#include <iostream>
#include <vector>

using Microsoft::WRL::ComPtr;
static void require(bool value) { if(!value) throw std::runtime_error("D3D upload regression failed"); }
template<class F> static void rejects(F operation) {
    bool failed=false; try { operation(); } catch(const std::exception&) { failed=true; }
    require(failed);
}
static void verify(ID3D11Device* device,ID3D11DeviceContext* context,ID3D11Texture2D* texture,
                   unsigned w,unsigned h,const std::vector<std::uint8_t>& pixels) {
    D3D11_TEXTURE2D_DESC description{}; texture->GetDesc(&description);
    require(description.Width==w && description.Height==h && description.MipLevels==1 && description.ArraySize==1);
    require(description.Format==DXGI_FORMAT_B8G8R8A8_UNORM && description.SampleDesc.Count==1);
    require(description.Usage==D3D11_USAGE_DEFAULT && description.CPUAccessFlags==0);
    require(description.BindFlags==D3D11_BIND_SHADER_RESOURCE && description.MiscFlags==D3D11_RESOURCE_MISC_SHARED);
    description.Usage=D3D11_USAGE_STAGING; description.BindFlags=0; description.MiscFlags=0;
    description.CPUAccessFlags=D3D11_CPU_ACCESS_READ;
    ComPtr<ID3D11Texture2D> readback;
    theatre::d3dCheck(device->CreateTexture2D(&description,nullptr,readback.GetAddressOf()),"software readback allocation");
    context->CopyResource(readback.Get(),texture);
    D3D11_MAPPED_SUBRESOURCE mapped{};
    theatre::d3dCheck(context->Map(readback.Get(),0,D3D11_MAP_READ,0,&mapped),"software readback map");
    bool identical=true;
    for(unsigned y=0;y<h;++y) {
        const auto* row=static_cast<const std::uint8_t*>(mapped.pData)+static_cast<std::size_t>(y)*mapped.RowPitch;
        identical=identical && std::memcmp(row,pixels.data()+static_cast<std::size_t>(y)*w*4,w*4)==0;
    }
    context->Unmap(readback.Get(),0);
    require(identical); // Every BGRA byte, both SBS halves, alpha and vertical ordering.
}
int main() {
    try {
        rejects([]{ theatre::D3DUpload::onAdapter(-1); }); // Reject before DXGI enumeration.
        rejects([]{ theatre::D3DUpload empty({},{}); });
        ComPtr<ID3D11Device> device;
        ComPtr<ID3D11DeviceContext> context;
        // WARP only: never enumerate/use a hardware adapter, create a window or initialize VR.
        theatre::d3dCheck(D3D11CreateDevice(nullptr,D3D_DRIVER_TYPE_WARP,nullptr,D3D11_CREATE_DEVICE_BGRA_SUPPORT,
            nullptr,0,D3D11_SDK_VERSION,device.GetAddressOf(),nullptr,context.GetAddressOf()),"software-WARP test device");
        theatre::D3DUpload upload(device,context);
        std::vector<std::uint8_t> pixels(theatre::imageBytes(640,480));
        for(std::size_t i=0;i<pixels.size();++i) pixels[i]=static_cast<std::uint8_t>((i*7+i/256)%256);
        rejects([&]{ upload.upload(640,480,nullptr,pixels.size()); });
        rejects([&]{ upload.upload(640,480,pixels.data(),pixels.size()-1); });
        rejects([&]{ upload.upload(640,480,pixels.data(),pixels.size()+1); });
        rejects([&]{ upload.upload(641,480,pixels.data(),pixels.size()); });
        upload.upload(640,480,pixels.data(),pixels.size());
        verify(device.Get(),context.Get(),upload.texture(),640,480,pixels);
        ComPtr<ID3D11Texture2D> original=upload.texture();
        pixels[0]=240; pixels.back()=53;
        upload.upload(640,480,pixels.data(),pixels.size());
        require(original.Get()==upload.texture());
        verify(device.Get(),context.Get(),upload.texture(),640,480,pixels);
        const auto unchanged=upload.texture();
        rejects([&]{ upload.upload(5000,480,pixels.data(),pixels.size()); });
        require(unchanged==upload.texture());
        pixels.assign(theatre::imageBytes(800,600),19);
        upload.upload(800,600,pixels.data(),pixels.size());
        require(original.Get()!=upload.texture());
        verify(device.Get(),context.Get(),upload.texture(),800,600,pixels);
        ComPtr<ID3D11Device> otherDevice;
        ComPtr<ID3D11DeviceContext> otherContext;
        theatre::d3dCheck(D3D11CreateDevice(nullptr,D3D_DRIVER_TYPE_WARP,nullptr,0,nullptr,0,D3D11_SDK_VERSION,
            otherDevice.GetAddressOf(),nullptr,otherContext.GetAddressOf()),"second software device");
        rejects([&]{ theatre::D3DUpload mismatch(device,otherContext); });
        std::cout << "Software-WARP full-size BGRA upload/reuse/resize/readback and rejection checks passed; no hardware/VR/capture\n";
        return 0;
    } catch(const std::exception& error) { std::cerr << error.what() << '\n'; return 1; }
}
