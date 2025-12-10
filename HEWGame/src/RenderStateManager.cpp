#include "RenderStateManager.h"

#include <cassert>

#include "Debug.h"

namespace
{
    D3D11_CULL_MODE ClampCullMode(D3D11_CULL_MODE mode)
    {
        switch (mode)
        {
        case D3D11_CULL_FRONT:
        case D3D11_CULL_NONE:
        case D3D11_CULL_BACK:
            return mode;
        default:
            return D3D11_CULL_BACK;
        }
    }

    RenderStateManager::Blend ToBlend(BlendMode legacy)
    {
        switch (legacy)
        {
        case BlendMode::BLEND_NONE:
            return RenderStateManager::Blend::Opaque;
        default:
            return RenderStateManager::Blend::Alpha;
        }
    }

    RenderStateManager::Sampler ToSampler(SamplerState legacy)
    {
        switch (legacy)
        {
        case SamplerState::SAMPLER_POINT:
            return RenderStateManager::Sampler::Point;
        default:
            return RenderStateManager::Sampler::Linear;
        }
    }
}

RenderStateManager& RenderStateManager::Instance()
{
    static RenderStateManager instance;
    return instance;
}

RenderStateManager::ScopedState::ScopedState()
    : m_owner(RenderStateManager::Instance())
{
    m_owner.PushState();
}

RenderStateManager::ScopedState::~ScopedState()
{
    m_owner.PopState();
}

void RenderStateManager::Initialize(ID3D11Device* device)
{
    EnsureDepthStates(device);
    EnsureBlendStates(device);
    EnsureRasterizerStates(device);
    EnsureSamplerStates(device);
    RunBalanceCheck();
}

void RenderStateManager::Shutdown()
{
    m_depthEnabled.Reset();
    m_depthDisabled.Reset();
    m_blendOpaque.Reset();
    m_blendAlpha.Reset();
    m_rasterizerCullNone.Reset();
    m_rasterizerCullFront.Reset();
    m_rasterizerCullBack.Reset();
    m_samplerLinear.Reset();
    m_samplerPoint.Reset();
    m_stateStack.clear();
}

void RenderStateManager::PushState()
{
    StateSnapshot snapshot{};
    if (auto* context = GetContext())
    {
        context->OMGetDepthStencilState(snapshot.depth.GetAddressOf(), &snapshot.stencilRef);
        context->OMGetBlendState(snapshot.blend.GetAddressOf(), snapshot.blendFactor, &snapshot.sampleMask);
        context->RSGetState(snapshot.rasterizer.GetAddressOf());
        context->PSGetSamplers(0, 1, snapshot.sampler.GetAddressOf());
    }
    m_stateStack.emplace_back(std::move(snapshot));
}

void RenderStateManager::PopState()
{
    if (m_stateStack.empty())
    {
        return;
    }

    StateSnapshot snapshot = std::move(m_stateStack.back());
    m_stateStack.pop_back();

    if (auto* context = GetContext())
    {
        if (snapshot.blend)
        {
            context->OMSetBlendState(snapshot.blend.Get(), snapshot.blendFactor, snapshot.sampleMask);
        }

        if (snapshot.depth)
        {
            context->OMSetDepthStencilState(snapshot.depth.Get(), snapshot.stencilRef);
        }

        if (snapshot.rasterizer)
        {
            context->RSSetState(snapshot.rasterizer.Get());
        }

        if (snapshot.sampler)
        {
            ID3D11SamplerState* sampler = snapshot.sampler.Get();
            context->PSSetSamplers(0, 1, &sampler);
        }
    }
}

void RenderStateManager::SetDepthTest(bool enabled)
{
    EnsureDepthStates(GetDevice());
    if (auto* context = GetContext())
    {
        context->OMSetDepthStencilState(enabled ? m_depthEnabled.Get() : m_depthDisabled.Get(), 1);
    }
}

void RenderStateManager::SetBlend(Blend blendMode)
{
    EnsureBlendStates(GetDevice());
    if (auto* context = GetContext())
    {
        const FLOAT blendFactor[4] = { 0.f, 0.f, 0.f, 0.f };
        ID3D11BlendState* target = blendMode == Blend::Alpha ? m_blendAlpha.Get() : m_blendOpaque.Get();
        context->OMSetBlendState(target, blendFactor, 0xffffffff);
    }
}

void RenderStateManager::SetBlend(BlendMode legacyBlend)
{
    SetBlend(ToBlend(legacyBlend));
}

void RenderStateManager::SetRasterizer(D3D11_CULL_MODE cullMode)
{
    EnsureRasterizerStates(GetDevice());
    if (auto* context = GetContext())
    {
        switch (ClampCullMode(cullMode))
        {
        case D3D11_CULL_FRONT:
            context->RSSetState(m_rasterizerCullFront.Get());
            break;
        case D3D11_CULL_NONE:
            context->RSSetState(m_rasterizerCullNone.Get());
            break;
        case D3D11_CULL_BACK:
        default:
            context->RSSetState(m_rasterizerCullBack.Get());
            break;
        }
    }
}

void RenderStateManager::SetSampler(Sampler sampler)
{
    EnsureSamplerStates(GetDevice());
    if (auto* context = GetContext())
    {
        ID3D11SamplerState* target = sampler == Sampler::Point ? m_samplerPoint.Get() : m_samplerLinear.Get();
        context->PSSetSamplers(0, 1, &target);
    }
}

void RenderStateManager::SetSampler(SamplerState legacySampler)
{
    SetSampler(ToSampler(legacySampler));
}

