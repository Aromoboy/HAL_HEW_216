#include "BackgroundRenderer.h"

#include "Diagnostics/Logger.h"
#include "Core/AppGame.h"
#include "DirectX.h"
#include "Sprite.h"
#include "Texture.h"

#include <algorithm>
#include <sstream>

namespace
{
    float Clamp01(float value)
    {
        if (value < 0.0f)
        {
            return 0.0f;
        }
        if (value > 1.0f)
        {
            return 1.0f;
        }
        return value;
    }

    float ClampRange(float value, float minValue, float maxValue)
    {
        return std::min(std::max(value, minValue), maxValue);
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

void BackgroundRenderer::Draw(float scrollOffset) const
{
    Sprite::ScopedPassDefaults guard;

    if (!m_ready || m_layers.empty())
    {
        return;
    }

    SetDepthTest(false);
    SetBlendMode(BLEND_ALPHA);
    SetSamplerState(SAMPLER_LINEAR_CLAMP);

    Sprite::SetWorld(m_identityMatrix);
    Sprite::SetView(m_identityMatrix);
    Sprite::SetProjection(m_identityMatrix);
    Sprite::SetOffset(DirectX::XMFLOAT2(0.0f, 0.0f));
    Sprite::SetSize(DirectX::XMFLOAT2(2.0f, 2.0f));

    const float t = m_config.scrollMaxWorldY > 0.0f
        ? Clamp01(scrollOffset / m_config.scrollMaxWorldY)
        : 0.0f;
    const auto& dxContext = App::Game::Instance().GetDXContext();
    const float screenWidth = static_cast<float>(dxContext.GetWidth());
    const float screenHeight = static_cast<float>(dxContext.GetHeight());
    const float screenAspect = screenHeight > 0.0f ? screenWidth / screenHeight : 1.0f;

    for (const auto& layer : m_layers)
    {
        if (!layer.texture)
        {
            continue;
        }

        Sprite::SetTexture(layer.texture.get());
        const float texWidth = static_cast<float>(layer.texture->GetWidth());
        const float texHeight = static_cast<float>(layer.texture->GetHeight());
        const float texAspect = texHeight > 0.0f ? texWidth / texHeight : 1.0f;

        float uScale = 1.0f;
        float vScale = 1.0f;
        float u0 = 0.0f;
        float v0 = 0.0f;
        if (texAspect >= screenAspect)
        {
            uScale = screenAspect / texAspect;
            vScale = 1.0f;
            u0 = (1.0f - uScale) * 0.5f;
            v0 = 0.0f;
        }
        else
        {
            uScale = 1.0f;
            vScale = texAspect / screenAspect;
            u0 = 0.0f;
        }

        const float tLayer = Clamp01(t * layer.config.parallaxFactor);
        const float maxV0 = 1.0f - vScale;
        v0 = maxV0 * (1.0f - tLayer);

        const float epsU = texWidth > 0.0f ? 0.5f / texWidth : 0.0f;
        const float epsV = texHeight > 0.0f ? 0.5f / texHeight : 0.0f;
        const float maxU0 = std::max(1.0f - uScale - epsU, epsU);
        const float maxV0Clamp = std::max(1.0f - vScale - epsV, epsV);
        u0 = ClampRange(u0, epsU, maxU0);
        v0 = ClampRange(v0, epsV, maxV0Clamp);

        Sprite::SetUVPos(DirectX::XMFLOAT2(u0, v0));
        Sprite::SetUVScale(DirectX::XMFLOAT2(uScale, vScale));
        Sprite::SetColor(layer.config.tintColor);
        Sprite::Draw();
    }

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
