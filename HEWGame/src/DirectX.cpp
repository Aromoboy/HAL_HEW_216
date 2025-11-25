#include "DirectX.h"
#include "Texture.h"
#include "Core/DXContext.h"

namespace
{
	Core::DXContext g_dxContext;
	ID3D11RasterizerState*		g_pRasterizerState[3];
	ID3D11DepthStencilState*	g_pDepthStencilState[2];
	ID3D11BlendState*			g_pBlendState[BLEND_MAX];
	ID3D11SamplerState*			g_pSamplerState[SAMPLER_MAX];
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
RenderTarget* GetDefaultRTV()
{
	return g_dxContext.GetDefaultRTV();
}
DepthStencil* GetDefaultDSV()
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

        RenderTarget* defaultRTV = g_dxContext.GetDefaultRTV();
        SetRenderTargets(1, &defaultRTV, g_dxContext.GetDefaultDSV());
        
//--- カリング設定
	D3D11_RASTERIZER_DESC rasterizer = {};
	D3D11_CULL_MODE cull[] = {
		D3D11_CULL_NONE,
		D3D11_CULL_FRONT,
		D3D11_CULL_BACK
	};
	rasterizer.FillMode = D3D11_FILL_SOLID;
	rasterizer.FrontCounterClockwise = false;
	for (int i = 0; i < 3; ++i)
	{
		rasterizer.CullMode = cull[i];
		hr = GetDevice()->CreateRasterizerState(&rasterizer, &g_pRasterizerState[i]);
		if (FAILED(hr)) { return hr; }
	}
	SetCullingMode(D3D11_CULL_BACK);

	//--- 深度テスト
	// https://learn.microsoft.com/ja-jp/windows/win32/direct3d11/d3d10-graphics-programming-guide-depth-stencil
	// https://qiita.com/wyt5818956/items/a2a36a1e6c7910512e7a
	D3D11_DEPTH_STENCIL_DESC dsDesc = {};
	// depth test parameter
	dsDesc.DepthEnable = true;
	dsDesc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ALL;
	dsDesc.DepthFunc = D3D11_COMPARISON_LESS;
	// stencil test parameter
	dsDesc.StencilEnable = true;
	dsDesc.StencilReadMask = 0xFF;
	dsDesc.StencilWriteMask = 0xFF;
	// stencil operations
	dsDesc.FrontFace.StencilFailOp = D3D11_STENCIL_OP_KEEP;
	dsDesc.FrontFace.StencilDepthFailOp = D3D11_STENCIL_OP_INCR;
	dsDesc.FrontFace.StencilPassOp = D3D11_STENCIL_OP_KEEP;
	dsDesc.FrontFace.StencilFunc = D3D11_COMPARISON_ALWAYS;
	dsDesc.BackFace = dsDesc.FrontFace;
	dsDesc.BackFace.StencilFailOp = D3D11_STENCIL_OP_DECR;
	// create state
	GetDevice()->CreateDepthStencilState(&dsDesc, &g_pDepthStencilState[0]);
	dsDesc.DepthEnable = false;
	GetDevice()->CreateDepthStencilState(&dsDesc, &g_pDepthStencilState[1]);
	SetDepthTest(false);

	//--- アルファブレンディング
	// https://pgming-ctrl.com/directx11/blend/
	D3D11_BLEND_DESC blendDesc = {};
	blendDesc.AlphaToCoverageEnable = FALSE;
	blendDesc.IndependentBlendEnable = FALSE;
	blendDesc.RenderTarget[0].BlendEnable = TRUE;
	blendDesc.RenderTarget[0].BlendOp = D3D11_BLEND_OP_ADD;
	blendDesc.RenderTarget[0].BlendOpAlpha = D3D11_BLEND_OP_ADD;
	blendDesc.RenderTarget[0].SrcBlendAlpha = D3D11_BLEND_ONE;
	blendDesc.RenderTarget[0].DestBlendAlpha = D3D11_BLEND_ZERO;
	blendDesc.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;
	D3D11_BLEND blend[BLEND_MAX][2] = {
		{D3D11_BLEND_ONE, D3D11_BLEND_ZERO},
		{D3D11_BLEND_SRC_ALPHA, D3D11_BLEND_INV_SRC_ALPHA},
		{D3D11_BLEND_ONE, D3D11_BLEND_ONE},
		{D3D11_BLEND_SRC_ALPHA, D3D11_BLEND_ONE},
		{D3D11_BLEND_ZERO, D3D11_BLEND_INV_SRC_COLOR},
		{D3D11_BLEND_INV_DEST_COLOR, D3D11_BLEND_ONE},
	};
	for (int i = 0; i < BLEND_MAX; ++i)
	{
		blendDesc.RenderTarget[0].SrcBlend = blend[i][0];
		blendDesc.RenderTarget[0].DestBlend = blend[i][1];
		hr = GetDevice()->CreateBlendState(&blendDesc, &g_pBlendState[i]);
		if (FAILED(hr)) { return hr; }
	}
	SetBlendMode(BLEND_ALPHA);

