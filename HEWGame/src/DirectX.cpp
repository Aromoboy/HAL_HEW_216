#include "TextureDX.h"
#include "RenderStateManager.h"
#include "Debug.h"

namespace
{
    Core::DXContext g_dxContext;
}

ID3D11Device* GetDevice()
{
    return g_dxContext.GetDevice();
}

ID3D11DeviceContext* GetContext()
{
    return g_dxContext.GetContext();
}

IDXGISwapChain* GetSwapChain()
{
    return g_dxContext.GetSwapChain();
}

RenderTargetDX* GetDefaultRTV()
{
    return g_dxContext.GetDefaultRTV();
}

DepthStencilDX* GetDefaultDSV()
{
    return g_dxContext.GetDefaultDSV();
}

HRESULT InitDirectX(HWND hWnd, UINT width, UINT height, bool fullscreen)
{
    HRESULT hr = g_dxContext.Init(hWnd, width, height, fullscreen);
    if (FAILED(hr))
    {
        return hr;
    }

    RenderTargetDX* defaultRTV = g_dxContext.GetDefaultRTV();
    SetRenderTargets(1, &defaultRTV, g_dxContext.GetDefaultDSV());

    auto& stateManager = RenderStateManager::Instance();
    stateManager.Initialize(GetDevice());
    stateManager.SetRasterizer(D3D11_CULL_BACK);
    stateManager.SetDepthTest(false);
    stateManager.SetBlend(BlendMode::BLEND_ALPHA);
    stateManager.SetSampler(SamplerState::SAMPLER_LINEAR);

    return S_OK;
}

void UninitDirectX()
{
    RenderStateManager::Instance().Shutdown();
    g_dxContext.Shutdown();
}

void BeginDrawDirectX()
{
    float color[4] = { 0.8f, 0.9f, 1.0f, 1.0f };
    g_dxContext.BeginDraw(color);
}

void EndDrawDirectX()
{
    g_dxContext.EndDraw();

    if (!RenderStateManager::Instance().IsStateStackBalanced())
    {
        DebugMessage("[DirectX] Render state stack was not balanced at frame end.\n");
    }
}

void SetRenderTargets(UINT num, RenderTargetDX** ppViews, DepthStencilDX* pView)
{
    static ID3D11RenderTargetView* rtvs[4];

    if (num > 4) num = 4;
    for (UINT i = 0; i < num; ++i)
    {
        rtvs[i] = ppViews[i]->GetView();
    }
    GetContext()->OMSetRenderTargets(num, rtvs, pView ? pView->GetView() : nullptr);

    D3D11_VIEWPORT vp{};
    vp.TopLeftX = 0.0f;
    vp.TopLeftY = 0.0f;
    vp.Width = static_cast<float>(ppViews[0]->GetWidth());
    vp.Height = static_cast<float>(ppViews[0]->GetHeight());
    vp.MinDepth = 0.0f;
    vp.MaxDepth = 1.0f;
    GetContext()->RSSetViewports(1, &vp);
}

void SetCullingMode(D3D11_CULL_MODE cull)
{
    RenderStateManager::Instance().SetRasterizer(cull);
}

void SetDepthTest(bool enable)
{
    RenderStateManager::Instance().SetDepthTest(enable);
}

void SetBlendMode(BlendMode blend)
{
    RenderStateManager::Instance().SetBlend(blend);
}

void SetSamplerState(SamplerState state)
{
    RenderStateManager::Instance().SetSampler(state);
}