bool RenderStateManager::IsStateStackBalanced() const
{
    return m_stateStack.empty();
}

void RenderStateManager::EnsureDepthStates(ID3D11Device* device)
{
    if (m_depthEnabled && m_depthDisabled)
    {
        return;
    }
    if (!device)
    {
        return;
    }

    D3D11_DEPTH_STENCIL_DESC dsDesc{};
    dsDesc.DepthEnable = true;
    dsDesc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ALL;
    dsDesc.DepthFunc = D3D11_COMPARISON_LESS;
    dsDesc.StencilEnable = true;
    dsDesc.StencilReadMask = 0xFF;
    dsDesc.StencilWriteMask = 0xFF;
    dsDesc.FrontFace.StencilFailOp = D3D11_STENCIL_OP_KEEP;
    dsDesc.FrontFace.StencilDepthFailOp = D3D11_STENCIL_OP_INCR;
    dsDesc.FrontFace.StencilPassOp = D3D11_STENCIL_OP_KEEP;
    dsDesc.FrontFace.StencilFunc = D3D11_COMPARISON_ALWAYS;
    dsDesc.BackFace = dsDesc.FrontFace;
    dsDesc.BackFace.StencilFailOp = D3D11_STENCIL_OP_DECR;
    device->CreateDepthStencilState(&dsDesc, m_depthEnabled.ReleaseAndGetAddressOf());

    dsDesc.DepthEnable = FALSE;
    device->CreateDepthStencilState(&dsDesc, m_depthDisabled.ReleaseAndGetAddressOf());
}

void RenderStateManager::EnsureBlendStates(ID3D11Device* device)
{
    if (m_blendAlpha && m_blendOpaque)
    {
        return;
    }
    if (!device)
    {
        return;
    }

    D3D11_BLEND_DESC blendDesc{};
    blendDesc.AlphaToCoverageEnable = FALSE;
    blendDesc.IndependentBlendEnable = FALSE;
    blendDesc.RenderTarget[0].BlendEnable = FALSE;
    blendDesc.RenderTarget[0].BlendOp = D3D11_BLEND_OP_ADD;
    blendDesc.RenderTarget[0].BlendOpAlpha = D3D11_BLEND_OP_ADD;
    blendDesc.RenderTarget[0].SrcBlendAlpha = D3D11_BLEND_ONE;
    blendDesc.RenderTarget[0].DestBlendAlpha = D3D11_BLEND_ZERO;
    blendDesc.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;

    blendDesc.RenderTarget[0].SrcBlend = D3D11_BLEND_ONE;
    blendDesc.RenderTarget[0].DestBlend = D3D11_BLEND_ZERO;
    device->CreateBlendState(&blendDesc, m_blendOpaque.ReleaseAndGetAddressOf());

    blendDesc.RenderTarget[0].BlendEnable = TRUE;
    blendDesc.RenderTarget[0].SrcBlend = D3D11_BLEND_SRC_ALPHA;
    blendDesc.RenderTarget[0].DestBlend = D3D11_BLEND_INV_SRC_ALPHA;
    device->CreateBlendState(&blendDesc, m_blendAlpha.ReleaseAndGetAddressOf());
}

void RenderStateManager::EnsureRasterizerStates(ID3D11Device* device)
{
    if (m_rasterizerCullBack && m_rasterizerCullFront && m_rasterizerCullNone)
    {
        return;
    }
    if (!device)
    {
        return;
    }

    D3D11_RASTERIZER_DESC rasterizer{};
    rasterizer.FillMode = D3D11_FILL_SOLID;
    rasterizer.FrontCounterClockwise = FALSE;
    rasterizer.DepthClipEnable = TRUE;

    rasterizer.CullMode = D3D11_CULL_BACK;
    device->CreateRasterizerState(&rasterizer, m_rasterizerCullBack.ReleaseAndGetAddressOf());

    rasterizer.CullMode = D3D11_CULL_FRONT;
    device->CreateRasterizerState(&rasterizer, m_rasterizerCullFront.ReleaseAndGetAddressOf());

    rasterizer.CullMode = D3D11_CULL_NONE;
    device->CreateRasterizerState(&rasterizer, m_rasterizerCullNone.ReleaseAndGetAddressOf());
}

void RenderStateManager::EnsureSamplerStates(ID3D11Device* device)
{
    if (m_samplerLinear && m_samplerPoint)
    {
        return;
    }
    if (!device)
    {
        return;
    }

    D3D11_SAMPLER_DESC sampler{};
    sampler.AddressU = D3D11_TEXTURE_ADDRESS_WRAP;
    sampler.AddressV = D3D11_TEXTURE_ADDRESS_WRAP;
    sampler.AddressW = D3D11_TEXTURE_ADDRESS_WRAP;
    sampler.ComparisonFunc = D3D11_COMPARISON_ALWAYS;
    sampler.MaxLOD = D3D11_FLOAT32_MAX;

    sampler.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
    device->CreateSamplerState(&sampler, m_samplerLinear.ReleaseAndGetAddressOf());

    sampler.Filter = D3D11_FILTER_MIN_MAG_MIP_POINT;
    device->CreateSamplerState(&sampler, m_samplerPoint.ReleaseAndGetAddressOf());
}

void RenderStateManager::RunBalanceCheck()
{
#if defined(_DEBUG)
    const size_t before = m_stateStack.size();
    {
        ScopedState guard;
    }
    if (m_stateStack.size() != before)
    {
        DebugMessage("[RenderStateManager] State stack leaked during balance check.\n");
        assert(false);
    }
#endif
}