	// サンプラー
	D3D11_SAMPLER_DESC samplerDesc = {};
	D3D11_FILTER filter[] = {
		D3D11_FILTER_MIN_MAG_MIP_LINEAR,
		D3D11_FILTER_MIN_MAG_MIP_POINT,
	};
	samplerDesc.AddressU = D3D11_TEXTURE_ADDRESS_WRAP;
	samplerDesc.AddressV = D3D11_TEXTURE_ADDRESS_WRAP;
	samplerDesc.AddressW = D3D11_TEXTURE_ADDRESS_WRAP;
	for (int i = 0; i < SAMPLER_MAX; ++i)
	{
		samplerDesc.Filter = filter[i];
		hr = GetDevice()->CreateSamplerState(&samplerDesc, &g_pSamplerState[i]);
		if (FAILED(hr)) { return hr; }
	}
	SetSamplerState(SAMPLER_LINEAR);

	return S_OK;
}

void UninitDirectX()
{
        for (int i = 0; i < SAMPLER_MAX; ++i)
                SAFE_RELEASE(g_pSamplerState[i]);
        for (int i = 0; i < BLEND_MAX; ++i)
                SAFE_RELEASE(g_pBlendState[i]);
        for (int i = 0; i < 2; ++i)
                SAFE_RELEASE(g_pDepthStencilState[i]);
        for(int i = 0; i < 3; ++ i)
                SAFE_RELEASE(g_pRasterizerState[i]);
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
}



void SetRenderTargets(UINT num, RenderTarget** ppViews, DepthStencil* pView)
{
	static ID3D11RenderTargetView* rtvs[4];

	if (num > 4) num = 4;
	for (UINT i = 0; i < num; ++i)
		rtvs[i] = ppViews[i]->GetView();
	GetContext()->OMSetRenderTargets(num, rtvs, pView ? pView->GetView() : nullptr);

	// ビューポートの設定
	D3D11_VIEWPORT vp;
	vp.TopLeftX = 0.0f;
	vp.TopLeftY = 0.0f;
	vp.Width = (float)ppViews[0]->GetWidth();
	vp.Height = (float)ppViews[0]->GetHeight();
	vp.MinDepth = 0.0f;
	vp.MaxDepth = 1.0f;
	GetContext()->RSSetViewports(1, &vp);
}

void SetCullingMode(D3D11_CULL_MODE cull)
{
	switch (cull)
	{
	case D3D11_CULL_NONE: GetContext()->RSSetState(g_pRasterizerState[0]); break;
	case D3D11_CULL_FRONT: GetContext()->RSSetState(g_pRasterizerState[1]); break;
	case D3D11_CULL_BACK: GetContext()->RSSetState(g_pRasterizerState[2]); break;
	}
}
void SetDepthTest(bool enable)
{
	GetContext()->OMSetDepthStencilState(
		g_pDepthStencilState[enable ? 0 : 1], 1);
}
void SetBlendMode(BlendMode blend)
{
	if (blend < 0 || blend >= BLEND_MAX) return;
	FLOAT blendFactor[4] = { D3D11_BLEND_ZERO, D3D11_BLEND_ZERO, D3D11_BLEND_ZERO, D3D11_BLEND_ZERO };
	GetContext()->OMSetBlendState(g_pBlendState[blend], blendFactor, 0xffffffff);
}
void SetSamplerState(SamplerState state)
{
	if (state < 0 || state >= SAMPLER_MAX) return;
	GetContext()->PSSetSamplers(0, 1, &g_pSamplerState[state]);
}