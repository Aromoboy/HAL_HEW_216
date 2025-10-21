#include <windows.h>
#include <sstream>

#include "DirectX.h"

namespace
{
        template <typename T>
        void SafeRelease(T*& resource)
        {
                if (resource)
                {
                        resource->Release();
                        resource = nullptr;
                }
        }
}

ID3D11Device* g_pDevice = nullptr;
ID3D11DeviceContext* g_pContext = nullptr;
IDXGISwapChain* g_pSwapChain = nullptr;
ID3D11RenderTargetView* g_pRTV = nullptr;
ID3D11BlendState* g_pBlendState = nullptr;
ID3D11Texture2D* g_pDepthStencil = nullptr;
ID3D11DepthStencilView* g_pDSV = nullptr;
ID3D11DepthStencilState* g_pDepthState = nullptr;
HWND g_hWnd = nullptr;

HRESULT InitDirectX(HWND hwnd, UINT width, UINT height, BOOL fullscreen)
{
        g_hWnd = hwnd;

        HRESULT hr = S_OK;
        DXGI_SWAP_CHAIN_DESC sd;
        ZeroMemory(&sd, sizeof(sd));
        sd.BufferDesc.Width = width;
        sd.BufferDesc.Height = height;
        sd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
        sd.BufferDesc.RefreshRate.Numerator = 60;
        sd.BufferDesc.RefreshRate.Denominator = 1;
        sd.SampleDesc.Count = 1;
        sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
        sd.BufferCount = 1;
        sd.OutputWindow = hwnd;
        sd.Windowed = fullscreen ? FALSE : TRUE;
        sd.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;

        D3D_DRIVER_TYPE driverType = D3D_DRIVER_TYPE_NULL;
        D3D_DRIVER_TYPE driverTypes[] = {
                D3D_DRIVER_TYPE_HARDWARE,
                D3D_DRIVER_TYPE_WARP,
                D3D_DRIVER_TYPE_REFERENCE
        };
        UINT numDriverTypes = ARRAYSIZE(driverTypes);

        D3D_FEATURE_LEVEL featureLevel = D3D_FEATURE_LEVEL_11_0;
        D3D_FEATURE_LEVEL featureLevels[] = {
                D3D_FEATURE_LEVEL_11_1,
                D3D_FEATURE_LEVEL_11_0,
                D3D_FEATURE_LEVEL_10_1,
                D3D_FEATURE_LEVEL_10_0,
                D3D_FEATURE_LEVEL_9_3,
                D3D_FEATURE_LEVEL_9_2,
                D3D_FEATURE_LEVEL_9_1
        };
        UINT numFeatureLevels = ARRAYSIZE(featureLevels);

        UINT createDeviceFlags = 0;
#ifdef _DEBUG
        createDeviceFlags |= D3D11_CREATE_DEVICE_DEBUG;
#endif

        OutputDebugStringA("[DirectX] Starting device creation...\n");
        for (UINT i = 0; i < numDriverTypes; ++i)
        {
                driverType = driverTypes[i];
                hr = D3D11CreateDeviceAndSwapChain(
                        nullptr,
                        driverType,
                        nullptr,
                        createDeviceFlags,
                        featureLevels,
                        numFeatureLevels,
                        D3D11_SDK_VERSION,
                        &sd,
                        &g_pSwapChain,
                        &g_pDevice,
                        &featureLevel,
                        &g_pContext);
                if (SUCCEEDED(hr))
                {
                        std::ostringstream success;
                        success << "[DirectX] Device created with driver " << i << " and feature level 0x" << std::hex << featureLevel << "\n";
                        OutputDebugStringA(success.str().c_str());
                        break;
                }
        }
        if (FAILED(hr))
        {
                OutputDebugStringA("[DirectX] Device creation failed.\n");
                return hr;
        }

        ID3D11Texture2D* pBackBuffer = nullptr;
        hr = g_pSwapChain->GetBuffer(0, __uuidof(ID3D11Texture2D), reinterpret_cast<void**>(&pBackBuffer));
        if (FAILED(hr))
        {
                std::ostringstream oss;
                oss << "[DirectX] Failed to acquire back buffer. HRESULT=0x" << std::hex << hr << "\n";
                OutputDebugStringA(oss.str().c_str());
                MessageBox(hwnd, oss.str().c_str(), "DirectX Error", MB_OK | MB_ICONERROR);
                SafeRelease(g_pBlendState);
                SafeRelease(g_pRTV);
                if (g_pSwapChain)
                {
                        g_pSwapChain->SetFullscreenState(false, nullptr);
                }
                SafeRelease(g_pSwapChain);
                if (g_pContext)
                {
                        g_pContext->ClearState();
                }
                SafeRelease(g_pContext);
                SafeRelease(g_pDevice);
                return hr;
        }

        hr = g_pDevice->CreateRenderTargetView(pBackBuffer, nullptr, &g_pRTV);
        SafeRelease(pBackBuffer);
        if (FAILED(hr))
        {
                std::ostringstream oss;
                oss << "[DirectX] Failed to create render target view. HRESULT=0x" << std::hex << hr << "\n";
                OutputDebugStringA(oss.str().c_str());
                MessageBox(hwnd, oss.str().c_str(), "DirectX Error", MB_OK | MB_ICONERROR);
                SafeRelease(g_pBlendState);
                if (g_pSwapChain)
                {
                        g_pSwapChain->SetFullscreenState(false, nullptr);
                }
                SafeRelease(g_pSwapChain);
                if (g_pContext)
                {
                        g_pContext->ClearState();
                }
                SafeRelease(g_pContext);
                SafeRelease(g_pDevice);
                return hr;
        }

        D3D11_TEXTURE2D_DESC depthDesc = {};
        depthDesc.Width = width;
        depthDesc.Height = height;
        depthDesc.MipLevels = 1;
        depthDesc.ArraySize = 1;
        depthDesc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
        depthDesc.SampleDesc.Count = 1;
        depthDesc.BindFlags = D3D11_BIND_DEPTH_STENCIL;
        hr = g_pDevice->CreateTexture2D(&depthDesc, nullptr, &g_pDepthStencil);
        if (FAILED(hr))
        {
                std::ostringstream oss;
                oss << "[DirectX] Failed to create depth stencil buffer. HRESULT=0x" << std::hex << hr << "\n";
                OutputDebugStringA(oss.str().c_str());
                return hr;
        }

        hr = g_pDevice->CreateDepthStencilView(g_pDepthStencil, nullptr, &g_pDSV);
        if (FAILED(hr))
        {
                std::ostringstream oss;
                oss << "[DirectX] Failed to create depth stencil view. HRESULT=0x" << std::hex << hr << "\n";
                OutputDebugStringA(oss.str().c_str());
                return hr;
        }

        g_pContext->OMSetRenderTargets(1, &g_pRTV, g_pDSV);

        D3D11_VIEWPORT vp;
        vp.TopLeftX = 0.0f;
        vp.TopLeftY = 0.0f;
        vp.Width = static_cast<float>(width);
        vp.Height = static_cast<float>(height);
        vp.MinDepth = 0.0f;
        vp.MaxDepth = 1.0f;
        g_pContext->RSSetViewports(1, &vp);

        D3D11_BLEND_DESC blendDesc;
        ZeroMemory(&blendDesc, sizeof(blendDesc));
        blendDesc.RenderTarget[0].BlendEnable = true;
        blendDesc.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;
        blendDesc.RenderTarget[0].SrcBlend = D3D11_BLEND_SRC_ALPHA;
        blendDesc.RenderTarget[0].DestBlend = D3D11_BLEND_INV_SRC_ALPHA;
        blendDesc.RenderTarget[0].BlendOp = D3D11_BLEND_OP_ADD;
        blendDesc.RenderTarget[0].SrcBlendAlpha = D3D11_BLEND_ONE;
        blendDesc.RenderTarget[0].DestBlendAlpha = D3D11_BLEND_ZERO;
        blendDesc.RenderTarget[0].BlendOpAlpha = D3D11_BLEND_OP_ADD;
        hr = g_pDevice->CreateBlendState(&blendDesc, &g_pBlendState);
        if (FAILED(hr))
        {
                OutputDebugStringA("[DirectX] Failed to create blend state.\n");
                return hr;
        }

        float blendFactor[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
        g_pContext->OMSetBlendState(g_pBlendState, blendFactor, 0xffffffff);

        D3D11_DEPTH_STENCIL_DESC depthStateDesc = {};
        depthStateDesc.DepthEnable = TRUE;
        depthStateDesc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ALL;
        depthStateDesc.DepthFunc = D3D11_COMPARISON_LESS_EQUAL;
        depthStateDesc.StencilEnable = FALSE;
        hr = g_pDevice->CreateDepthStencilState(&depthStateDesc, &g_pDepthState);
        if (FAILED(hr))
        {
                OutputDebugStringA("[DirectX] Failed to create depth state.\n");
                return hr;
        }
        g_pContext->OMSetDepthStencilState(g_pDepthState, 0);

        OutputDebugStringA("[DirectX] Initialization complete.\n");
        return hr;
}

void UninitDirectX()
{
        SafeRelease(g_pDepthState);
        SafeRelease(g_pDSV);
        SafeRelease(g_pDepthStencil);
        SafeRelease(g_pBlendState);
        SafeRelease(g_pRTV);
        if (g_pSwapChain)
        {
                g_pSwapChain->SetFullscreenState(false, nullptr);
        }
        SafeRelease(g_pSwapChain);
        if (g_pContext)
        {
                g_pContext->ClearState();
        }
        SafeRelease(g_pContext);
        SafeRelease(g_pDevice);
        g_hWnd = nullptr;
}

void BeginDrawDirectX()
{
        float color[4] = { 0.8f, 0.9f, 1.0f, 1.0f };
        g_pContext->ClearRenderTargetView(g_pRTV, color);
        if (g_pDSV)
        {
                        g_pContext->ClearDepthStencilView(g_pDSV, D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, 1.0f, 0);
        }
}

void    EndDrawDirectX()
{
        if (g_pSwapChain)
        {
                g_pSwapChain->Present(0, 0);
        }
}

ID3D11Device* GetDevice()
{
        return g_pDevice;
}

ID3D11DeviceContext* GetContext()
{
        return g_pContext;
}

HWND GetGameWindowHandle()
{
        return g_hWnd;
}
