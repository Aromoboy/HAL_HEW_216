#pragma once

#include <memory>
#include <wrl/client.h>
#include <d3d11.h>

class RenderTargetDX;
class DepthStencilDX;

namespace Core
{
        class DXContext
        {
        public:
                DXContext();
                ~DXContext();

                HRESULT Init(HWND hWnd, UINT width, UINT height, bool fullscreen);
                void BeginDraw(const float clearColor[4]);
                void EndDraw();
                HRESULT Resize(UINT width, UINT height);
                void Shutdown();

                ID3D11Device* GetDevice() const;
                ID3D11DeviceContext* GetContext() const;
                IDXGISwapChain* GetSwapChain() const;
                RenderTargetDX* GetDefaultRTV() const;
                DepthStencilDX* GetDefaultDSV() const;

        private:
                HRESULT CreateDeviceAndSwapChain(HWND hWnd, UINT width, UINT height, bool fullscreen);
                HRESULT CreateDefaultTargets();
                void ReleaseTargets();

        private:
                Microsoft::WRL::ComPtr<ID3D11Device> m_device;
                Microsoft::WRL::ComPtr<ID3D11DeviceContext> m_context;
                Microsoft::WRL::ComPtr<IDXGISwapChain> m_swapChain;
                std::unique_ptr<RenderTargetDX> m_defaultRTV;
                std::unique_ptr<DepthStencilDX> m_defaultDSV;
                HWND m_windowHandle;
                UINT m_width;
                UINT m_height;
        };
}

