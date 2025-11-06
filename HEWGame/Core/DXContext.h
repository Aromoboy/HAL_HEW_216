#pragma once

#include <windows.h>
#include <d3d11.h>
#include <wrl/client.h>

namespace Core
{

class DXContext
{
public:
        DXContext() = default;
        ~DXContext();

        HRESULT Init(HWND hwnd, UINT width, UINT height, BOOL fullscreen);
        HRESULT Resize(UINT width, UINT height);

        void BeginDraw();
        void BeginDraw(const FLOAT clearColor[4]);
        void EndDraw();
        void Shutdown();

        ID3D11Device* GetDevice() const noexcept { return m_device.Get(); }
        ID3D11DeviceContext* GetContext() const noexcept { return m_context.Get(); }
        IDXGISwapChain* GetSwapChain() const noexcept { return m_swapChain.Get(); }

private:
        HRESULT CreateDeviceAndSwapChain(HWND hwnd, UINT width, UINT height, BOOL fullscreen);
        HRESULT CreateRenderTarget(UINT width, UINT height);
        HRESULT ConfigureBlendState();
        void LogFailure(const char* stage, HRESULT hr) const;
        void UpdateViewport(UINT width, UINT height);

        Microsoft::WRL::ComPtr<ID3D11Device> m_device;
        Microsoft::WRL::ComPtr<ID3D11DeviceContext> m_context;
        Microsoft::WRL::ComPtr<IDXGISwapChain> m_swapChain;
        Microsoft::WRL::ComPtr<ID3D11RenderTargetView> m_renderTarget;
        Microsoft::WRL::ComPtr<ID3D11BlendState> m_blendState;
        D3D11_VIEWPORT m_viewport{};
        FLOAT m_defaultClearColor[4] = { 0.8f, 0.9f, 1.0f, 1.0f };
};

} // namespace Core

