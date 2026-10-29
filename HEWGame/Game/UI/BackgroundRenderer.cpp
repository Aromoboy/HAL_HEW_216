#include "BackgroundRenderer.h"

#include "Diagnostics/Logger.h"
#include "DirectX.h"
#include "Sprite.h"
#include "Texture.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <fstream>
#include <unordered_map>
#include <wrl/client.h>

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

    UVWindow ComputeVerticalUVWindow(
        float textureWidth,
        float textureHeight,
        float screenWidth,
        float screenHeight,
        float scrollOffset,
        float parallaxFactor,
        float baseOffset01,
        float pixelsPerUnit)
    {
        UVWindow out{};
        if (textureWidth <= 0.0f || textureHeight <= 0.0f || screenWidth <= 0.0f || screenHeight <= 0.0f)
        {
            return out;
        }

        const float screenAspect = screenWidth / screenHeight;
        const float textureAspect = textureWidth / textureHeight;
        const float scrollWorld = scrollOffset * parallaxFactor;
        const float scrollPx = scrollWorld * pixelsPerUnit;
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
            const float baseOffsetPx = clampedBaseOffset01 * (textureWidth - visibleWidthPx);
            const float offsetPx = std::clamp(scrollPx + baseOffsetPx, 0.0f, maxOffsetPx);

            float u0 = offsetPx / textureWidth;
            u0 = std::clamp(u0, 0.0f, 1.0f - scaleX);

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
            const float baseOffsetPx = clampedBaseOffset01 * (textureHeight - visibleHeightPx);
            const float offsetPx = std::clamp(scrollPx + baseOffsetPx, 0.0f, maxOffsetPx);

            float v0 = offsetPx / textureHeight;
            v0 = std::clamp(v0, 0.0f, 1.0f - scaleY);

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

struct BackgroundRenderer::LayeredScene
{
    struct Region { float x, y, width, height; };
    struct Vertex { DirectX::XMFLOAT2 position, uv; DirectX::XMFLOAT4 color; };
    struct Decoration
    {
        Region region;
        int phase;
        float x, y, width, depth, alpha, motionX, motionY, speed, roll;
        bool flip, rise, twinkle;
        DirectX::XMFLOAT3 tint;
    };
    static constexpr size_t kMaxQuads = 128;
    Texture atlas;
    VertexShader vertexShader;
    PixelShader pixelShader;
    MeshBuffer mesh;
    std::array<Region, 3> skies{};
    std::array<float, 4> heights{};
    std::vector<Decoration> decorations;
    std::array<Vertex, kMaxQuads * 6> vertices{};
    double elapsed = 0.0;
    double displayedProgress = 0.0;
    float minimumTransitionSeconds = 13.0f;

    bool Initialize(const std::array<float, 4>& transitions, float transitionSeconds)
    {
        if (!std::isfinite(transitionSeconds) || transitionSeconds < 0) return false;
        minimumTransitionSeconds = transitionSeconds;
        heights = transitions;
        for (size_t i = 0; i < heights.size(); ++i)
        {
            if (!std::isfinite(heights[i]) || (i && heights[i] <= heights[i - 1])) return false;
        }
        if (FAILED(atlas.Create("Assets/Texture/Background/atlas.png"))) return false;
        std::ifstream input("Assets/Texture/Background/atlas.csv");
        if (!input) return false;
        std::unordered_map<std::string, Region> regions;
        std::string line;
        while (std::getline(input, line))
        {
            std::replace(line.begin(), line.end(), ',', ' ');
            std::istringstream row(line);
            std::string name;
            Region r{};
            if (!(row >> name >> r.x >> r.y >> r.width >> r.height)
                || !std::isfinite(r.x) || !std::isfinite(r.y)
                || !std::isfinite(r.width) || !std::isfinite(r.height)
                || r.x < 0 || r.y < 0 || r.width <= 0 || r.height <= 0
                || r.x + r.width > atlas.GetWidth() || r.y + r.height > atlas.GetHeight()
                || !regions.emplace(name, r).second) return false;
        }
        bool valid = true;
        auto region = [&](const char* name)
        {
            const auto found = regions.find(name);
            if (found == regions.end()) { valid = false; return Region{}; }
            return found->second;
        };
        skies = { region("DaySky"), region("EveningSky"), region("NightSky") };
        auto add = [&](const char* name, int phase, float x, float y, float width, float depth,
            float alpha = 1.0f, float mx = 0.0f, float my = 0.0f, float speed = 0.25f,
            float roll = 0.0f, bool flip = false, bool rise = false, bool twinkle = false,
            DirectX::XMFLOAT3 tint = { 1.0f, 1.0f, 1.0f })
        {
            decorations.push_back({ region(name), phase, x, y, width, depth, alpha, mx, my,
                speed, roll, flip, rise, twinkle, tint });
        };
        // ミカ：遠景から近景へ並べ、ゲーム領域の中央は動く装飾で埋めない
        // ミカ：太陽を雲の後ろに下げ、下端を不透明な雲で隠す
        add("Sun", 0, 750, 64, 135, .08f, 1, 0, 0, .3f, .03f);
        add("Rainbow", 0, -55, 100, 1080, .12f, .72f);
        add("PineappleCloud", 0, 45, 18, 285, .18f, .76f, 30, 0, .12f, .025f);
        add("Cloud", 0, 720, 135, 330, .23f, 1, 36, 0, .1f);
        add("Hill", 0, -350, 390, 1200, .3f, 1, 0, 0, .25f, 0, false, false, false, { .56f, .84f, .64f });
        add("Hill", 0, 580, 405, 1100, .32f, 1, 0, 0, .25f, 0, false, false, false, { .5f, .79f, .62f });
        add("Castle", 0, 530, 165, 290, .42f, .85f);
        add("StrawberryHouse", 0, 142, 433, 56, .38f);
        add("StrawberryHouse2", 0, 60, 443, 50, .4f);
        add("Hotairballoon", 0, 790, 230, 65, .58f, 1, 15, 22, .25f, .06f);
        add("RedBalloon", 0, 145, 212, 27, .7f, 1, 6, 20, .3f, .09f);
        add("BlueBalloon", 0, 701, 355, 24, .75f, 1, 6, 20, .4f, .08f);
        add("StrawberryCloud", 0, -40, 175, 255, .65f, .9f, 32, 0, .16f, .03f);
        add("Chocolate", 0, -25, 450, 250, 1.15f);
        add("Road", 0, 0, 442, 490, 1.05f);
        add("Bridge", 0, -15, 438, 250, 1.1f);
        add("StrawberryHouse3", 0, 790, 447, 125, 1.1f);
        add("Glow", 1, 260, -110, 760, .05f, .55f, 0, 0, .25f, 0, false, false, false, { 1, .84f, .64f });
        add("Cloud4", 1, -130, 100, 600, .16f, .52f, 28, 0, .12f);
        add("Cloud4", 1, 660, 35, 450, .2f, .6f, 25, 0, .12f, 0, true);
        for (int i = 0; i < 12; ++i)
            add("Bubble1", 1, float((i * 173 + 50) % 1100 - 80), float((i * 83 + 550) % 610 - 45),
                float(18 + i % 4 * 13), .3f + i % 4 * .2f, .38f + i % 3 * .13f,
                10, float(7 + i % 3), .2f, 0, false, true);
        add("Cloud4", 1, -180, 420, 680, 1.1f, .85f, 44, 0, .17f);
        add("Cloud4", 1, 670, 350, 650, .9f, .85f, 40, 0, .14f, 0, true);
        add("Aurora", 2, -60, -300, 1100, .06f, .42f, 0, 8, .11f, .035f, false, false, true);
        for (int i = 0; i < 40; ++i)
            add("Sparkle", 2, float((i * 313 + 27) % 960), float((i * 197 + 39) % 540),
                i % 4 == 0 ? 7.0f : 4.0f, .08f, .7f, 0, 0, .7f, 0, false, false, true);
        add("RainbowPlanet", 2, 145, 46, 120, .18f, 1, 0, 0, .11f, .06f);
        add("Saturn", 2, -45, 310, 330, .55f, 1, 0, 0, .1f, .06f);
        add("moon", 2, 725, 185, 165, .8f, 1, 0, 0, .09f, .035f);
        add("Star1", 2, 850, 65, 32, .5f, .8f, 0, 0, .4f, .1f);
        add("Star1", 2, 240, 180, 18, .23f, .7f, 0, 0, .4f, .1f);
        if (!valid || decorations.size() + skies.size() > kMaxQuads) return false;
        const char* vs = R"(
struct VS_IN { float2 pos : POSITION; float2 uv : TEXCOORD; float4 color : COLOR; };
struct VS_OUT { float4 pos : SV_POSITION; float2 uv : TEXCOORD; float4 color : COLOR; };
VS_OUT main(VS_IN v) { VS_OUT o; o.pos=float4(v.pos,0,1); o.uv=v.uv; o.color=v.color; return o; }
)";
        const char* ps = R"(
Texture2D tex : register(t0); SamplerState samp : register(s0);
struct PS_IN { float4 pos : SV_POSITION; float2 uv : TEXCOORD; float4 color : COLOR; };
// ミカ：RGBを同率で下げ、彩度と透明度を保ったまま背景の明度を抑える
float4 main(PS_IN p) : SV_TARGET {
    float4 color = tex.Sample(samp,p.uv)*p.color;
    color.rgb *= 0.95f;
    return color;
}
)";
        if (FAILED(vertexShader.Compile(vs)) || FAILED(pixelShader.Compile(ps))) return false;
        MeshBuffer::Description desc{};
        desc.pVtx = vertices.data();
        desc.vtxSize = sizeof(Vertex);
        desc.vtxCount = static_cast<UINT>(vertices.size());
        desc.isWrite = true;
        desc.topology = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
        return SUCCEEDED(mesh.Create(desc));
    }

    double ProgressForHeight(float height) const
    {
        return std::clamp((static_cast<double>(height) - heights[0]) / (heights[1] - heights[0]), 0.0, 1.0)
            + std::clamp((static_cast<double>(height) - heights[2]) / (heights[3] - heights[2]), 0.0, 1.0);
    }

    void Update(float dt, float height)
    {
        if (!std::isfinite(dt) || dt <= 0) return;
        elapsed += dt;
        if (!std::isfinite(height)) return;
        const double target = ProgressForHeight(height);
        if (minimumTransitionSeconds == 0)
        {
            displayedProgress = target;
            return;
        }
        // ミカ：隣の背景への切替速度を制限し、二つ先へ進んでも中間の背景を飛ばさない
        const double step = static_cast<double>(dt) / minimumTransitionSeconds;
        displayedProgress += std::clamp(target - displayedProgress, -step, step);
    }

    static float Fade(double progress)
    {
        const float t = static_cast<float>(std::clamp(progress, 0.0, 1.0));
        return t * t * (3.0f - 2.0f * t);
    }

    void Draw(float height, float viewportWidth, float viewportHeight)
    {
        if (!std::isfinite(height)) height = 0;
        const float t = static_cast<float>(elapsed);
        const double progress = minimumTransitionSeconds == 0 ? ProgressForHeight(height) : displayedProgress;
        const float toClouds = Fade(progress);
        const float toSpace = Fade(progress - 1.0);
        const std::array<float, 3> opacity{ 1 - toClouds, toClouds * (1 - toSpace), toSpace };
        const float scale = std::max(viewportWidth / 960.0f, viewportHeight / 540.0f);
        const float sx = 2 * scale / viewportWidth, sy = 2 * scale / viewportHeight;
        size_t count = 0;
        auto quad = [&](Region r, float x, float y, float w, float h, float angle,
            DirectX::XMFLOAT4 color, bool flip = false)
        {
            if (color.w <= .001f || count + 6 > vertices.size()) return;
            const float cs = std::cos(angle), sn = std::sin(angle);
            const float cx = x + w * .5f, cy = y + h * .5f;
            const float u0 = (r.x + .5f) / atlas.GetWidth();
            const float u1 = (r.x + r.width - .5f) / atlas.GetWidth();
            const float v0 = (r.y + .5f) / atlas.GetHeight();
            const float v1 = (r.y + r.height - .5f) / atlas.GetHeight();
            Vertex corners[4];
            const float dx[4]{ -w * .5f, w * .5f, w * .5f, -w * .5f };
            const float dy[4]{ -h * .5f, -h * .5f, h * .5f, h * .5f };
            for (int i = 0; i < 4; ++i)
            {
                const bool right = (i == 1 || i == 2) != flip;
                corners[i] = { { (cx + dx[i] * cs - dy[i] * sn - 480) * sx,
                    (270 - cy - dx[i] * sn - dy[i] * cs) * sy },
                    { right ? u1 : u0, i >= 2 ? v1 : v0 }, color };
            }
            for (int i : { 0, 1, 2, 0, 2, 3 }) vertices[count++] = corners[i];
        };
        // ミカ：空は不透明な下地を残して連続的に混ぜ、装飾は同じ高さで別々に薄くする
        quad(skies[0], 0, 0, 960, 540, 0, { 1, 1, 1, 1 });
        quad(skies[1], 0, 0, 960, 540, 0, { 1, 1, 1, toClouds });
        quad(skies[2], 0, 0, 960, 540, 0, { 1, 1, 1, toSpace });
        size_t index = 0;
        for (const Decoration& d : decorations)
        {
            const float seed = static_cast<float>(index++) * 1.73f;
            if (opacity[d.phase] <= .001f) continue;
            const float base = d.phase == 0 ? 0 : heights[d.phase == 1 ? 1 : 3];
            const float camera = std::clamp(height - base, -10.0f, 30.0f) * 5.0f * d.depth;
            const float x = d.x + std::sin(t * d.speed + seed) * d.motionX - std::sin(t * .38f) * 26 * d.depth;
            float y = d.y + camera + std::cos(t * .29f) * 12 * d.depth;
            const float spriteHeight = d.width * d.region.height / d.region.width;
            if (d.rise)
            {
                // ミカ：泡が完全に画面外へ出てから戻し、視差移動中の飛びを防ぐ
                const float period = 540.0f + 2.0f * spriteHeight + 20.0f;
                y = std::fmod(d.y - t * d.motionY + camera, period);
                if (y < 0) y += period;
                y -= spriteHeight + 10.0f;
            }
            else y += std::sin(t * d.speed + seed) * d.motionY;
            const float alpha = d.alpha * opacity[d.phase] * (d.twinkle ? .7f + .3f * std::sin(t * d.speed + seed) : 1);
            quad(d.region, x, y, d.width, spriteHeight,
                std::sin(t * d.speed + seed) * d.roll, { d.tint.x, d.tint.y, d.tint.z, alpha }, d.flip);
        }
        // ミカ：全装飾を一つの動的バッファへ転送し、一回の描画で処理する
        if (!count || FAILED(mesh.Write(vertices.data()))) return;
        vertexShader.Bind();
        pixelShader.SetTexture(0, &atlas);
        pixelShader.Bind();
        mesh.Draw(static_cast<int>(count));
    }
};

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
    Shutdown();
    m_config = config;
    if (config.layeredScene)
    {
        auto scene = std::make_unique<LayeredScene>();
        if (scene->Initialize(config.transitionHeights, config.minimumTransitionSeconds))
        {
            m_scene = std::move(scene);
            m_ready = true;
            return true;
        }
        Diagnostics::LogInfo("[BackgroundRenderer] Layered assets unavailable; using configured background texture.");
    }
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
    m_scene.reset();
    m_layers.clear();
    m_ready = false;
}

