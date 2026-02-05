#include "ResultRenderer.h"

#include "DirectX.h"
#include "Sprite.h"
#include "Texture.h"
#include "ShaderList.h"
#include "DrawMatrix.h"
#include "AppGame.h"

#include <algorithm>
#include <array>
#include <cstring>
#include <iomanip>
#include <sstream>


namespace
{
    class ResultTexture : public Texture
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

class ResultRenderer::DynamicTexture : public ResultTexture
{
};

ResultRenderer::ResultRenderer()
{
    DirectX::XMStoreFloat4x4(&m_identityMatrix, DirectX::XMMatrixIdentity());
}

ResultRenderer::~ResultRenderer()
{
    Shutdown();
}

bool ResultRenderer::Initialize(const ResultLayoutConfig& config)
{
    m_config = config;
    m_texture = std::make_unique<DynamicTexture>();
    HRESULT hr = m_texture->Create(DXGI_FORMAT_R8G8B8A8_UNORM, m_config.textureWidth, m_config.textureHeight, nullptr, true);
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


    const char* VS = R"EOT(
struct VS_IN {
	float3 pos : POSITION0;
	float2 uv : TEXCOORD0;
};
struct VS_OUT {
	float4 pos : SV_POSITION;
	float2 uv : TEXCOORD0;
	float4 color : COLOR0;
};
cbuffer Matrix : register(b0) {
	float4x4 world;
	float4x4 view;
	float4x4 proj;
};
cbuffer Param : register(b1) {
	float2 offset;
	float2 size;
	float2 uvPos;
	float2 uvScale;
	float4 color;
};
VS_OUT main(VS_IN vin) {
	VS_OUT vout;
	vout.pos = float4(vin.pos, 1.0f);
	vout.pos.xy *= size;
	vout.pos.xy += offset;
	vout.pos = mul(vout.pos, world);
	vout.pos = mul(vout.pos, view);
	vout.pos = mul(vout.pos, proj);
	vout.uv = vin.uv;
	vout.uv *= uvScale;
	vout.uv += uvPos;
	vout.color = color;
	return vout;
})EOT";
    const char* PS = R"EOT(
struct PS_IN {
	float4 pos : SV_POSITION;
	float2 uv : TEXCOORD0;
	float4 color : COLOR0;
};
Texture2D tex : register(t0);
SamplerState samp : register(s0);
float4 main(PS_IN pin) : SV_TARGET {
	return tex.Sample(samp, pin.uv) * pin.color;
})EOT";


    struct Vertex
    {
        float pos[3];
        float uv[2];
    } vtx[] = {
        {{-0.5f, 0.5f, 0.0f}, {0.0f, 0.0f}},
        {{ 0.5f, 0.5f, 0.0f}, {1.0f, 0.0f}},
        {{-0.5f,-0.5f, 0.0f}, {0.0f, 1.0f}},
        {{ 0.5f,-0.5f, 0.0f}, {1.0f, 1.0f}},
    };

    MeshDX::Description desc{};
    desc.pVtx = vtx;
    desc.vtxSize = sizeof(Vertex);
    desc.vtxCount = 4;
    desc.topology = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP;

    m_spriteMesh = std::make_shared<MeshDX>(desc, -1);

    vs = std::make_shared<VertexShader>();
    vs->Compile(VS);

    ps = std::make_shared <PixelShader>();
    ps->Compile(PS);

    m_ready = true;
    m_textureDirty = true;
    return true;
}

void ResultRenderer::Shutdown()
{
    DestroyTextResources();
    m_texture.reset();
    

    for (int i = 0; i < MAX_RESULT_SPRITE; i++)
    {
        m_resultTexture[i].reset();
    }

    m_rgbaBuffer.clear();
    m_ready = false;
    m_textureDirty = false;
}

bool ResultRenderer::CreateTextResources()
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

void ResultRenderer::DestroyTextResources()
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



void ResultRenderer::Draw(const ResultDisplayData& data)
{
    if (!m_ready)
    {
        return;
    }

    UpdateBitmap(data);
    UploadTexture();
    RenderFream(data);
}

