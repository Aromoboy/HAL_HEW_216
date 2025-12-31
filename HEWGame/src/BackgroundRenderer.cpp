#include "BackgroundRenderer.h"

#include "Diagnostics/Logger.h"
#include "DirectX.h"
#include "Sprite.h"
#include "Texture.h"
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
    if (m_config.texturePath.empty())
    {
        return false;
    }

    if (!LoadTexture(m_config.texturePath))
    {
        return false;
    }

    m_ready = true;
    return true;
}

void BackgroundRenderer::Shutdown()
{
    m_texture.reset();
    m_ready = false;
}

void BackgroundRenderer::Draw() const
{
    if (!m_ready || !m_texture)
    {
        return;
    }

    SetDepthTest(false);
    SetBlendMode(BLEND_ALPHA);

    Sprite::SetWorld(m_identityMatrix);
    Sprite::SetView(m_identityMatrix);
    Sprite::SetProjection(m_identityMatrix);
    Sprite::SetTexture(m_texture.get());
    Sprite::SetUVPos(DirectX::XMFLOAT2(0.0f, 0.0f));
    Sprite::SetUVScale(DirectX::XMFLOAT2(1.0f, 1.0f));
    Sprite::SetColor(m_config.tintColor);
    Sprite::SetSize(DirectX::XMFLOAT2(2.0f, 2.0f));
    Sprite::SetOffset(DirectX::XMFLOAT2(0.0f, 0.0f));
    Sprite::Draw();

    SetDepthTest(true);
}

bool BackgroundRenderer::LoadTexture(const std::string& path)
{
    m_texture = std::make_unique<Texture>();
    if (!m_texture)
    {
        return false;
    }

    const HRESULT hr = m_texture->Create(path.c_str());
    if (FAILED(hr))
    {
        std::ostringstream oss;
        oss << "[BackgroundRenderer] Failed to load texture: " << path << " hr=0x" << std::hex << hr;
        Diagnostics::LogInfo(oss.str());
        m_texture.reset();
        return false;
    }

    return true;
}
