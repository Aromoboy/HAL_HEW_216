#include "BackgroundRenderer.h"

#include "Diagnostics/Logger.h"
#include "DirectX.h"
#include "Sprite.h"
#include "Texture.h"
#include <algorithm>
#include <sstream>

namespace
{
    struct UVWindow
    {
        float u0 = 0.0f;
        float v0 = 0.0f;
        float scaleX = 1.0f;
        float scaleY = 1.0f;
    };

    bool TryGetViewportSize(float& width, float& height)
    {
        RenderTarget* rtv = GetDefaultRTV();
        if (!rtv)
        {
            return false;
        }

        width = static_cast<float>(rtv->GetWidth());
        height = static_cast<float>(rtv->GetHeight());
        return width > 0.0f && height > 0.0f;
    }

    UVWindow ComputeVerticalUVWindow(
        float textureWidth,
        float textureHeight,
        float screenWidth,
        float screenHeight,
        float scrollOffset,
        float parallaxFactor,
        float baseOffset01,
        float pixelsPerUnit)
    {
        UVWindow out{};
        if (textureWidth <= 0.0f || textureHeight <= 0.0f || screenWidth <= 0.0f || screenHeight <= 0.0f)
        {
            return out;
        }

        const float screenAspect = screenWidth / screenHeight;
        const float textureAspect = textureWidth / textureHeight;
        const float scrollWorld = scrollOffset * parallaxFactor;
        const float scrollPx = scrollWorld * pixelsPerUnit;
        const float clampedBaseOffset01 = std::clamp(baseOffset01, 0.0f, 1.0f);

        if (textureAspect < screenAspect)
        {
            const float visibleWidthPx = textureHeight * screenAspect;
            if (visibleWidthPx <= 0.0f)
            {
                return out;
            }

            const float maxOffsetPx = std::max(0.0f, textureWidth - visibleWidthPx);
            const float baseOffsetPx = clampedBaseOffset01 * (textureWidth - visibleWidthPx);
            const float offsetPx = std::clamp(scrollPx + baseOffsetPx, 0.0f, maxOffsetPx);
            const float segmentWidthUV = std::clamp(visibleWidthPx / textureWidth, 0.0f, 1.0f);

            float u0 = offsetPx / textureWidth;
            u0 = std::clamp(u0, 0.0f, 1.0f - segmentWidthUV);

            out.u0 = u0;
            out.scaleX = segmentWidthUV;
            out.scaleY = 1.0f;
        }
        else
        {
            const float visibleHeightPx = textureWidth * (screenHeight / screenWidth);
            if (visibleHeightPx <= 0.0f)
            {
                return out;
            }

            const float maxOffsetPx = std::max(0.0f, textureHeight - visibleHeightPx);
            const float baseOffsetPx = clampedBaseOffset01 * (textureHeight - visibleHeightPx);
            const float offsetPx = std::clamp(scrollPx + baseOffsetPx, 0.0f, maxOffsetPx);
            const float segmentHeightUV = std::clamp(visibleHeightPx / textureHeight, 0.0f, 1.0f);

            float v0 = offsetPx / textureHeight;
            v0 = std::clamp(v0, 0.0f, 1.0f - segmentHeightUV);

            out.v0 = v0;
            out.scaleY = segmentHeightUV;
        }
        return out;
    }

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
    SetBlendMode(BLEND_ALPHA);

    float viewportWidth = 0.0f;
    float viewportHeight = 0.0f;
    const bool hasViewport = TryGetViewportSize(viewportWidth, viewportHeight);

    Sprite::SetWorld(m_identityMatrix);
    Sprite::SetView(m_identityMatrix);
    Sprite::SetProjection(m_identityMatrix);
    for (const auto& layer : m_layers)
    {
        if (!layer.texture)
        {
            continue;
        }

        SetSamplerState(ToSamplerState(layer.config.samplingMode));
        Sprite::SetOffset(DirectX::XMFLOAT2(0.0f, 0.0f));
        // Aspect is corrected via UVs; geometry stays in NDC.
        Sprite::SetSize(DirectX::XMFLOAT2(2.0f, 2.0f));
        Sprite::SetTexture(layer.texture.get());
        const float scrollDirection = layer.config.invertScroll ? -1.0f : 1.0f;
        const float adjustedScrollOffset = scrollDirection * scrollOffset;
        const float layerPixelsPerUnit = layer.config.scrollOffsetInPixels ? 1.0f : pixelsPerUnit;
        if (hasViewport)
        {
            const float textureWidth = static_cast<float>(layer.texture->GetWidth());
            const float textureHeight = static_cast<float>(layer.texture->GetHeight());
            const UVWindow window = ComputeVerticalUVWindow(
                textureWidth,
                textureHeight,
                viewportWidth,
                viewportHeight,
                adjustedScrollOffset,
                layer.config.parallaxFactor,
                layer.config.baseOffset01,
                layerPixelsPerUnit);
            Sprite::SetUVPos({ window.u0, window.v0 });
            Sprite::SetUVScale({ window.scaleX, window.scaleY });
        }
        else
        {
            const float parallaxOffsetY = adjustedScrollOffset * layer.config.parallaxFactor;
            const DirectX::XMFLOAT2 parallaxOffset(0.0f, parallaxOffsetY);
            Sprite::SetUVPos(parallaxOffset);
            Sprite::SetUVScale(DirectX::XMFLOAT2(1.0f, 1.0f));
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
