#ifndef __DIRECTX_H__
#define __DIRECTX_H__

#include <d3d11.h>

#pragma comment(lib, "d3d11.lib")

#define SAFE_DELETE(p)			do{if(p){delete p; p = nullptr;}}while(0)
#define SAFE_DELETE_ARRAY(p)	do{if(p){delete[] p; p = nullptr;}}while(0)
#define SAFE_RELEASE(p)			do{if(p){p->Release(); p = nullptr;}}while(0)

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

void SetRenderTargets(UINT num, RenderTarget** ppViews, DepthStencil* pView);
void SetCullingMode(D3D11_CULL_MODE cull);
void SetDepthTest(bool enable);
void SetBlendMode(BlendMode blend);
void SetSamplerState(SamplerState state);

#endif