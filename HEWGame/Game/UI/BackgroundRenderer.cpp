#include "BackgroundRenderer.h"

#include "Diagnostics/Logger.h"
#include "DirectX.h"
#include "Sprite.h"
#include "Texture.h"
#include <algorithm>
#include <sstream>

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

void BackgroundRenderer::Draw(float scrollOffset) const
{
    Sprite::ScopedPassDefaults guard;

    if (!m_ready || m_layers.empty())
    {
        return;
    }

    float viewportWidth = 0.0f;
    float viewportHeight = 0.0f;
    if (!TryGetViewportSize(viewportWidth, viewportHeight))
    {
        return;
    }

    SetDepthTest(false);
    SetBlendMode(BLEND_ALPHA);

    Sprite::SetWorld(m_identityMatrix);
    Sprite::SetView(m_identityMatrix);
    DirectX::XMFLOAT4X4 projection;
    DirectX::XMStoreFloat4x4(
        &projection,
        DirectX::XMMatrixOrthographicOffCenterLH(0.0f, viewportWidth, viewportHeight, 0.0f, 0.0f, 1.0f)
    );
    Sprite::SetProjection(projection);
    for (const auto& layer : m_layers)
    {
        if (!layer.texture)
        {
            continue;
        }

        const float textureWidth = static_cast<float>(layer.texture->GetWidth());
        const float textureHeight = static_cast<float>(layer.texture->GetHeight());
        if (textureWidth <= 0.0f || textureHeight <= 0.0f)
        {
            continue;
        }

        const float scaleX = viewportWidth / textureWidth;
        const float scaleY = viewportHeight / textureHeight;
        const float scale = layer.config.fitMode == BackgroundRendererConfig::FitMode::Contain
            ? std::min(scaleX, scaleY)
            : std::max(scaleX, scaleY);
        const float scaledWidth = textureWidth * scale;
        const float scaledHeight = textureHeight * scale;

        Sprite::SetSize(DirectX::XMFLOAT2(scaledWidth, scaledHeight));
        Sprite::SetOffset(DirectX::XMFLOAT2(viewportWidth * 0.5f, viewportHeight * 0.5f));
        Sprite::SetTexture(layer.texture.get());
        const float scrollDirection = layer.config.invertScroll ? -1.0f : 1.0f;
        const float parallaxOffsetY = scrollDirection * scrollOffset * layer.config.parallaxFactor;
        const DirectX::XMFLOAT2 parallaxOffset(0.0f, parallaxOffsetY);
        Sprite::SetUVPos(parallaxOffset);
        Sprite::SetUVScale(DirectX::XMFLOAT2(1.0f, 1.0f));
        Sprite::SetColor(layer.config.tintColor);
        Sprite::Draw();
    }

    SetDepthTest(true);
}

bool BackgroundRenderer::TryGetViewportSize(float& width, float& height) const
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
