#ifndef __DIRECTX_H__
#define __DIRECTX_H__

#include <d3d11.h>
#include <DirectXMath.h>

#pragma comment(lib, "d3d11.lib")

#define SAFE_DELETE(p)                  do{if(p){delete p; p = nullptr;}}while(0)
#define SAFE_DELETE_ARRAY(p)    do{if(p){delete[] p; p = nullptr;}}while(0)
#define SAFE_RELEASE(p)                 do{if(p){p->Release(); p = nullptr;}}while(0)

class RenderTarget;
class DepthStencil;

enum BlendMode
{
        BLEND_NONE,
        BLEND_ALPHA,
        BLEND_ADD,
        BLEND_ADDALPHA,
        BLEND_SUB,
        BLEND_SCREEN,
        BLEND_MAX
};

enum SamplerState
{
        SAMPLER_LINEAR,
        SAMPLER_POINT,
        SAMPLER_POSTPROCESS,
        SAMPLER_MAX
};

ID3D11Device* GetDevice();
ID3D11DeviceContext* GetContext();
IDXGISwapChain* GetSwapChain();
RenderTarget* GetDefaultRTV();
DepthStencil* GetDefaultDSV();

HRESULT InitDirectX(HWND hWnd, UINT width, UINT height, bool fullscreen);
void UninitDirectX();
void BeginDrawDirectX();
void EndDrawDirectX();
HRESULT ResizeDirectX(UINT width, UINT height);
bool IsDirectXReady();

void SetRenderTargets(UINT num, RenderTarget** ppViews, DepthStencil* pView);
void SetCullingMode(D3D11_CULL_MODE cull);
void SetDepthTest(bool enable);
void SetDepthTestWrite(bool enable, bool write);
void SetBlendMode(BlendMode blend);
void SetSamplerState(SamplerState state);

struct PostProcessParams
{
        float gamma;
        float saturation;
        float contrast;
        float padding;
        DirectX::XMFLOAT4 fogColor;
        float fogStart;
        float fogRange;
        float cameraNear;
        float cameraFar;
        float vignetteIntensity;
        float microBloomIntensity;
        float bloomThreshold;
        float padding2;
        DirectX::XMFLOAT2 invScreenSize;
	float fxaaEdgeThreshold;
	float fxaaEdgeThresholdMin;
	float fxaaSubpixelQuality;
	int fxaaEnable;
	float postTime;
	float grainAmount;
	int grainEnable;
	int grainAnimate;
};

void BindPostProcessParams(
        float gamma = 2.2f,
        float saturation = 0.85f,
        float contrast = 1.1f,
        const DirectX::XMFLOAT4& fogColor = DirectX::XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f),
        float fogStart = 50.0f,
        float fogRange = 50.0f,
        float cameraNear = 0.1f,
        float cameraFar = 1000.0f,
        float vignetteIntensity = 1.2f,
        float microBloomIntensity = 0.15f,
        float bloomThreshold = 0.8f,
        const DirectX::XMFLOAT2& invScreenSize = DirectX::XMFLOAT2(1.0f, 1.0f),
	float fxaaEdgeThreshold = 0.166f,
	float fxaaEdgeThresholdMin = 0.0833f,
	float fxaaSubpixelQuality = 0.75f,
	int fxaaEnable = 0,
	float postTime = 0.0f,
	float grainAmount = 0.0f,
	int grainEnable = 0,
	int grainAnimate = 0);

#endif
