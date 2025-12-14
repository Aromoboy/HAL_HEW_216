#include "Core/DXContext.h"

#include <array>

#include "Debug.h"
#include "DirectX.h"
#include "Texture.h"

namespace
{
        constexpr float kDefaultClearColor[4] = { 0.8f, 0.9f, 1.0f, 1.0f };
}

namespace Core
{
        DXContext::DXContext()
                : m_windowHandle(nullptr)
                , m_width(0)
                , m_height(0)
        {
        }

        DXContext::~DXContext()
        {
                Shutdown();
        }

        HRESULT DXContext::Init(HWND hWnd, UINT width, UINT height, bool fullscreen)
        {
                m_windowHandle = hWnd;
                m_width = width;
                m_height = height;

                HRESULT hr = CreateDeviceAndSwapChain(hWnd, width, height, fullscreen);
                if (FAILED(hr))
                {
                        DebugMessage("[DXContext] Failed to create device and swap chain.\n");
                        return hr;
                }

                hr = CreateDefaultTargets();
                if (FAILED(hr))
                {
                        DebugMessage("[DXContext] Failed to create default render targets.\n");
                        return hr;
                }

                return S_OK;
        }

        void DXContext::BeginDraw(const float clearColor[4])
        {
                if (m_defaultRTV)
                {
                        m_defaultRTV->Clear(clearColor ? clearColor : kDefaultClearColor);
                }
                if (m_defaultDSV)
                {
                        m_defaultDSV->Clear();
                }
        }

        void DXContext::EndDraw()
        {
                if (m_swapChain)
                {
                        m_swapChain->Present(0, 0);
                }
        }

        HRESULT DXContext::Resize(UINT width, UINT height)
        {
                if (width == 0 || height == 0)
                {
                        return S_OK;
                }

                if (!m_swapChain)
                {
                        DebugMessage("[DXContext] Resize requested before swap chain creation.\n");
                        return E_FAIL;
                }

                if (m_context)
                {
                        DebugMessage("[DXContext] Clearing render targets and shader resources prior to resize.\n");
                        m_context->OMSetRenderTargets(0, nullptr, nullptr);

                        ID3D11ShaderResourceView* nullSrvs[D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT] = {};
                        m_context->VSSetShaderResources(0, D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT, nullSrvs);
                        m_context->PSSetShaderResources(0, D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT, nullSrvs);
                        m_context->GSSetShaderResources(0, D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT, nullSrvs);
                        m_context->HSSetShaderResources(0, D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT, nullSrvs);
                        m_context->DSSetShaderResources(0, D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT, nullSrvs);
                        m_context->CSSetShaderResources(0, D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT, nullSrvs);
                }

                ReleaseTargets();

                HRESULT hr = m_swapChain->ResizeBuffers(0, width, height, DXGI_FORMAT_UNKNOWN, 0);
                if (FAILED(hr))
                {
                        DebugMessage("[DXContext] ResizeBuffers failed.\n");
                        return hr;
                }

                m_width = width;
                m_height = height;

                hr = CreateDefaultTargets();
                if (FAILED(hr))
                {
                        DebugMessage("[DXContext] Failed to recreate default render targets after resize.\n");
                        return hr;
                }

                RenderTarget* defaultRTV = GetDefaultRTV();
                SetRenderTargets(1, &defaultRTV, GetDefaultDSV());

                return S_OK;
        }

        void DXContext::Shutdown()
        {
                ReleaseTargets();
                if (m_context)
                {
                        m_context->ClearState();
                }
                if (m_swapChain)
                {
                        m_swapChain->SetFullscreenState(false, NULL);
                }
                m_swapChain.Reset();
                m_context.Reset();
                m_device.Reset();
        }

        ID3D11Device* DXContext::GetDevice() const
        {
                return m_device.Get();
        }

        ID3D11DeviceContext* DXContext::GetContext() const
        {
                return m_context.Get();
        }

        IDXGISwapChain* DXContext::GetSwapChain() const
        {
                return m_swapChain.Get();
        }

        RenderTarget* DXContext::GetDefaultRTV() const
        {
                return m_defaultRTV.get();
        }

        DepthStencil* DXContext::GetDefaultDSV() const
        {
                return m_defaultDSV.get();
        }

        HRESULT DXContext::CreateDeviceAndSwapChain(HWND hWnd, UINT width, UINT height, bool fullscreen)
        {
                DXGI_SWAP_CHAIN_DESC sd = {};
                sd.BufferDesc.Width = width;
                sd.BufferDesc.Height = height;
                sd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
                sd.SampleDesc.Count = 1;
                sd.BufferDesc.RefreshRate.Numerator = 1000;
                sd.BufferDesc.RefreshRate.Denominator = 1;
                sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
                sd.BufferCount = 1;
                sd.OutputWindow = hWnd;
                sd.Windowed = fullscreen ? FALSE : TRUE;
                sd.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;

                constexpr std::array<D3D_DRIVER_TYPE, 3> driverTypes = {
                        D3D_DRIVER_TYPE_HARDWARE,
                        D3D_DRIVER_TYPE_WARP,
                        D3D_DRIVER_TYPE_REFERENCE,
                };

                constexpr std::array<D3D_FEATURE_LEVEL, 7> featureLevels = {
                        D3D_FEATURE_LEVEL_11_1,
                        D3D_FEATURE_LEVEL_11_0,
                        D3D_FEATURE_LEVEL_10_1,
                        D3D_FEATURE_LEVEL_10_0,
                        D3D_FEATURE_LEVEL_9_3,
                        D3D_FEATURE_LEVEL_9_2,
                        D3D_FEATURE_LEVEL_9_1,
                };

                UINT createDeviceFlags = 0;
                HRESULT hr = E_FAIL;

                for (size_t driverTypeIndex = 0; driverTypeIndex < driverTypes.size(); ++driverTypeIndex)
                {
                        hr = D3D11CreateDeviceAndSwapChain(
                                nullptr,
                                driverTypes[driverTypeIndex],
                                nullptr,
                                createDeviceFlags,
                                featureLevels.data(),
                                static_cast<UINT>(featureLevels.size()),
                                D3D11_SDK_VERSION,
                                &sd,
                                m_swapChain.GetAddressOf(),
                                m_device.GetAddressOf(),
                                nullptr,
                                m_context.GetAddressOf());
                        if (SUCCEEDED(hr))
                        {
                                break;
                        }
                }

                if (FAILED(hr))
                {
                        DebugMessage("[DXContext] D3D11CreateDeviceAndSwapChain failed for all driver types.\n");
                }

                return hr;
        }

        HRESULT DXContext::CreateDefaultTargets()
        {
                ReleaseTargets();

                m_defaultRTV = std::make_unique<RenderTarget>();
                HRESULT hr = m_defaultRTV->CreateFromScreen();
                if (FAILED(hr))
                {
                        DebugMessage("[DXContext] Failed to create default render target from swap chain.\n");
                        m_defaultRTV.reset();
                        return hr;
                }

                m_defaultDSV = std::make_unique<DepthStencil>();
                hr = m_defaultDSV->Create(m_defaultRTV->GetWidth(), m_defaultRTV->GetHeight(), false);
                if (FAILED(hr))
                {
                        DebugMessage("[DXContext] Failed to create default depth stencil.\n");
                        m_defaultDSV.reset();
                        return hr;
                }

                return S_OK;
        }

        void DXContext::ReleaseTargets()
        {
                m_defaultDSV.reset();
                m_defaultRTV.reset();
        }
}

