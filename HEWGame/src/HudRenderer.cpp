#include "HudRenderer.h"

#include "DirectX.h"
#include "Sprite.h"
#include "Texture.h"

#include <algorithm>
#include <array>
#include <cstring>
#include <iomanip>
#include <sstream>

namespace
{
    class HudTexture : public Texture
    {
    public:
        void UpdatePixels(const void* data, UINT pitch)
        {
            if (!GetContext())
            {
                return;
            }
            if (!m_pTex)
            {
                return;
            }
            GetContext()->UpdateSubresource(m_pTex, 0, nullptr, data, pitch, 0);
        }
    };
}

class HudRenderer::DynamicTexture : public HudTexture
{
};

HudRenderer::HudRenderer()
{
    DirectX::XMStoreFloat4x4(&m_identityMatrix, DirectX::XMMatrixIdentity());
}

HudRenderer::~HudRenderer()
{
    Shutdown();
}

bool HudRenderer::Initialize(const HudLayoutConfig& config)
{
    m_config = config;
    m_texture = std::make_unique<DynamicTexture>();
    HRESULT hr = m_texture->Create(DXGI_FORMAT_R8G8B8A8_UNORM, m_config.textureWidth, m_config.textureHeight);
    if (FAILED(hr))
    {
        return false;
    }

    const size_t pixelCount = static_cast<size_t>(m_config.textureWidth) * static_cast<size_t>(m_config.textureHeight);
    m_rgbaBuffer.resize(pixelCount * 4);

    if (!CreateTextResources())
    {
        Shutdown();
        return false;
    }

    if (!CreateGuidelineTexture())
    {
        Shutdown();
        return false;
    }

    m_ready = true;
    m_textureDirty = true;
    return true;
}

void HudRenderer::Shutdown()
{
    DestroyTextResources();
    m_texture.reset();
    m_guidelineTexture.reset();
    m_rgbaBuffer.clear();
    m_ready = false;
    m_textureDirty = false;
}

bool HudRenderer::CreateTextResources()
{
#if !defined(_WIN32)
    return false;
#else
    BITMAPINFO info = {};
    info.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    info.bmiHeader.biWidth = static_cast<LONG>(m_config.textureWidth);
    info.bmiHeader.biHeight = -static_cast<LONG>(m_config.textureHeight);
    info.bmiHeader.biPlanes = 1;
    info.bmiHeader.biBitCount = 32;
    info.bmiHeader.biCompression = BI_RGB;

    m_memoryDC = CreateCompatibleDC(nullptr);
    if (!m_memoryDC)
    {
        return false;
    }

    m_bitmap = CreateDIBSection(m_memoryDC, &info, DIB_RGB_COLORS, &m_rawBits, nullptr, 0);
    if (!m_bitmap)
    {
        return false;
    }
    m_prevBitmap = SelectObject(m_memoryDC, m_bitmap);

    LOGFONTA font = {};
    font.lfHeight = -static_cast<LONG>(m_config.fontSizePixels);
    font.lfWeight = FW_SEMIBOLD;
    strcpy_s(font.lfFaceName, "Segoe UI");
    m_font = CreateFontIndirectA(&font);
    if (!m_font)
    {
        return false;
    }
    m_prevFont = SelectObject(m_memoryDC, m_font);

    SetBkMode(m_memoryDC, TRANSPARENT);
    SetTextColor(m_memoryDC, RGB(255, 255, 255));

    return true;
#endif
}

void HudRenderer::DestroyTextResources()
{
#if defined(_WIN32)
    if (m_memoryDC && m_prevFont)
    {
        SelectObject(m_memoryDC, m_prevFont);
    }
    if (m_font)
    {
        DeleteObject(m_font);
    }
    m_prevFont = nullptr;
    m_font = nullptr;

    if (m_memoryDC && m_prevBitmap)
    {
        SelectObject(m_memoryDC, m_prevBitmap);
    }
    if (m_bitmap)
    {
        DeleteObject(m_bitmap);
    }
    m_prevBitmap = nullptr;
    m_bitmap = nullptr;
    m_rawBits = nullptr;

    if (m_memoryDC)
    {
        DeleteDC(m_memoryDC);
    }
    m_memoryDC = nullptr;
#endif
}