void ResultRenderer::Draw(std::shared_ptr<Texture2D> a_texture, float2 a_pos, float2 a_size, float3 a_color, float a_alpha, float2 a_uvPos, float2 a_uvScale)
{
    float3 pos(0.0f, 0.0f, 0.0f);
    float3 size(1.0f, 1.0f, 1.0f);


    DirectX::XMFLOAT4X4 world;
    DirectX::XMFLOAT4X4 view;
    DirectX::XMFLOAT4X4 proj;

    DrawMatrix::CreateWorldMatrix(world, pos, size, float3(0.0f, 0.0f, 0.0f), false);

    DrawMatrix::CreateViewMatrix(view, float3(0.0f, 0.0f, -10.0f), float3(0.0f, 0.0f, 1000.0f), float3(0.0f, 1.0f, 0.0f));

    auto& dxContext = App::Game::Instance().GetDXContext();
    const float viewportWidth = std::max(1.0f, static_cast<float>(dxContext.GetWidth()));
    const float viewportHeight = std::max(1.0f, static_cast<float>(dxContext.GetHeight()));
    DirectX::XMStoreFloat4x4(
        &proj,
        DirectX::XMMatrixOrthographicOffCenterLH(
            0.0f,
            viewportWidth,
            viewportHeight,
            0.0f,
            0.0f,
            1.0f)
    );
    DrawSprite(a_texture, world, view, proj, DrawSpriteData(a_uvPos, a_uvScale, a_pos, a_size, a_color, a_alpha));
}

void ResultRenderer::DrawSprite(std::shared_ptr<Texture2D> texture, const DirectX::XMFLOAT4X4& worldMatrix, const DirectX::XMFLOAT4X4& viewMatrix, const DirectX::XMFLOAT4X4& projMatrix, const DrawSpriteData& drawData)
{
    // 各マトリクスと描画用のパラメーター
    DirectX::XMFLOAT4X4 wvp[3];
    DirectX::XMFLOAT4 param[3];

    // 各マトリクスを設定
    wvp[0] = worldMatrix;
    wvp[1] = viewMatrix;
    wvp[2] = projMatrix;


    // ステータスを設定
    param[0] = DirectX::XMFLOAT4(drawData.projOffset.x, drawData.projOffset.y, drawData.projScale.x, drawData.projScale.y);
    param[1] = DirectX::XMFLOAT4(drawData.uvPos.x, drawData.uvPos.y, drawData.uvScale.x, drawData.uvScale.y);
    param[2] = DirectX::XMFLOAT4(drawData.r, drawData.g, drawData.b, drawData.alpha);


    // シェーダーをバインド
    ShaderList::SetWVP(wvp);

    vs->WriteBuffer(0, wvp);
    vs->WriteBuffer(1, param);
    vs->Bind();

    ps->SetTexture(0, texture->Get());
    ps->Bind();

    // 描画
    MeshDraw();

}


void ResultRenderer::MeshDraw()
{
    ID3D11DeviceContext* pContext = GetContext();
    UINT offset = 0;

    const Core::VertexBuffer& vertexBuffer = m_spriteMesh->GetVertexBuffer();
    vertexBuffer.Bind(0, offset);

    GetContext()->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);

    pContext->Draw(vertexBuffer.GetCount(), 0);

}

void ResultRenderer::UpdateBitmap(const ResultDisplayData& data)
{
#if defined(_WIN32)
    if (!m_memoryDC)
    {
        return;
    }

    if (m_rawBits)
    {
        const size_t pixelCount = static_cast<size_t>(m_config.textureWidth) * static_cast<size_t>(m_config.textureHeight);
        std::memset(m_rawBits, 0, pixelCount * 4);
    }

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

    if (m_rawBits)
    {
        const size_t pixelCount = static_cast<size_t>(m_config.textureWidth) * static_cast<size_t>(m_config.textureHeight);
        uint8_t* pixels = static_cast<uint8_t*>(m_rawBits);
        for (size_t i = 0; i < pixelCount; ++i)
        {
            const uint8_t b = pixels[0];
            const uint8_t g = pixels[1];
            const uint8_t r = pixels[2];
            const uint8_t alpha = std::max({ r, g, b });
            pixels[3] = alpha;
            pixels += 4;
        }
    }

    GdiFlush();
    m_textureDirty = true;
#else
    (void)data;
#endif
}

