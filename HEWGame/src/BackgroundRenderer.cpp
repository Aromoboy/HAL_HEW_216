#include "BackgroundRenderer.h"

#include "DirectX.h"
#include "Sprite.h"
#include "Texture.h"

#include <sstream>

#if defined(_WIN32)
#    ifndef NOMINMAX
#        define NOMINMAX
#    endif
#    include <windows.h>
#endif

namespace
{
#if defined(_WIN32) && defined(_DEBUG)
    void DebugTrace(const std::string& message)
    {
        ::OutputDebugStringA(message.c_str());
    }
#else
    void DebugTrace(const std::string& message)
    {
        (void)message;
    }
#endif

    constexpr const char* kLogPrefix = "[BackgroundRenderer] ";
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
    if (m_config.texturePath.empty())
    {
        DebugTrace(std::string(kLogPrefix) + "No texture path configured; background renderer disabled.\n");
        return false;
    }

    if (!LoadTexture(m_config.texturePath))
    {
        std::ostringstream oss;
        oss << kLogPrefix << "Failed to load texture from '" << m_config.texturePath
            << "'. Background layer disabled.\n";
        DebugTrace(oss.str());
        return false;
    }

    m_ready = true;
    std::ostringstream oss;
    oss << kLogPrefix << "Initialized with texture='" << m_config.texturePath
        << "' tint=(" << m_config.tintColor.x << ',' << m_config.tintColor.y << ','
        << m_config.tintColor.z << ',' << m_config.tintColor.w << ").\n";
    DebugTrace(oss.str());
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
        DebugTrace(std::string(kLogPrefix) + "Failed to allocate texture instance.\n");
        return false;
    }

    if (FAILED(m_texture->Create(path.c_str())))
    {
        m_texture.reset();
        return false;
    }

    return true;
}
