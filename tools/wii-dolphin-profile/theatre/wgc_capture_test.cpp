// SPDX-License-Identifier: GPL-3.0-only
#include "wgc_capture.h"
#include "d3d_upload.h"
#include <winrt/base.h>
#include <wincodec.h>
#include <wrl/client.h>
#include <iostream>

using Microsoft::WRL::ComPtr;
static void require(bool value) { if(!value) throw std::runtime_error("WGC readback/PNG regression failed"); }
template<class F> static void rejects(F operation) {
    bool failed=false; try { operation(); } catch(const std::exception&) { failed=true; }
    require(failed);
}
int main(int argc,char** argv) {
    try {
        if(argc!=2) return 2;
        winrt::init_apartment(winrt::apartment_type::multi_threaded);
        ComPtr<ID3D11Device> device; ComPtr<ID3D11DeviceContext> context;
        theatre::d3dCheck(D3D11CreateDevice(nullptr,D3D_DRIVER_TYPE_WARP,nullptr,D3D11_CREATE_DEVICE_BGRA_SUPPORT,
            nullptr,0,D3D11_SDK_VERSION,device.GetAddressOf(),nullptr,context.GetAddressOf()),"software WARP fixture");
        std::vector<std::uint8_t> pixels(1284u*724u*4u);
        for(std::size_t i=0;i<pixels.size();++i) pixels[i]=static_cast<std::uint8_t>((i+i/256)%256);
        D3D11_TEXTURE2D_DESC desc{};
        desc.Width=1284;desc.Height=724;desc.MipLevels=1;desc.ArraySize=1;
        desc.Format=DXGI_FORMAT_B8G8R8A8_UNORM;desc.SampleDesc.Count=1;desc.Usage=D3D11_USAGE_DEFAULT;
        D3D11_SUBRESOURCE_DATA initial{pixels.data(),1284*4,0};
        ComPtr<ID3D11Texture2D> source;
        theatre::d3dCheck(device->CreateTexture2D(&desc,&initial,source.GetAddressOf()),"software framed-window fixture");
        const theatre::Rect crop{2,2,1282,722};
        auto frame=theatre::readClientPixels(device.Get(),context.Get(),source.Get(),crop,1234);
        require(frame.width==1280 && frame.height==720 && frame.timeMs==1234);
        for(unsigned row=0;row<720;++row)
            for(unsigned column=0;column<1280*4;++column)
                require(frame.pixels[static_cast<std::size_t>(row)*1280*4+column]==pixels[(static_cast<std::size_t>(row)+2)*1284*4+8+column]);
        rejects([&]{ theatre::readClientPixels(device.Get(),context.Get(),source.Get(),{-1,2,1282,722},0); });
        rejects([&]{ theatre::readClientPixels(device.Get(),context.Get(),nullptr,crop,0); });
        ComPtr<ID3D11Device> other; ComPtr<ID3D11DeviceContext> otherContext;
        theatre::d3dCheck(D3D11CreateDevice(nullptr,D3D_DRIVER_TYPE_WARP,nullptr,0,nullptr,0,D3D11_SDK_VERSION,
            other.GetAddressOf(),nullptr,otherContext.GetAddressOf()),"second software context");
        rejects([&]{ theatre::readClientPixels(device.Get(),otherContext.Get(),source.Get(),crop,0); });
        desc.Format=DXGI_FORMAT_R8G8B8A8_UNORM;
        ComPtr<ID3D11Texture2D> wrongFormat;
        theatre::d3dCheck(device->CreateTexture2D(&desc,&initial,wrongFormat.GetAddressOf()),"unsupported-format fixture");
        rejects([&]{ theatre::readClientPixels(device.Get(),context.Get(),wrongFormat.Get(),crop,0); });
        const auto file=std::filesystem::path(argv[1])/"synthetic-wgc-frame.png";
        std::filesystem::create_directories(file.parent_path());
        if(std::filesystem::exists(file)) throw std::runtime_error("Unexpected existing synthetic fixture");
        theatre::writeFramePng(file,frame);
        rejects([&]{ theatre::writeFramePng(file,frame); });
        ComPtr<IWICImagingFactory> factory;
        theatre::d3dCheck(CoCreateInstance(CLSID_WICImagingFactory,nullptr,CLSCTX_INPROC_SERVER,IID_PPV_ARGS(factory.GetAddressOf())),"PNG fixture factory");
        ComPtr<IWICBitmapDecoder> decoder;
        theatre::d3dCheck(factory->CreateDecoderFromFilename(file.c_str(),nullptr,GENERIC_READ,WICDecodeMetadataCacheOnLoad,decoder.GetAddressOf()),"PNG fixture decode");
        ComPtr<IWICBitmapFrameDecode> image;
        theatre::d3dCheck(decoder->GetFrame(0,image.GetAddressOf()),"PNG fixture frame");
        ComPtr<IWICFormatConverter> converter;
        theatre::d3dCheck(factory->CreateFormatConverter(converter.GetAddressOf()),"PNG fixture conversion");
        theatre::d3dCheck(converter->Initialize(image.Get(),GUID_WICPixelFormat32bppBGRA,WICBitmapDitherTypeNone,nullptr,0,WICBitmapPaletteTypeCustom),"PNG fixture BGRA");
        std::vector<std::uint8_t> decoded(frame.pixels.size());
        theatre::d3dCheck(converter->CopyPixels(nullptr,1280*4,static_cast<UINT>(decoded.size()),decoded.data()),"PNG fixture readback");
        require(decoded==frame.pixels);
        converter.Reset();image.Reset();decoder.Reset();factory.Reset();
        std::filesystem::remove(file);
        std::cout << "Software client crop/full-size PNG round-trip passed; no HWND, WGC session or VR initialized\n";
        return 0;
    } catch(const winrt::hresult_error& error) { std::cerr << winrt::to_string(error.message()) << '\n'; return 1; }
      catch(const std::exception& error) { std::cerr << error.what() << '\n'; return 1; }
}