void HudRenderer::Draw(const HudDisplayData& data)
{
    if (!m_ready)
    {
        return;
    }

    UpdateBitmap(data);
    UploadTexture();
    RenderGuideline(data);
    RenderSprite(data);
}

void HudRenderer::UpdateBitmap(const HudDisplayData& data)
{
#if defined(_WIN32)
    if (!m_memoryDC)
    {
        return;
    }

    PatBlt(m_memoryDC, 0, 0, static_cast<int>(m_config.textureWidth), static_cast<int>(m_config.textureHeight), BLACKNESS);

    const std::string hudText = BuildHudString(data);

    RECT textRect = {};
    textRect.left = static_cast<LONG>(m_config.innerPaddingPixels);
    textRect.top = static_cast<LONG>(m_config.innerPaddingPixels);
    textRect.right = static_cast<LONG>(m_config.textureWidth) - static_cast<LONG>(m_config.innerPaddingPixels);
    textRect.bottom = static_cast<LONG>(m_config.textureHeight) - static_cast<LONG>(m_config.innerPaddingPixels);

    RECT shadowRect = textRect;
    shadowRect.left += static_cast<LONG>(m_config.shadowOffsetX);
    shadowRect.top += static_cast<LONG>(m_config.shadowOffsetY);
    shadowRect.right += static_cast<LONG>(m_config.shadowOffsetX);
    shadowRect.bottom += static_cast<LONG>(m_config.shadowOffsetY);

    SetTextColor(m_memoryDC, RGB(0, 0, 0));
    DrawTextA(m_memoryDC, hudText.c_str(), -1, &shadowRect, DT_LEFT | DT_TOP | DT_NOPREFIX);

    SetTextColor(m_memoryDC, RGB(255, 255, 255));
    DrawTextA(m_memoryDC, hudText.c_str(), -1, &textRect, DT_LEFT | DT_TOP | DT_NOPREFIX);

    GdiFlush();
    m_textureDirty = true;
#else
    (void)data;
#endif
}

void HudRenderer::UploadTexture()
{
    if (!m_textureDirty || !m_rawBits || m_rgbaBuffer.empty() || !m_texture)
    {
        return;
    }

    const size_t pixelCount = static_cast<size_t>(m_config.textureWidth) * static_cast<size_t>(m_config.textureHeight);
    uint8_t* src = static_cast<uint8_t*>(m_rawBits);
    uint8_t* dst = m_rgbaBuffer.data();
    for (size_t i = 0; i < pixelCount; ++i)
    {
        const uint8_t b = src[0];
        const uint8_t g = src[1];
        const uint8_t r = src[2];
        const uint8_t intensity = std::max({ r, g, b });
        dst[0] = 255;
        dst[1] = 255;
        dst[2] = 255;
        dst[3] = intensity;
        src += 4;
        dst += 4;
    }

    m_texture->UpdatePixels(m_rgbaBuffer.data(), m_config.textureWidth * 4);
    m_textureDirty = false;
}

bool HudRenderer::TryGetViewportSize(float& width, float& height) const
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

void HudRenderer::RenderGuideline(const HudDisplayData& data)
{
    if (!data.guidelineVisible || !m_guidelineTexture)
    {
        return;
    }

    float viewportWidth = 0.0f;
    float viewportHeight = 0.0f;
    if (!TryGetViewportSize(viewportWidth, viewportHeight))
    {
        return;
    }

    const float thicknessClip = (m_config.guidelineThicknessPixels / viewportHeight) * 2.0f;
    const float repeatSpacing = std::max(1.0f, m_config.guidelinePatternPixels);
    const float repeatCount = std::max(1.0f, viewportWidth / repeatSpacing);
    const float clampedY = std::clamp(data.guidelineClipY, -0.98f, 0.98f);

    SetDepthTest(false);
    SetBlendMode(BLEND_ALPHA);

    Sprite::SetWorld(m_identityMatrix);
    Sprite::SetView(m_identityMatrix);
    Sprite::SetProjection(m_identityMatrix);
    Sprite::SetTexture(m_guidelineTexture.get());
    Sprite::SetUVPos(DirectX::XMFLOAT2(0.0f, 0.0f));
    Sprite::SetUVScale(DirectX::XMFLOAT2(repeatCount, 1.0f));
    Sprite::SetColor(m_config.guidelineColor);
    Sprite::SetSize(DirectX::XMFLOAT2(2.0f, thicknessClip));
    Sprite::SetOffset(DirectX::XMFLOAT2(0.0f, clampedY));
    Sprite::Draw();

    SetDepthTest(true);
}

