#include "DirectX.h"
#include "Texture.h"
#include "AppGame.h"

namespace
{
	ID3D11RasterizerState*		g_pRasterizerState[3];
	ID3D11DepthStencilState*	g_pDepthStencilState[3];
	ID3D11BlendState*			g_pBlendState[BLEND_MAX];
	ID3D11SamplerState*			g_pSamplerState[SAMPLER_MAX];
	ID3D11Buffer*			g_pPostProcessParamBuffer = nullptr;
}

namespace
{
	Core::DXContext& GetDXContext()
	{
		return App::Game::Instance().GetDXContext();
	}
}

ID3D11Device* GetDevice()
{
	return GetDXContext().GetDevice();
}
ID3D11DeviceContext* GetContext()
{
	return GetDXContext().GetContext();
}
IDXGISwapChain* GetSwapChain()
{
	return GetDXContext().GetSwapChain();
}
RenderTarget* GetDefaultRTV()
{
	return GetDXContext().GetDefaultRTV();
}
DepthStencil* GetDefaultDSV()
{
        return GetDXContext().GetDefaultDSV();
}

bool IsDirectXReady()
{
        return GetDXContext().IsReady();
}


HRESULT InitDirectX(HWND hWnd, UINT width, UINT height, bool fullscreen)
{
        HRESULT hr = GetDXContext().Init(hWnd, width, height, fullscreen);
        if (FAILED(hr))
        {
                return hr;
        }

        RenderTarget* defaultRTV = GetDXContext().GetDefaultRTV();
        SetRenderTargets(1, &defaultRTV, GetDXContext().GetDefaultDSV());
        
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
	D3D11_DEPTH_STENCIL_DESC dsDesc = {};
	// 深度テストの設定
	dsDesc.DepthEnable = true;
	dsDesc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ALL;
	dsDesc.DepthFunc = D3D11_COMPARISON_LESS;
	// ステンシルテストの設定
	dsDesc.StencilEnable = true;
	dsDesc.StencilReadMask = 0xFF;
	dsDesc.StencilWriteMask = 0xFF;
	// ステンシル操作
	dsDesc.FrontFace.StencilFailOp = D3D11_STENCIL_OP_KEEP;
	dsDesc.FrontFace.StencilDepthFailOp = D3D11_STENCIL_OP_INCR;
	dsDesc.FrontFace.StencilPassOp = D3D11_STENCIL_OP_KEEP;
	dsDesc.FrontFace.StencilFunc = D3D11_COMPARISON_ALWAYS;
	dsDesc.BackFace = dsDesc.FrontFace;
	dsDesc.BackFace.StencilFailOp = D3D11_STENCIL_OP_DECR;
	// ステート作成
	GetDevice()->CreateDepthStencilState(&dsDesc, &g_pDepthStencilState[0]);
	dsDesc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ZERO;
	GetDevice()->CreateDepthStencilState(&dsDesc, &g_pDepthStencilState[2]);
	dsDesc.DepthEnable = false;
	GetDevice()->CreateDepthStencilState(&dsDesc, &g_pDepthStencilState[1]);
	SetDepthTest(false);

	//--- アルファブレンディング
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
        for (int i = 0; i < 3; ++i)
                SAFE_RELEASE(g_pDepthStencilState[i]);
        for(int i = 0; i < 3; ++ i)
                SAFE_RELEASE(g_pRasterizerState[i]);
        SAFE_RELEASE(g_pPostProcessParamBuffer);
        GetDXContext().Shutdown();
}

void BeginDrawDirectX()
{
        const float srgbClearColor[4] = { 0.8f, 0.9f, 1.0f, 1.0f };
        GetDXContext().BeginDraw(srgbClearColor);
}
void EndDrawDirectX()
{
        GetDXContext().EndDraw();
}

HRESULT ResizeDirectX(UINT width, UINT height)
{
        return GetDXContext().Resize(width, height);
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
void SetDepthTestWrite(bool enable, bool write)
{
	if (!enable)
	{
		GetContext()->OMSetDepthStencilState(g_pDepthStencilState[1], 1);
		return;
	}

	GetContext()->OMSetDepthStencilState(g_pDepthStencilState[write ? 0 : 2], 1);
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

void BindPostProcessParams(float gamma, float saturation, float contrast, const DirectX::XMFLOAT4& fogColor, float fogStart, float fogRange, float cameraNear, float cameraFar, float vignetteIntensity, float microBloomIntensity, float bloomThreshold, const DirectX::XMFLOAT2& invScreenSize, float fxaaEdgeThreshold, float fxaaEdgeThresholdMin, float fxaaSubpixelQuality, int fxaaEnable)
{
	ID3D11Device* device = GetDevice();
	ID3D11DeviceContext* context = GetContext();
	if (!device || !context)
	{
		return;
	}

	if (!g_pPostProcessParamBuffer)
	{
		D3D11_BUFFER_DESC desc = {};
		desc.ByteWidth = sizeof(PostProcessParams);
		desc.Usage = D3D11_USAGE_DEFAULT;
		desc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
		HRESULT hr = device->CreateBuffer(&desc, nullptr, &g_pPostProcessParamBuffer);
		if (FAILED(hr))
		{
			return;
		}
	}

	const PostProcessParams params = {
		gamma,
		saturation,
		contrast,
		0.0f,
		fogColor,
		fogStart,
		fogRange,
		cameraNear,
		cameraFar,
		vignetteIntensity,
		microBloomIntensity,
		bloomThreshold,
		0.0f,
		invScreenSize,
		fxaaEdgeThreshold,
		fxaaEdgeThresholdMin,
		fxaaSubpixelQuality,
		fxaaEnable,
		{ 0.0f, 0.0f }
	};
	context->UpdateSubresource(g_pPostProcessParamBuffer, 0, nullptr, &params, 0, 0);
	context->PSSetConstantBuffers(0, 1, &g_pPostProcessParamBuffer);
}
