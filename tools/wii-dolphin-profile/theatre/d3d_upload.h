// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include <d3d11.h>
#include <wrl/client.h>
#include <memory>
#include <cstddef>

namespace theatre {
class D3DUpload {
public:
    static std::unique_ptr<D3DUpload> onAdapter(int index);
    // Injected device/context permits a strictly software-WARP regression.
    D3DUpload(Microsoft::WRL::ComPtr<ID3D11Device> device,
              Microsoft::WRL::ComPtr<ID3D11DeviceContext> context);
    ~D3DUpload();
    D3DUpload(const D3DUpload&)=delete;
    D3DUpload& operator=(const D3DUpload&)=delete;
    bool matches(unsigned w,unsigned h) const;
    void upload(unsigned w,unsigned h,const void* bgra,std::size_t bytes);
    ID3D11Texture2D* texture() const { return m_texture.Get(); }
private:
    Microsoft::WRL::ComPtr<ID3D11Device> m_device;
    Microsoft::WRL::ComPtr<ID3D11DeviceContext> m_context;
    Microsoft::WRL::ComPtr<ID3D11Texture2D> m_texture;
    unsigned m_width=0,m_height=0;
};
void d3dCheck(HRESULT result,const char* operation);
}
