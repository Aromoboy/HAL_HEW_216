#include "BackgroundRenderer.h"

#include "Diagnostics/Logger.h"
#include "DirectX.h"
#include "Sprite.h"
#include "Texture.h"
#include <algorithm>
#include <cmath>
#include <sstream>
#include <vector>

namespace
{
    struct UVWindow
    {
        float u0 = 0.0f;
        float v0 = 0.0f;
        float scaleX = 1.0f;
        float scaleY = 1.0f;
    };

    float WrapOffset(float value, float wrapSize)
    {
        if (wrapSize <= 0.0f)
        {
            return 0.0f;
        }

        float wrapped = std::fmod(value, wrapSize);
        if (wrapped < 0.0f)
        {
            wrapped += wrapSize;
        }

        return wrapped;
    }

    UVWindow ComputeVerticalUVWindow(
        float textureWidth,
        float textureHeight,
        float screenWidth,
        float screenHeight,
        float scrollOffset,
        float baseOffset01,
        float pixelsPerUnit,
        bool wrapSampling)
    {
        UVWindow out{};
        if (textureWidth <= 0.0f || textureHeight <= 0.0f || screenWidth <= 0.0f || screenHeight <= 0.0f)
        {
            return out;
        }

        constexpr float kWrapMarginPx = 0.5f;
        const float screenAspect = screenWidth / screenHeight;
        const float textureAspect = textureWidth / textureHeight;
        const float scrollWorld = scrollOffset;
        const float tileHeightWorld = (pixelsPerUnit > 0.0f) ? (textureHeight / pixelsPerUnit) : 0.0f;
        float scrollPx = scrollWorld * pixelsPerUnit;
        if (!wrapSampling && tileHeightWorld > 0.0f)
        {
            float normalizedScrollWorld = std::fmod(scrollWorld, tileHeightWorld);
            if (normalizedScrollWorld < 0.0f)
            {
                normalizedScrollWorld += tileHeightWorld;
            }
            scrollPx = normalizedScrollWorld * pixelsPerUnit;
        }
        const float clampedBaseOffset01 = std::clamp(baseOffset01, 0.0f, 1.0f);

        if (textureAspect > screenAspect)
        {
            const float scaleX = screenAspect / textureAspect;
            if (scaleX <= 0.0f)
            {
                return out;
            }

            const float visibleWidthPx = textureWidth * scaleX;
            const float offsetRangePx = textureWidth - visibleWidthPx;
            const float maxOffsetPx = std::max(0.0f, offsetRangePx);
            float baseOffsetPx = clampedBaseOffset01 * (textureWidth - visibleWidthPx);
            float offsetPx = scrollPx + baseOffsetPx;
            float u0 = 0.0f;

            if (wrapSampling)
            {
                baseOffsetPx = clampedBaseOffset01 * textureWidth;
                offsetPx = WrapOffset(scrollPx + baseOffsetPx + kWrapMarginPx, textureWidth);
                u0 = offsetPx / textureWidth;
            }
            else
            {
                offsetPx = std::clamp(offsetPx, 0.0f, maxOffsetPx);
                u0 = offsetPx / textureWidth;
                u0 = std::clamp(u0, 0.0f, 1.0f - scaleX);
            }

            out.u0 = u0;
            out.scaleX = scaleX;
            out.scaleY = 1.0f;
        }
        else if (textureAspect < screenAspect)
        {
            const float scaleY = textureAspect / screenAspect;
            if (scaleY <= 0.0f)
            {
                return out;
            }

            const float visibleHeightPx = textureHeight * scaleY;
            const float offsetRangePx = textureHeight - visibleHeightPx;
            const float maxOffsetPx = std::max(0.0f, offsetRangePx);
            float baseOffsetPx = clampedBaseOffset01 * (textureHeight - visibleHeightPx);
            float offsetPx = scrollPx + baseOffsetPx;
            float v0 = 0.0f;

            if (wrapSampling)
            {
                baseOffsetPx = clampedBaseOffset01 * textureHeight;
                offsetPx = WrapOffset(scrollPx + baseOffsetPx + kWrapMarginPx, textureHeight);
                v0 = offsetPx / textureHeight;
            }
            else
            {
                offsetPx = std::clamp(offsetPx, 0.0f, maxOffsetPx);
                v0 = offsetPx / textureHeight;
                v0 = std::clamp(v0, 0.0f, 1.0f - scaleY);
            }

            out.v0 = v0;
            out.scaleY = scaleY;
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
        // scrollOffset is world-space displacement coming from the active scene/camera.
        const float scrollDirection = layer.config.invertScroll ? -1.0f : 1.0f;
        const float adjustedScrollOffset = scrollDirection * scrollOffset;
        const float layerPixelsPerUnit = layer.config.scrollOffsetInPixels ? 1.0f : pixelsPerUnit;
        const float textureWidth = static_cast<float>(layer.texture->GetWidth());
        const float textureHeight = static_cast<float>(layer.texture->GetHeight());
        const bool wrapSampling = layer.config.samplingMode == BackgroundRendererConfig::SamplingMode::Wrap;
        const UVWindow window = ComputeVerticalUVWindow(
            textureWidth,
            textureHeight,
            viewportWidth,
            viewportHeight,
            adjustedScrollOffset,
            layer.config.baseOffset01,
            layerPixelsPerUnit,
            wrapSampling);
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

std::optional<float> BackgroundRenderer::GetMinLayerTextureHeightPixels() const
{
    if (!m_ready || m_layers.empty())
    {
        return std::nullopt;
    }

    std::optional<float> minHeight;
    for (const auto& layer : m_layers)
    {
        if (!layer.texture)
        {
            continue;
        }

        const float height = static_cast<float>(layer.texture->GetHeight());
        if (!std::isfinite(height) || height <= 0.0f)
        {
            continue;
        }

        if (!minHeight || height < *minHeight)
        {
            minHeight = height;
        }
    }

    return minHeight;
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
