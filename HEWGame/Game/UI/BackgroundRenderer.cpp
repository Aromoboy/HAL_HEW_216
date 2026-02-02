#include "BackgroundRenderer.h"

#include "Diagnostics/Logger.h"
#include "DirectX.h"
#include "Sprite.h"
#include "Texture.h"
#include "UI/BackgroundUVUtil.h"
#include <algorithm>

#include <sstream>
#include <vector>

namespace
{
    SamplerState ToSamplerState(BackgroundRendererConfig::SamplingMode mode)
    {
        switch (mode)
        {
        case BackgroundRendererConfig::SamplingMode::Clamp:
            return SAMPLER_POSTPROCESS;
        case BackgroundRendererConfig::SamplingMode::Wrap:
        default:
            return SAMPLER_LINEAR;
        }
    }
}

BackgroundRenderer::BackgroundRenderer()
{
    DirectX::XMStoreFloat4x4(&m_identityMatrix, DirectX::XMMatrixIdentity());
}

BackgroundRenderer::~BackgroundRenderer()
{
    Shutdown();
}

bool BackgroundRenderer::Initialize(const BackgroundRendererConfig& config)
{
    m_config = config;
    m_layers.clear();
    if (m_config.layers.empty())
    {
        return false;
    }

    for (const auto& layer : m_config.layers)
    {
        if (layer.texturePath.empty())
        {
            Diagnostics::LogInfo("[BackgroundRenderer] Background layer texture path empty.");
            m_layers.clear();
            return false;
        }

        auto texture = LoadTexture(layer.texturePath);
        if (!texture)
        {
            m_layers.clear();
            return false;
        }

        LoadedLayer loadedLayer{ layer, std::move(texture) };
        m_layers.push_back(std::move(loadedLayer));
    }

    m_ready = true;
    return true;
}

void BackgroundRenderer::Shutdown()
{
    m_layers.clear();
    m_ready = false;
}

void BackgroundRenderer::Draw(float scrollOffset, float pixelsPerUnit) const
{
    Sprite::ScopedPassDefaults guard;

    if (!m_ready || m_layers.empty())
    {
        return;
    }

    SetDepthTest(false);
    //SetBlendMode(BLEND_ALPHA);

    ID3D11DeviceContext* context = GetContext();
    if (!context)
    {
        Diagnostics::LogInfo("[BackgroundRenderer] Viewport context unavailable; skipping background draw.");
        return;
    }

    for (const auto& layer : m_layers)
    {
        if (!layer.texture)
        {
            continue;
        }

        UINT viewportCount = 0;
        context->RSGetViewports(&viewportCount, nullptr);
        const bool hasViewport = viewportCount > 0;
        if (!hasViewport)
        {
            Diagnostics::LogInfo("[BackgroundRenderer] Viewport size unavailable; skipping background layer.");
            continue;
        }

        std::vector<D3D11_VIEWPORT> viewports(viewportCount);
        context->RSGetViewports(&viewportCount, viewports.data());
        const D3D11_VIEWPORT& viewport = viewports.front();

        const float viewportWidth = viewport.Width;
        const float viewportHeight = viewport.Height;
        Sprite::ResetStateToDefaults();
        Sprite::SetWorld(m_identityMatrix);
        Sprite::SetView(m_identityMatrix);
        Sprite::SetProjection(m_identityMatrix);
        SetSamplerState(ToSamplerState(layer.config.samplingMode));
        Sprite::SetOffset(DirectX::XMFLOAT2(0.0f, 0.0f));
        // Aspect is corrected via UVs; geometry stays in NDC.
        Sprite::SetSize(DirectX::XMFLOAT2(2.0f, 2.0f));
        Sprite::SetTexture(layer.texture.get());
        const float scrollDirection = layer.config.invertScroll ? -1.0f : 1.0f;
        const float adjustedScrollOffset = scrollDirection * scrollOffset;
        const float layerPixelsPerUnit = layer.config.scrollOffsetInPixels ? 1.0f : pixelsPerUnit;
        const float textureWidth = static_cast<float>(layer.texture->GetWidth());
        const float textureHeight = static_cast<float>(layer.texture->GetHeight());
        const BackgroundUV::UVWindow window = BackgroundUV::ComputeVerticalUVWindow(
            textureWidth,
            textureHeight,
            viewportWidth,
            viewportHeight,
            adjustedScrollOffset,
            layer.config.parallaxFactor,
            layer.config.baseOffset01,
            layerPixelsPerUnit);
        Sprite::SetUVPos({ window.u0, window.v0 });
        if (!(window.scaleX == 1.0f && window.scaleY == 1.0f))
        {
            Sprite::SetUVScale({ window.scaleX, window.scaleY });
        }
        Sprite::SetColor(layer.config.tintColor);
        Sprite::Draw();
    }

    SetSamplerState(SAMPLER_LINEAR);
    SetDepthTest(true);
}

std::unique_ptr<Texture> BackgroundRenderer::LoadTexture(const std::string& path) const
{
    auto texture = std::make_unique<Texture>();
    if (!texture)
    {
        return nullptr;
    }

    const HRESULT hr = texture->Create(path.c_str());
    if (FAILED(hr))
    {
        std::ostringstream oss;
        oss << "[BackgroundRenderer] Failed to load texture: " << path << " hr=0x" << std::hex << hr;
        Diagnostics::LogInfo(oss.str());
        return nullptr;
    }

    return texture;
}
