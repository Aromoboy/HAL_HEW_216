#pragma once

#include <memory>
#include <wrl/client.h>
#include <d3d11.h>

class RenderTarget;
class DepthStencil;

namespace Core
{
        struct SceneRenderTargets
        {
                std::unique_ptr<RenderTarget> color;
                std::unique_ptr<DepthStencil> depth;
        };

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
                UINT GetWidth() const;
                UINT GetHeight() const;
                ID3D11RenderTargetView* GetSceneColorRTV() const;
                ID3D11ShaderResourceView* GetSceneColorSRV() const;
                ID3D11DepthStencilView* GetSceneDepthDSV() const;
                ID3D11ShaderResourceView* GetSceneDepthSRV() const;

        private:
                HRESULT CreateDeviceAndSwapChain(HWND hWnd, UINT width, UINT height, bool fullscreen);
                HRESULT CreateDefaultTargets();
                HRESULT CreateSceneTargets(UINT width, UINT height);
                void ReleaseTargets();
                void ReleaseSceneTargets();

        private:
                Microsoft::WRL::ComPtr<ID3D11Device> m_device;
                Microsoft::WRL::ComPtr<ID3D11DeviceContext> m_context;
                Microsoft::WRL::ComPtr<IDXGISwapChain> m_swapChain;
                std::unique_ptr<RenderTarget> m_defaultRTV;
                std::unique_ptr<DepthStencil> m_defaultDSV;
                SceneRenderTargets m_sceneTargets;
                HWND m_windowHandle;
                UINT m_width;
                UINT m_height;
                bool m_ready;
        };
}
