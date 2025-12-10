#include "PowerUpRenderer.h"
#include "Defines.h"
#include "DirectX.h"
#include "PowerUpTextures.h"

#include <cstdint>
#include <vector>

namespace
{
    constexpr float2 kBackFramePositionOffset = { 0.00f,  0.000f };
    constexpr float2 kFramePositionOffset = { -0.18f, -0.008f };
    constexpr float2 kIconPositionOffset = { -0.18f, -0.008f };
    constexpr float2 kCostIconPositionOffset = { 0.10f,  -0.050f };
    constexpr float2 kHudCostIconPositionOffset = { 0.40f, 0.30f };

    constexpr float kIconScaleOffset = 0.5f;
    constexpr float kFrameScaleOffset = 0.5f;
    constexpr float kBackFrameScaleOffset = 1.8f;
    constexpr float kCostIconScaleOffset = 0.3f;
    constexpr float kHudCostIconScaleOffset = 0.25f;
}

// ================================================================
// PowerUpStringTexture
// ================================================================

PowerUpStringTexture::PowerUpStringTexture()
    : m_texture(std::make_shared<Texture2D>())
    , m_stringTextureScale(float2{ 1.0f, 1.0f })
{
}

HRESULT PowerUpStringTexture::Create(DXGI_FORMAT /*format*/, UINT width, UINT height)
{
    m_width = width;
    m_height = height;
    std::vector<uint8_t> blank(static_cast<size_t>(width) * static_cast<size_t>(height) * 4, 0u);
    m_cacheKey = "powerup:string:" + std::to_string(reinterpret_cast<std::uintptr_t>(this));
    m_texture = TextureCache::Instance().LoadFromMemory(
        m_cacheKey,
        GetDevice(),
        blank.data(),
        blank.size(),
        width,
        height,
        "RGBA");

    return m_texture ? S_OK : E_FAIL;
}

void PowerUpStringTexture::UpdatePixels(const void* /*data*/, UINT /*pitch*/)
{
    // TODO: update texture pixels if you want dynamic text
}

void PowerUpStringTexture::SetScale(float2 a_scale)
{
    m_stringTextureScale = a_scale;
}

float2 PowerUpStringTexture::GetScale() const
{
    return m_stringTextureScale;
}

UINT PowerUpStringTexture::GetWidth() const
{
    return m_width;
}

UINT PowerUpStringTexture::GetHeight() const
{
    return m_height;
}

ID3D11ShaderResourceView* PowerUpStringTexture::Get() const
{
    return m_texture ? m_texture->Get() : nullptr;
}

std::shared_ptr<Texture2D> PowerUpStringTexture::GetTexture() const
{
    return m_texture;
}


// ================================================================
// PowerUpRenderer
// ================================================================

PowerUpRenderer::PowerUpRenderer(TextureCache& a_cache)
    : m_cache(a_cache)
{
    const std::string costKey = PowerUpTextures::BuildKey(PowerUpTextures::kCostPath);
    const std::string frameKey = PowerUpTextures::BuildKey(PowerUpTextures::kFramePath);
    const std::string backFrameKey = PowerUpTextures::BuildKey(PowerUpTextures::kBackFramePath);

    m_cost = m_cache.GetTexture(costKey);
    m_iconFrame = m_cache.GetTexture(frameKey);
    m_backFrame = m_cache.GetTexture(backFrameKey);

    m_quadRenderer = std::make_unique<UnifiedQuadRenderer>();
}

PowerUpRenderer::~PowerUpRenderer() = default;

bool PowerUpRenderer::Initialize(const PowerUpHudLayoutConfig& config)
{
    m_config = config;
    return true;
}

void PowerUpRenderer::Shutdown()
{
    m_quadRenderer.reset();

    m_cost.reset();
    m_iconFrame.reset();
    m_backFrame.reset();
}


// ================================================================
// PUBLIC DRAW FUNCTIONS
// ================================================================

void PowerUpRenderer::DrawHud(const CameraSpace::ICamera& camera)
{
    if (!m_cost)
        return;

    DrawSprite(
        m_cost,
        UVRect{},
        kHudCostIconPositionOffset,
        float2{ kHudCostIconScaleOffset, kHudCostIconScaleOffset },
        DirectX::XMFLOAT4(1, 1, 1, 1),
        camera
    );
}

