#include "BackgroundRenderer.h"

#include "DirectX.h"
#include "Sprite.h"

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
    Sprite::SetTexture(m_texture);
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
    const auto texture = TextureCache::Instance().Load(
        TextureCache::MakeFileKey(path),
        GetDevice(),
        std::wstring(path.begin(), path.end()));
    m_texture = texture;
    return m_texture != nullptr;
}