void ResultRenderer::UploadTexture()
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
        const uint8_t a = src[3];
        dst[0] = r;
        dst[1] = g;
        dst[2] = b;
        dst[3] = a;
        src += 4;
        dst += 4;
    }

    m_texture->UpdatePixels(m_rgbaBuffer.data(), m_config.textureWidth * 4);
    m_textureDirty = false;
}

bool ResultRenderer::TryGetViewportSize(float& width, float& height) const
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

void ResultRenderer::RenderFream(const ResultDisplayData& data)
{

    for (int i = 0; i < MAX_RESULT_SPRITE; i++)
    {
        if (!m_resultTexture[i]) continue;
    }

    float viewportWidth = 0.0f;
    float viewportHeight = 0.0f;
    if (!TryGetViewportSize(viewportWidth, viewportHeight))
    {
        return;
    }


    for (int i = 0; i < MAX_RESULT_SPRITE; i++)
    {
        SetDepthTest(false);
        SetBlendMode(BLEND_ALPHA);

       
        DirectX::XMFLOAT4X4 proj;
        DirectX::XMStoreFloat4x4(
            &proj,
            DirectX::XMMatrixOrthographicOffCenterLH(
                0.0f, viewportWidth,
                viewportHeight, 0.0f,
                0.0f, 1.0f)
        );
        Sprite::SetWorld(m_identityMatrix);
        Sprite::SetView(m_identityMatrix);
        Sprite::SetProjection(proj);
        Sprite::SetTexture(m_resultTexture[i].get());
       /* Sprite::SetUVPos(DirectX::XMFLOAT2(0.0f, 0.0f));
        Sprite::SetUVScale(DirectX::XMFLOAT2(1.0f, 1.0f));*/
        Sprite::SetColor({ 1,1,1,1 });
     /*   Sprite::SetColor(m_config.normalTextColor);*/
        Sprite::SetSize(DirectX::XMFLOAT2(static_cast<float>(m_config.textureWidth), static_cast<float>(m_config.textureHeight)));
        Sprite::SetOffset(DirectX::XMFLOAT2(viewportWidth * 0.5f, viewportHeight * 0.5f));
        Sprite::Draw();

        SetDepthTest(true);
    }


}

void ResultRenderer::RenderSprite(const ResultDisplayData& data)
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
   /* Sprite::SetView(m_identityMatrix);
    Sprite::SetProjection(m_identityMatrix);*/



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

std::string ResultRenderer::BuildHudString(const ResultDisplayData& data) const
{
    std::ostringstream stream;
    stream << std::fixed << std::setprecision(1);
    stream << "Timer " << std::setw(5) << std::setfill(' ') << data.remainingSeconds << "s";
    stream << "  [" << (data.timeUp ? "Result" : "Play") << "]\n";

    stream << std::fixed << std::setprecision(2);

    stream << "Speed x" << data.moveSpeedMultiplier
        << " (" << data.moveSpeedUnitsPerSecond << " u/s)  [1=Boost 2=Reset]\n";

    stream << std::fixed << std::setprecision(0);
    stream << "Score " << data.score;
    stream << "  (Enter=Confirm  R=Retry)";
    stream << "\nSweets " << data.sweetsCount;
    return stream.str();
}

bool ResultRenderer::CreateGuidelineTexture()
{
    for (int i = 0; i < MAX_RESULT_SPRITE; i++)
    {
        m_resultTexture[i] = std::make_unique<Texture>();
        if (!m_resultTexture[i])
        {
            return false;
        }
    }


    constexpr size_t kSize = 4;
    std::array<uint32_t, kSize* kSize> pixels{};
    for (size_t y = 0; y < kSize; ++y)
    {
        for (size_t x = 0; x < kSize; ++x)
        {
            const bool opaque = (x % 2) == 0;
            pixels[y * kSize + x] = opaque ? 0xFFFFFFFFu : 0x00000000u;
        }
    }


    
    bool bIsFailed = false;

    for (int i = 0; i < MAX_RESULT_SPRITE; i++)
    {
       const HRESULT hr = m_resultTexture[i]->Create("Assets/Texture/background_UI_sample.png", true);
        if (FAILED(hr))
        {
            bIsFailed = true;
            break;
        }
    }


    if (bIsFailed)
    {
        for (int i = 0; i < MAX_RESULT_SPRITE; i++)
        {
            m_resultTexture[i].reset();
        }

        return false;
    }



    return true;
}
