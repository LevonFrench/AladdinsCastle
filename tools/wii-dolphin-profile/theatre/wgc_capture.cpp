// SPDX-License-Identifier: GPL-3.0-only
#include "wgc_capture.h"
#include "d3d_upload.h"
#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.Graphics.Capture.h>
#include <winrt/Windows.Graphics.DirectX.Direct3D11.h>
#include <windows.graphics.capture.interop.h>
#include <windows.graphics.directx.direct3d11.interop.h>
#include <dxgi.h>
#include <d3d11_4.h>
#include <dwmapi.h>
#include <wincodec.h>
#include <objbase.h>
#include <wrl/client.h>
#include <cstring>

namespace theatre {
using Microsoft::WRL::ComPtr;
namespace capture=winrt::Windows::Graphics::Capture;
namespace direct=winrt::Windows::Graphics::DirectX::Direct3D11;
namespace graphics=winrt::Windows::Graphics;

CapturedFrame readClientPixels(ID3D11Device* device,ID3D11DeviceContext* context,
                              ID3D11Texture2D* source,const Rect& crop,std::int64_t timeMs) {
    if(!device || !context || !source) throw std::runtime_error("WGC texture/device/context missing");
    ComPtr<ID3D11Device> contextDevice,sourceDevice;
    context->GetDevice(contextDevice.GetAddressOf()); source->GetDevice(sourceDevice.GetAddressOf());
    if(contextDevice.Get()!=device || sourceDevice.Get()!=device)
        throw std::runtime_error("WGC readback device/context/texture mismatch");
    D3D11_TEXTURE2D_DESC desc{}; source->GetDesc(&desc);
    validateCrop(crop,desc.Width,desc.Height);
    if(desc.Format!=DXGI_FORMAT_B8G8R8A8_UNORM || desc.SampleDesc.Count!=1)
        throw std::runtime_error("WGC returned unsupported pixel format");
    CapturedFrame result;
    result.width=static_cast<unsigned>(crop.right-crop.left);
    result.height=static_cast<unsigned>(crop.bottom-crop.top);
    result.timeMs=timeMs; result.pixels.resize(imageBytes(result.width,result.height));
    desc.Width=result.width; desc.Height=result.height; desc.MipLevels=1; desc.ArraySize=1;
    desc.Usage=D3D11_USAGE_STAGING; desc.CPUAccessFlags=D3D11_CPU_ACCESS_READ;
    desc.BindFlags=0; desc.MiscFlags=0;
    ComPtr<ID3D11Texture2D> staging;
    d3dCheck(device->CreateTexture2D(&desc,nullptr,staging.GetAddressOf()),"WGC client staging texture");
    D3D11_BOX box{static_cast<UINT>(crop.left),static_cast<UINT>(crop.top),0,
                 static_cast<UINT>(crop.right),static_cast<UINT>(crop.bottom),1};
    context->CopySubresourceRegion(staging.Get(),0,0,0,0,source,0,&box);
    D3D11_MAPPED_SUBRESOURCE mapped{};
    d3dCheck(context->Map(staging.Get(),0,D3D11_MAP_READ,0,&mapped),"WGC client readback");
    for(unsigned row=0;row<result.height;++row)
        std::memcpy(result.pixels.data()+static_cast<std::size_t>(row)*result.width*4,
            static_cast<const std::uint8_t*>(mapped.pData)+static_cast<std::size_t>(row)*mapped.RowPitch,result.width*4);
    context->Unmap(staging.Get(),0);
    return result;
}
static void encodePng(IStream* stream,unsigned imageWidth,unsigned imageHeight,const std::vector<std::uint8_t>& pixels) {
    if(pixels.size()!=imageBytes(imageWidth,imageHeight)) throw std::runtime_error("PNG requires exact client pixels");
    ComPtr<IWICImagingFactory> factory;
    d3dCheck(CoCreateInstance(CLSID_WICImagingFactory,nullptr,CLSCTX_INPROC_SERVER,IID_PPV_ARGS(factory.GetAddressOf())),"WIC factory");
    ComPtr<IWICBitmapEncoder> encoder;
    d3dCheck(factory->CreateEncoder(GUID_ContainerFormatPng,nullptr,encoder.GetAddressOf()),"PNG encoder");
    d3dCheck(encoder->Initialize(stream,WICBitmapEncoderNoCache),"PNG initialize");
    ComPtr<IWICBitmapFrameEncode> output;
    d3dCheck(encoder->CreateNewFrame(output.GetAddressOf(),nullptr),"PNG frame");
    d3dCheck(output->Initialize(nullptr),"PNG frame initialize");
    d3dCheck(output->SetSize(imageWidth,imageHeight),"PNG size");
    auto format=GUID_WICPixelFormat32bppBGRA;
    d3dCheck(output->SetPixelFormat(&format),"PNG BGRA format");
    if(format!=GUID_WICPixelFormat32bppBGRA) throw std::runtime_error("PNG encoder changed pixel format");
    d3dCheck(output->WritePixels(imageHeight,imageWidth*4,static_cast<UINT>(pixels.size()),
        const_cast<BYTE*>(pixels.data())),"PNG pixels");
    d3dCheck(output->Commit(),"PNG frame commit"); d3dCheck(encoder->Commit(),"PNG commit");
}
void writeFramePng(const std::filesystem::path& file,const CapturedFrame& frame) {
    if(frame.pixels.size()!=imageBytes(frame.width,frame.height) || std::filesystem::exists(file))
        throw std::runtime_error("Proof PNG requires valid pixels and a fresh filename");
    ComPtr<IWICImagingFactory> factory;
    d3dCheck(CoCreateInstance(CLSID_WICImagingFactory,nullptr,CLSCTX_INPROC_SERVER,IID_PPV_ARGS(factory.GetAddressOf())),"WIC factory");
    ComPtr<IWICStream> stream;
    d3dCheck(factory->CreateStream(stream.GetAddressOf()),"WIC stream");
    d3dCheck(stream->InitializeFromFilename(file.c_str(),GENERIC_WRITE),"Private proof PNG");
    ComPtr<IWICBitmapEncoder> encoder;
    d3dCheck(factory->CreateEncoder(GUID_ContainerFormatPng,nullptr,encoder.GetAddressOf()),"PNG encoder");
    d3dCheck(encoder->Initialize(stream.Get(),WICBitmapEncoderNoCache),"PNG initialize");
    ComPtr<IWICBitmapFrameEncode> output;
    d3dCheck(encoder->CreateNewFrame(output.GetAddressOf(),nullptr),"PNG frame");
    d3dCheck(output->Initialize(nullptr),"PNG frame initialize");
    d3dCheck(output->SetSize(frame.width,frame.height),"PNG size");
    auto format=GUID_WICPixelFormat32bppBGRA;
    d3dCheck(output->SetPixelFormat(&format),"PNG BGRA format");
    if(format!=GUID_WICPixelFormat32bppBGRA) throw std::runtime_error("PNG encoder changed pixel format");
    d3dCheck(output->WritePixels(frame.height,frame.width*4,static_cast<UINT>(frame.pixels.size()),
        const_cast<BYTE*>(frame.pixels.data())),"PNG pixels");
    d3dCheck(output->Commit(),"PNG frame commit"); d3dCheck(encoder->Commit(),"PNG commit");
}
void writeNewPrivateFile(const std::filesystem::path& file,const std::uint8_t* bytes,std::size_t length) {
    HANDLE handle=CreateFileW(file.c_str(),GENERIC_WRITE,0,nullptr,CREATE_NEW,FILE_ATTRIBUTE_NORMAL,nullptr);
    if(handle==INVALID_HANDLE_VALUE) throw std::runtime_error("Cannot create fresh failure diagnostic file");
    try {
        while(length) {
            const auto chunk=static_cast<DWORD>(std::min<std::size_t>(length,1024*1024));
            DWORD written=0;
            if(!WriteFile(handle,bytes,chunk,&written,nullptr) || written!=chunk)
                throw std::runtime_error("Cannot write private failure diagnostic file");
            bytes+=written; length-=written;
        }
        if(!FlushFileBuffers(handle)) throw std::runtime_error("Cannot flush private failure diagnostic file");
    } catch(...) { CloseHandle(handle); throw; }
    CloseHandle(handle);
}
void writeFailurePng(const std::filesystem::path& file,const DiagnosticSample& frame) {
    ComPtr<IStream> stream;
    d3dCheck(CreateStreamOnHGlobal(nullptr,TRUE,stream.GetAddressOf()),"Failure PNG memory stream");
    encodePng(stream.Get(),frame.width,frame.height,frame.pixels);
    STATSTG stat{};
    d3dCheck(stream->Stat(&stat,STATFLAG_NONAME),"Failure PNG length");
    if(stat.cbSize.QuadPart>64*1024*1024) throw std::runtime_error("Failure PNG exceeds bounded encoding");
    HGLOBAL memory=nullptr;
    d3dCheck(GetHGlobalFromStream(stream.Get(),&memory),"Failure PNG bytes");
    const auto* bytes=static_cast<const std::uint8_t*>(GlobalLock(memory));
    if(!bytes) throw std::runtime_error("Cannot access failure PNG bytes");
    try { writeNewPrivateFile(file,bytes,static_cast<std::size_t>(stat.cbSize.QuadPart)); }
    catch(...) { GlobalUnlock(memory); throw; }
    GlobalUnlock(memory);
}
struct WindowCapture::Impl {
    CaptureObservation observation;
    HWND window{};
    ComPtr<ID3D11Device> device;
    ComPtr<ID3D11DeviceContext> context;
    direct::IDirect3DDevice runtimeDevice{nullptr};
    capture::GraphicsCaptureItem item{nullptr};
    capture::Direct3D11CaptureFramePool pool{nullptr};
    capture::GraphicsCaptureSession session{nullptr};
    graphics::SizeInt32 size{};
    ~Impl() {
        try { if(session) session.Close(); if(pool) pool.Close(); } catch(...) {}
        session=nullptr; pool=nullptr; item=nullptr; runtimeDevice=nullptr;
        if(context) { context->ClearState(); context->Flush(); }
    }
};
WindowCapture::WindowCapture(HWND window):m(std::make_unique<Impl>()) {
    m->window=window;
    if(!IsWindow(window)) throw std::runtime_error("Selected capture window is gone");
    if(!capture::GraphicsCaptureSession::IsSupported()) throw std::runtime_error("Windows.Graphics.Capture is unavailable");
    d3dCheck(D3D11CreateDevice(nullptr,D3D_DRIVER_TYPE_HARDWARE,nullptr,D3D11_CREATE_DEVICE_BGRA_SUPPORT,
        nullptr,0,D3D11_SDK_VERSION,m->device.GetAddressOf(),nullptr,m->context.GetAddressOf()),"WGC capture device");
    ComPtr<ID3D11Multithread> multithread;
    d3dCheck(m->context.As(&multithread),"WGC multithread protection");
    multithread->SetMultithreadProtected(TRUE);
    ComPtr<IDXGIDevice> dxgi;
    d3dCheck(m->device.As(&dxgi),"WGC DXGI device");
    winrt::com_ptr<IInspectable> inspectable;
    winrt::check_hresult(CreateDirect3D11DeviceFromDXGIDevice(dxgi.Get(),inspectable.put()));
    m->runtimeDevice=inspectable.as<direct::IDirect3DDevice>();
    auto factory=winrt::get_activation_factory<capture::GraphicsCaptureItem,IGraphicsCaptureItemInterop>();
    winrt::check_hresult(factory->CreateForWindow(window,winrt::guid_of<capture::GraphicsCaptureItem>(),winrt::put_abi(m->item)));
    m->size=m->item.Size(); captureSize(m->size.Width,m->size.Height);
    m->observation.sourceWidth=static_cast<unsigned>(m->size.Width);
    m->observation.sourceHeight=static_cast<unsigned>(m->size.Height);
    m->pool=capture::Direct3D11CaptureFramePool::CreateFreeThreaded(m->runtimeDevice,
        winrt::Windows::Graphics::DirectX::DirectXPixelFormat::B8G8R8A8UIntNormalized,2,m->size);
    m->session=m->pool.CreateCaptureSession(m->item);
    m->session.IsCursorCaptureEnabled(false);
    m->session.StartCapture();
}
WindowCapture::~WindowCapture()=default;
CaptureObservation WindowCapture::lastObservation() const { return m->observation; }
std::optional<CapturedFrame> WindowCapture::take() {
    m->observation.cropValid=false;
    auto frame=acquireCaptureFrame(m->observation,[&]{ return m->pool.TryGetNextFrame(); });
    if(!frame) return std::nullopt;
    auto size=frame.ContentSize(); captureSize(size.Width,size.Height);
    m->observation.sourceWidth=static_cast<unsigned>(size.Width);
    m->observation.sourceHeight=static_cast<unsigned>(size.Height);
    if(size.Width!=m->size.Width || size.Height!=m->size.Height) {
        frame.Close(); m->size=size;
        m->pool.Recreate(m->runtimeDevice,winrt::Windows::Graphics::DirectX::DirectXPixelFormat::B8G8R8A8UIntNormalized,2,size);
        m->observation.kind=CapturePoll::PoolRecreated;
        return std::nullopt;
    }
    // Match WGC's frame origin to this selected window's physical geometry.
    // No other window metadata or desktop pixels are queried.
    RECT window{},client{}; POINT origin{};
    if(!GetClientRect(m->window,&client) || !ClientToScreen(m->window,&origin))
        throw std::runtime_error("Selected client geometry unavailable");
    bool matched=SUCCEEDED(DwmGetWindowAttribute(m->window,DWMWA_EXTENDED_FRAME_BOUNDS,&window,sizeof(window))) &&
        window.right-window.left==size.Width && window.bottom-window.top==size.Height;
    if(!matched) matched=GetWindowRect(m->window,&window) && window.right-window.left==size.Width && window.bottom-window.top==size.Height;
    if(!matched) throw std::runtime_error("WGC frame does not match selected window geometry");
    const Rect crop{origin.x-window.left,origin.y-window.top,
        origin.x-window.left+client.right,origin.y-window.top+client.bottom};
    validateCrop(crop,static_cast<unsigned>(size.Width),static_cast<unsigned>(size.Height));
    m->observation.crop=crop;
    m->observation.cropValid=true;
    auto access=frame.Surface().as<::Windows::Graphics::DirectX::Direct3D11::IDirect3DDxgiInterfaceAccess>();
    ComPtr<ID3D11Texture2D> texture;
    winrt::check_hresult(access->GetInterface(IID_PPV_ARGS(texture.GetAddressOf())));
    const auto time=frame.SystemRelativeTime().count()/10000;
    auto result=readClientPixels(m->device.Get(),m->context.Get(),texture.Get(),crop,time);
    m->observation.kind=CapturePoll::Frame;
    frame.Close(); return result;
}
}
