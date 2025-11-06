#include "DXContext.h"

#include <cstdio>

#pragma comment(lib, "d3d11.lib")

namespace
{
        constexpr UINT kBackBufferIndex = 0u;
}

namespace Core
{

DXContext::~DXContext()
{
        Shutdown();
}

HRESULT DXContext::Init(HWND hwnd, UINT width, UINT height, BOOL fullscreen)
{
        HRESULT hr = CreateDeviceAndSwapChain(hwnd, width, height, fullscreen);
        if (FAILED(hr))
        {
                LogFailure("D3D11CreateDeviceAndSwapChain", hr);
                Shutdown();
                return hr;
        }

        hr = CreateRenderTarget(width, height);
        if (FAILED(hr))
        {
                LogFailure("CreateRenderTargetView", hr);
                Shutdown();
                return hr;
        }

        hr = ConfigureBlendState();
        if (FAILED(hr))
        {
                Shutdown();
                return hr;
        }
        UpdateViewport(width, height);

        return S_OK;
}

HRESULT DXContext::Resize(UINT width, UINT height)
{
        if (!m_swapChain || !m_context)
        {
                return E_FAIL;
        }

        m_context->OMSetRenderTargets(0, nullptr, nullptr);
        m_renderTarget.Reset();

        HRESULT hr = m_swapChain->ResizeBuffers(0, width, height, DXGI_FORMAT_UNKNOWN, 0);
        if (FAILED(hr))
        {
                LogFailure("IDXGISwapChain::ResizeBuffers", hr);
                return hr;
        }

        hr = CreateRenderTarget(width, height);
        if (FAILED(hr))
        {
                LogFailure("CreateRenderTargetView", hr);
                return hr;
        }

        UpdateViewport(width, height);
        return S_OK;
}

void DXContext::BeginDraw()
{
        BeginDraw(m_defaultClearColor);
}

void DXContext::BeginDraw(const FLOAT clearColor[4])
{
        if (!m_context || !m_renderTarget)
        {
                return;
        }

        m_context->OMSetRenderTargets(1, m_renderTarget.GetAddressOf(), nullptr);
        m_context->ClearRenderTargetView(m_renderTarget.Get(), clearColor);
}

void DXContext::EndDraw()
{
        if (m_swapChain)
        {
                m_swapChain->Present(0, 0);
        }
}

void DXContext::Shutdown()
{
        if (m_swapChain)
        {
                m_swapChain->SetFullscreenState(FALSE, nullptr);
        }

        if (m_context)
        {
                m_context->ClearState();
        }

        m_blendState.Reset();
        m_renderTarget.Reset();
        m_swapChain.Reset();
        m_context.Reset();
        m_device.Reset();
}

HRESULT DXContext::CreateDeviceAndSwapChain(HWND hwnd, UINT width, UINT height, BOOL fullscreen)
{
        DXGI_SWAP_CHAIN_DESC swapDesc = {};
        swapDesc.BufferDesc.Width = width;
        swapDesc.BufferDesc.Height = height;
        swapDesc.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
        swapDesc.BufferDesc.RefreshRate.Numerator = 60;
        swapDesc.BufferDesc.RefreshRate.Denominator = 1;
        swapDesc.SampleDesc.Count = 1;
        swapDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
        swapDesc.BufferCount = 1;
        swapDesc.OutputWindow = hwnd;
        swapDesc.Windowed = fullscreen ? FALSE : TRUE;
        swapDesc.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;

        const D3D_DRIVER_TYPE driverTypes[] = {
                D3D_DRIVER_TYPE_HARDWARE,
                D3D_DRIVER_TYPE_WARP,
                D3D_DRIVER_TYPE_REFERENCE,
        };

        const D3D_FEATURE_LEVEL featureLevels[] = {
                D3D_FEATURE_LEVEL_11_1,
                D3D_FEATURE_LEVEL_11_0,
                D3D_FEATURE_LEVEL_10_1,
                D3D_FEATURE_LEVEL_10_0,
                D3D_FEATURE_LEVEL_9_3,
                D3D_FEATURE_LEVEL_9_2,
                D3D_FEATURE_LEVEL_9_1,
        };

        UINT createDeviceFlags = 0;
#ifdef _DEBUG
        // createDeviceFlags |= D3D11_CREATE_DEVICE_DEBUG;
#endif

        HRESULT hr = E_FAIL;
        for (D3D_DRIVER_TYPE driverType : driverTypes)
        {
                D3D_FEATURE_LEVEL obtainedLevel = {};
                hr = D3D11CreateDeviceAndSwapChain(
                        nullptr,
                        driverType,
                        nullptr,
                        createDeviceFlags,
                        featureLevels,
                        ARRAYSIZE(featureLevels),
                        D3D11_SDK_VERSION,
                        &swapDesc,
                        m_swapChain.ReleaseAndGetAddressOf(),
                        m_device.ReleaseAndGetAddressOf(),
                        &obtainedLevel,
                        m_context.ReleaseAndGetAddressOf());

                if (SUCCEEDED(hr))
                {
                        break;
                }
        }

        if (FAILED(hr))
        {
                return hr;
        }

        return S_OK;
}

HRESULT DXContext::CreateRenderTarget(UINT width, UINT height)
{
        if (!m_device || !m_swapChain)
        {
                return E_FAIL;
        }

        Microsoft::WRL::ComPtr<ID3D11Texture2D> backBuffer;
        HRESULT hr = m_swapChain->GetBuffer(kBackBufferIndex, __uuidof(ID3D11Texture2D), reinterpret_cast<void**>(backBuffer.GetAddressOf()));
        if (FAILED(hr))
        {
                LogFailure("IDXGISwapChain::GetBuffer", hr);
                return hr;
        }

        hr = m_device->CreateRenderTargetView(backBuffer.Get(), nullptr, m_renderTarget.ReleaseAndGetAddressOf());
        if (FAILED(hr))
        {
                LogFailure("ID3D11Device::CreateRenderTargetView", hr);
                return hr;
        }

        m_context->OMSetRenderTargets(1, m_renderTarget.GetAddressOf(), nullptr);
        return S_OK;
}

HRESULT DXContext::ConfigureBlendState()
{
        if (!m_device || !m_context)
        {
                LogFailure("ConfigureBlendState", E_FAIL);
                return E_FAIL;
        }

        D3D11_BLEND_DESC blendDesc = {};
        blendDesc.RenderTarget[0].BlendEnable = TRUE;
        blendDesc.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;
        blendDesc.RenderTarget[0].SrcBlend = D3D11_BLEND_SRC_ALPHA;
        blendDesc.RenderTarget[0].DestBlend = D3D11_BLEND_INV_SRC_ALPHA;
        blendDesc.RenderTarget[0].BlendOp = D3D11_BLEND_OP_ADD;
        blendDesc.RenderTarget[0].SrcBlendAlpha = D3D11_BLEND_ONE;
        blendDesc.RenderTarget[0].DestBlendAlpha = D3D11_BLEND_ZERO;
        blendDesc.RenderTarget[0].BlendOpAlpha = D3D11_BLEND_OP_ADD;

        HRESULT hr = m_device->CreateBlendState(&blendDesc, m_blendState.ReleaseAndGetAddressOf());
        if (FAILED(hr))
        {
                LogFailure("CreateBlendState", hr);
                return hr;
        }

        const FLOAT blendFactor[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
        m_context->OMSetBlendState(m_blendState.Get(), blendFactor, 0xffffffff);
        return S_OK;
}

void DXContext::LogFailure(const char* stage, HRESULT hr) const
{
        if (!stage)
        {
                stage = "Unknown";
        }

        char systemMessage[512] = {};
        DWORD length = FormatMessageA(
                FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
                nullptr,
                hr,
                MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
                systemMessage,
                static_cast<DWORD>(sizeof(systemMessage) / sizeof(systemMessage[0])),
                nullptr);

        char buffer[1024];
        if (length > 0)
        {
                std::snprintf(buffer, std::size(buffer), "[DXContext] %s failed (0x%08lX): %s\n", stage, hr, systemMessage);
        }
        else
        {
                std::snprintf(buffer, std::size(buffer), "[DXContext] %s failed (0x%08lX)\n", stage, hr);
        }

        OutputDebugStringA(buffer);
}

void DXContext::UpdateViewport(UINT width, UINT height)
{
        if (!m_context)
        {
                return;
        }

        m_viewport.TopLeftX = 0.0f;
        m_viewport.TopLeftY = 0.0f;
        m_viewport.Width = static_cast<float>(width);
        m_viewport.Height = static_cast<float>(height);
        m_viewport.MinDepth = 0.0f;
        m_viewport.MaxDepth = 1.0f;

        m_context->RSSetViewports(1, &m_viewport);
}

} // namespace Core

