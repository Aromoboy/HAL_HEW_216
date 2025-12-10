#pragma once

#include <vector>
#include <wrl/client.h>

#include "DirectX.h"

class RenderStateManager
{
public:
    enum class Blend
    {
        Opaque,
        Alpha,
    };

    enum class Sampler
    {
        Linear,
        Point,
    };

    class ScopedState
    {
    public:
        ScopedState();
        ScopedState(const ScopedState&) = delete;
        ScopedState& operator=(const ScopedState&) = delete;
        ~ScopedState();

    private:
        RenderStateManager& m_owner;
    };

    static RenderStateManager& Instance();

    void Initialize(ID3D11Device* device);
    void Shutdown();

    void PushState();
    void PopState();

    void SetDepthTest(bool enabled);
    void SetBlend(Blend blendMode);
    void SetBlend(BlendMode legacyBlend);
    void SetRasterizer(D3D11_CULL_MODE cullMode);
    void SetSampler(Sampler sampler);
    void SetSampler(SamplerState legacySampler);

    bool IsStateStackBalanced() const;

private:
    struct StateSnapshot
    {
        Microsoft::WRL::ComPtr<ID3D11DepthStencilState> depth;
        UINT stencilRef = 0;

        Microsoft::WRL::ComPtr<ID3D11BlendState> blend;
        FLOAT blendFactor[4]{};
        UINT sampleMask = 0xffffffff;

        Microsoft::WRL::ComPtr<ID3D11RasterizerState> rasterizer;
        Microsoft::WRL::ComPtr<ID3D11SamplerState> sampler;
    };

    void EnsureDepthStates(ID3D11Device* device);
    void EnsureBlendStates(ID3D11Device* device);
    void EnsureRasterizerStates(ID3D11Device* device);
    void EnsureSamplerStates(ID3D11Device* device);
    void RunBalanceCheck();

    Microsoft::WRL::ComPtr<ID3D11DepthStencilState> m_depthEnabled;
    Microsoft::WRL::ComPtr<ID3D11DepthStencilState> m_depthDisabled;

    Microsoft::WRL::ComPtr<ID3D11BlendState> m_blendOpaque;
    Microsoft::WRL::ComPtr<ID3D11BlendState> m_blendAlpha;

    Microsoft::WRL::ComPtr<ID3D11RasterizerState> m_rasterizerCullNone;
    Microsoft::WRL::ComPtr<ID3D11RasterizerState> m_rasterizerCullFront;
    Microsoft::WRL::ComPtr<ID3D11RasterizerState> m_rasterizerCullBack;

    Microsoft::WRL::ComPtr<ID3D11SamplerState> m_samplerLinear;
    Microsoft::WRL::ComPtr<ID3D11SamplerState> m_samplerPoint;

    std::vector<StateSnapshot> m_stateStack;
};

