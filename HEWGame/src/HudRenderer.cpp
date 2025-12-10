#include "HudRenderer.h"

#include "DirectX.h"
#include "ICamera.h"
#include "Vector.h"

#include <algorithm>
#include <array>
#include <cstring>
#include <iomanip>
#include <limits>
#include <sstream>

namespace
{
    struct IdentityCamera : public CameraSpace::ICamera
    {
        IdentityCamera()
        {
            DirectX::XMStoreFloat4x4(&m_view, DirectX::XMMatrixIdentity());
            DirectX::XMStoreFloat4x4(&m_proj, DirectX::XMMatrixIdentity());
        }

        const DirectX::XMFLOAT4X4& GetView() const override { return m_view; }
        const DirectX::XMFLOAT4X4& GetProjection() const override { return m_proj; }

    protected:
        void RecalculateView() override {}

    private:
        DirectX::XMFLOAT4X4 m_view{};
        DirectX::XMFLOAT4X4 m_proj{};
    };

    struct QuadVertex
    {
        DirectX::XMFLOAT3 pos;
        DirectX::XMFLOAT2 uv;
        DirectX::XMFLOAT3 normal;
    };

    std::shared_ptr<MeshDX> CreateQuadMesh(const DirectX::XMFLOAT2& uvScale)
    {
        const std::array<QuadVertex, 4> vertices{
            QuadVertex{ { -0.5f,  0.5f, 0.0f }, { 0.0f, 0.0f }, { 0.0f, 0.0f, -1.0f } },
            QuadVertex{ {  0.5f,  0.5f, 0.0f }, { uvScale.x, 0.0f }, { 0.0f, 0.0f, -1.0f } },
            QuadVertex{ { -0.5f, -0.5f, 0.0f }, { 0.0f, uvScale.y }, { 0.0f, 0.0f, -1.0f } },
            QuadVertex{ {  0.5f, -0.5f, 0.0f }, { uvScale.x, uvScale.y }, { 0.0f, 0.0f, -1.0f } },
        };

        MeshDX::Description desc{};
        desc.pVtx = vertices.data();
        desc.vtxSize = sizeof(QuadVertex);
        desc.vtxCount = static_cast<UINT>(vertices.size());
        desc.topology = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP;

        return std::make_shared<MeshDX>(desc, 0);
    }
}

HudRenderer::HudRenderer() = default;

HudRenderer::~HudRenderer()
{
    Shutdown();
}

bool HudRenderer::Initialize(const HudLayoutConfig& config)
{
    m_config = config;
    const size_t pixelCount = static_cast<size_t>(m_config.textureWidth) * static_cast<size_t>(m_config.textureHeight);
    m_rgbaBuffer.resize(pixelCount * 4);

    m_modelRenderer = std::make_unique<ModelRender>();
    m_quadMesh = CreateQuadMesh(DirectX::XMFLOAT2(1.0f, 1.0f));
    m_guidelineMesh = CreateQuadMesh(DirectX::XMFLOAT2(1.0f, 1.0f));

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
    m_modelRenderer.reset();
    m_quadMesh.reset();
    m_guidelineMesh.reset();
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
    if (!m_textureDirty || !m_rawBits || m_rgbaBuffer.empty())
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

    auto updatedTexture = std::make_shared<Texture2D>();
    if (updatedTexture->LoadFromMemory(
            GetDevice(),
            m_rgbaBuffer.data(),
            m_rgbaBuffer.size(),
            m_config.textureWidth,
            m_config.textureHeight,
            "RGBA"))
    {
        m_texture = updatedTexture;
        m_textureDirty = false;
    }
}

bool HudRenderer::TryGetViewportSize(float& width, float& height) const
{
    RenderTargetDX* rtv = GetDefaultRTV();
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
    if (!data.guidelineVisible || !m_guidelineTexture || !m_modelRenderer)
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

    if (!m_guidelineMesh || std::abs(m_guidelineUVScaleX - repeatCount) > std::numeric_limits<float>::epsilon())
    {
        m_guidelineMesh = CreateQuadMesh(DirectX::XMFLOAT2(repeatCount, 1.0f));
        m_guidelineUVScaleX = repeatCount;
    }

    auto material = std::make_shared<MaterialDX>(
        m_config.guidelineColor,
        DirectX::XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f),
        DirectX::XMFLOAT4(0.0f, 0.0f, 0.0f, 0.0f),
        m_guidelineTexture);

    Render::Model model(
        std::vector<std::shared_ptr<MeshDX>>{ m_guidelineMesh },
        std::vector<std::shared_ptr<MaterialDX>>{ material });

    Transform transform{};
    transform.position = float3{ 0.0f, clampedY, 0.0f };
    transform.rotation = float3{ 0.0f, 0.0f, 0.0f };
    transform.scale = float3{ 2.0f, thicknessClip, 1.0f };

    static IdentityCamera identityCamera{};
    m_modelRenderer->Draw(model, transform, &identityCamera);

    SetDepthTest(true);
}

void HudRenderer::RenderSprite(const HudDisplayData& data)
{
    if (!m_texture || !m_modelRenderer || !m_quadMesh)
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

    const bool showWarning = data.timeUp || data.remainingSeconds <= m_config.warningThresholdSeconds;
    const DirectX::XMFLOAT4 color = showWarning ? m_config.warningTextColor : m_config.normalTextColor;

    auto material = std::make_shared<MaterialDX>(
        color,
        DirectX::XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f),
        DirectX::XMFLOAT4(0.0f, 0.0f, 0.0f, 0.0f),
        m_texture);
    Render::Model model(
        std::vector<std::shared_ptr<MeshDX>>{ m_quadMesh },
        std::vector<std::shared_ptr<MaterialDX>>{ material });

    Transform transform{};
    transform.position = float3{ offsetX, offsetY, 0.0f };
    transform.rotation = float3{ 0.0f, 0.0f, 0.0f };
    transform.scale = float3{ widthClip, heightClip, 1.0f };

    static IdentityCamera identityCamera{};
    m_modelRenderer->Draw(model, transform, &identityCamera);

    SetDepthTest(true);
}

std::string HudRenderer::BuildHudString(const HudDisplayData& data) const
{
    std::ostringstream stream;
    stream << std::fixed << std::setprecision(1);
    stream << "Timer " << std::setw(5) << std::setfill(' ') << data.remainingSeconds << "s";
    stream << "  [" << (data.timeUp ? "Result" : "Play") << "]\n";

    stream << std::fixed << std::setprecision(2);
//    stream << "Top height live " << data.currentHeight << "m";
//    stream << " | peak " << data.peakHeight << "m\n";

    stream << "Speed x" << data.moveSpeedMultiplier
        << " (" << data.moveSpeedUnitsPerSecond << " u/s)  [1=Boost 2=Reset]\n";

    stream << std::fixed << std::setprecision(0);
    stream << "Score " << data.score;
    stream << "  (Enter=Confirm  R=Retry)";

    return stream.str();
}

bool HudRenderer::CreateGuidelineTexture()
{
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

    auto texture = std::make_shared<Texture2D>();
    if (!texture->LoadFromMemory(
            GetDevice(),
            reinterpret_cast<const uint8_t*>(pixels.data()),
            pixels.size() * sizeof(uint32_t),
            static_cast<unsigned int>(kSize),
            static_cast<unsigned int>(kSize),
            "BGRA"))
    {
        return false;
    }

    m_guidelineTexture = texture;
    return true;
}
