#pragma once

#include <memory>
#include <wrl/client.h>
#include <d3d11.h>

class RenderTarget;
class DepthStencil;

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

                bool IsReady() const { return m_ready; }

                ID3D11Device* GetDevice() const;
                ID3D11DeviceContext* GetContext() const;
                IDXGISwapChain* GetSwapChain() const;
                RenderTarget* GetDefaultRTV() const;
                DepthStencil* GetDefaultDSV() const;

        private:
                HRESULT CreateDeviceAndSwapChain(HWND hWnd, UINT width, UINT height, bool fullscreen);
                HRESULT CreateDefaultTargets();
                void ReleaseTargets();

        private:
                Microsoft::WRL::ComPtr<ID3D11Device> m_device;
                Microsoft::WRL::ComPtr<ID3D11DeviceContext> m_context;
                Microsoft::WRL::ComPtr<IDXGISwapChain> m_swapChain;
                std::unique_ptr<RenderTarget> m_defaultRTV;
                std::unique_ptr<DepthStencil> m_defaultDSV;
                HWND m_windowHandle;
                UINT m_width;
                UINT m_height;
                bool m_ready;
        };
}

