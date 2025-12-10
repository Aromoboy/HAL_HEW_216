#ifndef __TEXTUREDX_H__
#define __TEXTUREDX_H__

#include <string>
#include <d3d11.h>
#include <wrl/client.h>

class Texture2D
{
public:
    bool LoadFromFile(ID3D11Device* device, const std::wstring& path);
    bool LoadFromMemory(ID3D11Device* device, const uint8_t* data, size_t size, unsigned int width = 0, unsigned int height = 0, const char* formatHint = nullptr);
    bool CreateBlank(ID3D11Device* device, DXGI_FORMAT format, UINT width, UINT height, const void* initialData = nullptr, UINT pitch = 0);
    bool UpdatePixels(ID3D11DeviceContext* context, const void* data, UINT pitch);
    ID3D11ShaderResourceView* Get() const { return textureView.Get(); }
    UINT GetWidth() const { return width; }
    UINT GetHeight() const { return height; }

private:
    UINT width = 0;
    UINT height = 0;
    Microsoft::WRL::ComPtr<ID3D11Texture2D> texture;
    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> textureView;
};

class RenderTargetDX
{
public:
    bool Create(ID3D11Device* device, UINT width, UINT height, DXGI_FORMAT format = DXGI_FORMAT_R8G8B8A8_UNORM);
    bool CreateFromSwapChain(ID3D11Device* device, IDXGISwapChain* swapChain);
    void Clear(ID3D11DeviceContext* context, const float* color) const;
    void Clear(ID3D11DeviceContext* context) const;
    ID3D11RenderTargetView* GetView() const { return renderTargetView.Get(); }
    UINT GetWidth() const { return width; }
    UINT GetHeight() const { return height; }
    DXGI_FORMAT GetFormat() const { return format; }

private:
    DXGI_FORMAT format = DXGI_FORMAT_UNKNOWN;
    UINT width = 0;
    UINT height = 0;
    Microsoft::WRL::ComPtr<ID3D11Texture2D> texture;
    Microsoft::WRL::ComPtr<ID3D11RenderTargetView> renderTargetView;
};

class DepthStencilDX
{
public:
    bool Create(ID3D11Device* device, UINT width, UINT height, bool useStencil);
    void Clear(ID3D11DeviceContext* context) const;
    ID3D11DepthStencilView* GetView() const { return depthStencilView.Get(); }
    UINT GetWidth() const { return width; }
    UINT GetHeight() const { return height; }

private:
    DXGI_FORMAT format = DXGI_FORMAT_UNKNOWN;
    UINT width = 0;
    UINT height = 0;
    Microsoft::WRL::ComPtr<ID3D11Texture2D> texture;
    Microsoft::WRL::ComPtr<ID3D11DepthStencilView> depthStencilView;
};
#endif