void BackgroundRenderer::Update(float dt, float scrollOffset)
{
    if (m_scene) m_scene->Update(dt, scrollOffset);
}

void BackgroundRenderer::Draw(float scrollOffset, float pixelsPerUnit) const
{
    Sprite::ScopedPassDefaults guard;

    if (!m_ready || (!m_scene && m_layers.empty()))
    {
        return;
    }

    ID3D11DeviceContext* context = GetContext();
    if (!context)
    {
        Diagnostics::LogInfo("[BackgroundRenderer] Viewport context unavailable; skipping background draw.");
        return;
    }

    // ミカ：背景専用の状態を保存し、後続のモデルやUIへ透過設定を持ち出さない
    Microsoft::WRL::ComPtr<ID3D11BlendState> oldBlend;
    Microsoft::WRL::ComPtr<ID3D11DepthStencilState> oldDepth;
    Microsoft::WRL::ComPtr<ID3D11SamplerState> oldSampler;
    FLOAT blendFactor[4]{};
    UINT sampleMask = 0, stencilRef = 0;
    context->OMGetBlendState(&oldBlend, blendFactor, &sampleMask);
    context->OMGetDepthStencilState(&oldDepth, &stencilRef);
    context->PSGetSamplers(0, 1, &oldSampler);
    SetDepthTest(false);
    SetBlendMode(BLEND_ALPHA);
    if (m_scene)
    {
        D3D11_VIEWPORT viewport{};
        UINT count = 1;
        context->RSGetViewports(&count, &viewport);
        if (count && viewport.Width > 0 && viewport.Height > 0)
        {
            SetSamplerState(SAMPLER_POSTPROCESS);
            m_scene->Draw(scrollOffset, viewport.Width, viewport.Height);
        }
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
        const float scrollDirection = layer.config.invertScroll ? -1.0f : 1.0f;
        const float adjustedScrollOffset = scrollDirection * scrollOffset;
        const float layerPixelsPerUnit = layer.config.scrollOffsetInPixels ? 1.0f : pixelsPerUnit;
        const float textureWidth = static_cast<float>(layer.texture->GetWidth());
        const float textureHeight = static_cast<float>(layer.texture->GetHeight());
        const UVWindow window = ComputeVerticalUVWindow(
            textureWidth,
            textureHeight,
            viewportWidth,
            viewportHeight,
            adjustedScrollOffset,
            layer.config.parallaxFactor,
            layer.config.baseOffset01,
            layerPixelsPerUnit);
        Sprite::SetUVPos({ window.u0, window.v0 });
        if (!(window.scaleX == 1.0f && window.scaleY == 1.0f))
        {
            Sprite::SetUVScale({ window.scaleX, window.scaleY });
        }
        Sprite::SetColor(layer.config.tintColor);
        Sprite::Draw();
    }

    context->OMSetBlendState(oldBlend.Get(), blendFactor, sampleMask);
    context->OMSetDepthStencilState(oldDepth.Get(), stencilRef);
    ID3D11SamplerState* sampler = oldSampler.Get();
    context->PSSetSamplers(0, 1, &sampler);
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