void PowerUpRenderer::DrawCursor(
    std::shared_ptr<Texture2D> texture,
    float2 pos,
    float2 scale,
    const CameraSpace::ICamera& camera)
{
    if (!texture)
        return;

    float2 p{ pos.x + kBackFramePositionOffset.x * scale.x,
              pos.y + kBackFramePositionOffset.y * scale.y };

    float2 s{ scale.x * kBackFrameScaleOffset,
              scale.y * kBackFrameScaleOffset };

    DrawSprite(texture, UVRect{}, p, s, DirectX::XMFLOAT4(1, 1, 1, 1), camera);
}

void PowerUpRenderer::DrawNode(const NodeRenderData& data, const CameraSpace::ICamera& camera)
{
    if (!m_backFrame || !m_iconFrame || !m_cost)
        return;

    // BACK FRAME
    float2 backPos{
        data.drawPosition.x + kBackFramePositionOffset.x * data.drawScale.x,
        data.drawPosition.y + kBackFramePositionOffset.y * data.drawScale.y
    };

    float2 backScale{ data.drawScale.x * kBackFrameScaleOffset,
                      data.drawScale.y * kBackFrameScaleOffset };

    DrawSprite(m_backFrame, UVRect{}, backPos, backScale, DirectX::XMFLOAT4(1, 1, 1, 1), camera);

    // ICON FRAME
    float2 framePos{
        data.drawPosition.x + kFramePositionOffset.x * data.drawScale.x * kBackFrameScaleOffset,
        data.drawPosition.y + kFramePositionOffset.y * data.drawScale.y * kBackFrameScaleOffset
    };

    float2 frameScale{ data.drawScale.x * kFrameScaleOffset,
                       data.drawScale.y * kFrameScaleOffset };

    DrawSprite(m_iconFrame, UVRect{}, framePos, frameScale, DirectX::XMFLOAT4(1, 1, 1, 1), camera);

    // ICON
    if (data.texture)
    {
        float2 iconPos{
            data.drawPosition.x + kIconPositionOffset.x * data.drawScale.x * kBackFrameScaleOffset,
            data.drawPosition.y + kIconPositionOffset.y * data.drawScale.y * kBackFrameScaleOffset
        };

        float2 iconScale{ data.drawScale.x * kIconScaleOffset,
                          data.drawScale.y * kIconScaleOffset };

        DrawSprite(data.texture, UVRect{}, iconPos, iconScale, DirectX::XMFLOAT4(1, 1, 1, 1), camera);
    }

    // COST ICON
    float2 costPos{
        data.drawPosition.x + kCostIconPositionOffset.x * data.drawScale.x * kBackFrameScaleOffset,
        data.drawPosition.y + kCostIconPositionOffset.y * data.drawScale.y * kBackFrameScaleOffset
    };

    float2 costScale{ data.drawScale.x * kCostIconScaleOffset,
                      data.drawScale.y * kCostIconScaleOffset };

    DrawSprite(m_cost, UVRect{}, costPos, costScale, DirectX::XMFLOAT4(1, 1, 1, 1), camera);
}

void PowerUpRenderer::DrawSprite(
    const std::shared_ptr<Texture2D>& texture,
    const UVRect& uvRect,
    const float2& position,
    const float2& scale,
    const DirectX::XMFLOAT4& tint,
    const CameraSpace::ICamera& camera)
{
    (void)camera;

    if (!texture || !m_quadRenderer)
        return;

    float viewportWidth = 0.0f;
    float viewportHeight = 0.0f;
    if (!m_quadRenderer->TryGetViewportSize(viewportWidth, viewportHeight))
    {
        return;
    }

    const float centerX = (position.x * 0.5f + 0.5f) * viewportWidth;
    const float centerY = (1.0f - position.y) * 0.5f * viewportHeight;
    const float sizeX = scale.x * 0.5f * viewportWidth;
    const float sizeY = scale.y * 0.5f * viewportHeight;

    UnifiedQuadRenderer::DrawParams params{};
    params.texture = texture;
    params.tint = tint;
    params.positionPx = DirectX::XMFLOAT2(centerX - sizeX * 0.5f, centerY - sizeY * 0.5f);
    params.sizePx = DirectX::XMFLOAT2(sizeX, sizeY);
    params.uvStart = DirectX::XMFLOAT2(uvRect.start.x, uvRect.start.y);
    params.uvSize = DirectX::XMFLOAT2(uvRect.size.x, uvRect.size.y);
    params.enableBlending = true;
    params.disableDepth = true;
    m_quadRenderer->Draw(params);
}