void HudRenderer::RenderSprite(const HudDisplayData& data)
{
    if (!m_texture)
    {
        return;
    }

    float viewportWidth = 0.0f;
    float viewportHeight = 0.0f;
    if (!TryGetViewportSize(viewportWidth, viewportHeight))
    {
        return;
    }

    const float widthClip = (static_cast<float>(m_config.textureWidth) / viewportWidth) * 2.0f;
    const float heightClip = (static_cast<float>(m_config.textureHeight) / viewportHeight) * 2.0f;
    const float marginXClip = (m_config.marginPixels / viewportWidth) * 2.0f;
    const float marginYClip = (m_config.marginPixels / viewportHeight) * 2.0f;

    const float offsetX = 1.0f - marginXClip - (widthClip * 0.5f);
    const float offsetY = 1.0f - marginYClip - (heightClip * 0.5f);

    SetDepthTest(false);
    SetBlendMode(BLEND_ALPHA);

    Sprite::SetWorld(m_identityMatrix);
    Sprite::SetView(m_identityMatrix);
    Sprite::SetProjection(m_identityMatrix);
    Sprite::SetTexture(m_texture.get());
    Sprite::SetUVPos(DirectX::XMFLOAT2(0.0f, 0.0f));
    Sprite::SetUVScale(DirectX::XMFLOAT2(1.0f, 1.0f));

    const bool showWarning = data.timeUp || data.remainingSeconds <= m_config.warningThresholdSeconds;
    const DirectX::XMFLOAT4 color = showWarning ? m_config.warningTextColor : m_config.normalTextColor;
    Sprite::SetColor(color);
    Sprite::SetSize(DirectX::XMFLOAT2(widthClip, heightClip));
    Sprite::SetOffset(DirectX::XMFLOAT2(offsetX, offsetY));
    Sprite::Draw();

    SetDepthTest(true);
}

std::string HudRenderer::BuildHudString(const HudDisplayData& data) const
{
    std::ostringstream stream;
    stream << std::fixed << std::setprecision(1);
    stream << "Timer " << std::setw(5) << std::setfill(' ') << data.remainingSeconds << "s";
    stream << "  [" << (data.timeUp ? "Result" : "Play") << "]\n";

    stream << std::fixed << std::setprecision(2);
    stream << "Top height live " << data.currentHeight << "m";
    stream << " | peak " << data.peakHeight << "m\n";

    stream << std::fixed << std::setprecision(0);
    stream << "Score " << data.score;
    stream << "  (Enter=Confirm  R=Retry)";

    return stream.str();
}

bool HudRenderer::CreateGuidelineTexture()
{
    m_guidelineTexture = std::make_unique<Texture>();
    if (!m_guidelineTexture)
    {
        return false;
    }

    constexpr size_t kSize = 4;
    std::array<uint32_t, kSize * kSize> pixels{};
    for (size_t y = 0; y < kSize; ++y)
    {
        for (size_t x = 0; x < kSize; ++x)
        {
            const bool opaque = (x % 2) == 0;
            pixels[y * kSize + x] = opaque ? 0xFFFFFFFFu : 0x00000000u;
        }
    }

    const HRESULT hr = m_guidelineTexture->Create(
        DXGI_FORMAT_R8G8B8A8_UNORM,
        static_cast<UINT>(kSize),
        static_cast<UINT>(kSize),
        pixels.data());
    if (FAILED(hr))
    {
        m_guidelineTexture.reset();
        return false;
    }

    return true;
}
