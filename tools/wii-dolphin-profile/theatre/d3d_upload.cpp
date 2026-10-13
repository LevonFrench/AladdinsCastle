// SPDX-License-Identifier: GPL-3.0-only
#include "d3d_upload.h"
#include "core.h"
#include <dxgi1_2.h>
#include <sstream>
#include <utility>

namespace theatre {
using Microsoft::WRL::ComPtr;
void d3dCheck(HRESULT result,const char* operation) {
    if(FAILED(result)) {
        std::ostringstream message;
        message << operation << ": HRESULT=0x" << std::hex << static_cast<unsigned long>(result);
        if(result==DXGI_ERROR_DEVICE_REMOVED) message << " DXGI_ERROR_DEVICE_REMOVED";
        else if(result==DXGI_ERROR_DEVICE_RESET) message << " DXGI_ERROR_DEVICE_RESET";
        else if(result==DXGI_ERROR_UNSUPPORTED) message << " DXGI_ERROR_UNSUPPORTED";
        else if(result==E_INVALIDARG) message << " E_INVALIDARG";
        throw std::runtime_error(message.str());
    }
}
std::unique_ptr<D3DUpload> D3DUpload::onAdapter(int index) {
    if(index<0) throw std::runtime_error("SteamVR returned invalid D3D11 adapter index; no default-adapter fallback");
    ComPtr<IDXGIFactory1> factory;
    d3dCheck(CreateDXGIFactory1(IID_PPV_ARGS(factory.GetAddressOf())),"CreateDXGIFactory1");
    ComPtr<IDXGIAdapter1> adapter;
    d3dCheck(factory->EnumAdapters1(static_cast<UINT>(index),adapter.GetAddressOf()),"SteamVR adapter lookup");
    ComPtr<ID3D11Device> device;
    ComPtr<ID3D11DeviceContext> context;
    const D3D_FEATURE_LEVEL requested[]={D3D_FEATURE_LEVEL_11_0,D3D_FEATURE_LEVEL_10_1,D3D_FEATURE_LEVEL_10_0};
    d3dCheck(D3D11CreateDevice(adapter.Get(),D3D_DRIVER_TYPE_UNKNOWN,nullptr,D3D11_CREATE_DEVICE_BGRA_SUPPORT,
        requested,3,D3D11_SDK_VERSION,device.GetAddressOf(),nullptr,context.GetAddressOf()),"D3D11CreateDevice on SteamVR adapter");
    return std::make_unique<D3DUpload>(std::move(device),std::move(context));
}
D3DUpload::D3DUpload(ComPtr<ID3D11Device> device,ComPtr<ID3D11DeviceContext> context)
    :m_device(std::move(device)),m_context(std::move(context)) {
    if(!m_device || !m_context) throw std::runtime_error("D3D11 device/context unavailable");
    ComPtr<ID3D11Device> contextDevice;
    m_context->GetDevice(contextDevice.GetAddressOf());
    if(contextDevice.Get()!=m_device.Get()) throw std::runtime_error("D3D11 context belongs to a different device");
}
D3DUpload::~D3DUpload() {
    if(m_context) { m_context->ClearState(); m_context->Flush(); }
    m_texture.Reset(); m_context.Reset(); m_device.Reset();
}
bool D3DUpload::matches(unsigned w,unsigned h) const {
    return m_texture && m_width==w && m_height==h;
}
void D3DUpload::upload(unsigned w,unsigned h,const void* bgra,std::size_t bytes) {
    const auto expected=imageBytes(w,h);
    if(!bgra || bytes!=expected) throw std::runtime_error("D3D11 upload requires exact packed BGRA client bytes");
    d3dCheck(m_device->GetDeviceRemovedReason(),"D3D11 device status");
    if(!matches(w,h)) {
        D3D11_TEXTURE2D_DESC description{};
        description.Width=w; description.Height=h; description.MipLevels=1; description.ArraySize=1;
        description.Format=DXGI_FORMAT_B8G8R8A8_UNORM;
        description.SampleDesc.Count=1; description.Usage=D3D11_USAGE_DEFAULT;
        description.BindFlags=D3D11_BIND_SHADER_RESOURCE;
        description.MiscFlags=D3D11_RESOURCE_MISC_SHARED;
        ComPtr<ID3D11Texture2D> next;
        d3dCheck(m_device->CreateTexture2D(&description,nullptr,next.GetAddressOf()),"BGRA8 shared overlay texture");
        m_texture=std::move(next); m_width=w; m_height=h;
    }
    // GDI DIB is top-down and packed. No channel swap, rescale, flip or capture fallback.
    m_context->UpdateSubresource(m_texture.Get(),0,nullptr,bgra,w*4,0);
    m_context->Flush();
    d3dCheck(m_device->GetDeviceRemovedReason(),"D3D11 upload completion");
}
}
