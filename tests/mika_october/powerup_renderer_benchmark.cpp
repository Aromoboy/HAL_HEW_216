#include "PowerUpRenderer.h"
#include "ShaderList.h"
#include "DrawMatrix.h"
#include "DirectXTex.h"

#include <wrl/client.h>
#include <wincodec.h>
#include <algorithm>
#include <array>
#include <cassert>
#include <chrono>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <memory>
#include <string>
#include <vector>

std::uint64_t g_uploadCalls = 0;
std::uint64_t g_drawCalls = 0;

namespace
{
    using Microsoft::WRL::ComPtr;
    ComPtr<ID3D11Device> device;
    ComPtr<ID3D11DeviceContext> context;
    ComPtr<ID3D11SamplerState> sampler;
    ComPtr<ID3D11BlendState> blend;
    ComPtr<ID3D11DepthStencilState> depth;
    ComPtr<ID3D11Query> completion;
    std::unique_ptr<RenderTarget> target;
    std::array<std::unique_ptr<VertexShader>, 2> globalShaders;

    void Check(HRESULT hr)
    {
        if (FAILED(hr))
        {
            std::cerr << "D3D/WIC failed: " << std::hex << hr << '\n';
            std::abort();
        }
    }

    void WaitForGpu()
    {
        context->End(completion.Get());
        context->Flush();
        while (context->GetData(completion.Get(), nullptr, 0, 0) == S_FALSE)
        {
            SwitchToThread();
        }
    }

    class Camera final : public CameraSpace::ICamera
    {
    public:
        Camera()
        {
            DirectX::XMStoreFloat4x4(&view, DirectX::XMMatrixIdentity());
            DrawMatrix::CreateProjectionMatrix(projection, DirectX::XM_PIDIV4,
                960.0f / 540.0f, 0.1f, 1000.0f);
        }
        const DirectX::XMFLOAT4X4& GetView() const override { return view; }
        const DirectX::XMFLOAT4X4& GetProjection() const override { return projection; }
    protected:
        void RecalculateView() override {}
    private:
        DirectX::XMFLOAT4X4 view{}, projection{};
    };

    struct Card
    {
        std::shared_ptr<Texture2D> icon;
        std::shared_ptr<PowerUpStringTexture> text;
        std::string name, info;
        float2 position, scale;
        int level, maximum, cost;
        bool dirty = true;
    };

    void LoadImages(TextureCache& cache)
    {
        const std::pair<const char*, const wchar_t*> images[] = {
            {"Star", L"UI_Cakeblock_backeffect.png"},
            {"Cost", L"upgrade_UI_pointUP_icon.png"},
            {"SpeedUp", L"upgrade_UI_MoveSpeed_icon.png"},
            {"Whip", L"upgrade_UI_cream_plus_icon.png"},
            {"Frame", L"UI_whiteblock.png"},
            {"BackFrame", L"UI_Cakeblock.png"},
            {"Number", L"number.png"},
            {"SweetsSpawnSpeed", L"upgrade_UI_fruit_Speed_icon.png"},
            {"SweetsSpawnQuantity", L"upgrade_UI_fruit_UP_icon.png"},
            {"PointAmount", L"upgrade_UI_pointUP_icon _remake.png"},
            {"Timer", L"upgrade_UI_time_bonus_icon.png"},
#if POWERUP_BENCH_BEFORE
            {"PowerUpBG", L"upgrade_menu_background.png"},
            {"Fairy", L"PowerUpBackFairy.png"},
#endif
        };
        for (const auto& image : images)
        {
            assert(cache.Load(image.first, device.Get(), std::wstring(L"Assets/Texture/") + image.second));
        }
    }

    std::vector<Card> CreateCards(TextureCache& cache)
    {
        const char* keys[] = {"SpeedUp", "SweetsSpawnSpeed", "SweetsSpawnQuantity", "PointAmount", "Timer", "Whip"};
        const char* names[] = {"移動速度", "提供速度", "フルーツ提供量", "もらえるポイント", "タイマー強化", "ホイップ追加"};
        const char* info[] = {"キャラの移動速度が速くなる！", "フルーツがもっと運ばれてくる！", "一度に運ばれるフルーツの量が増える！", "より多くのポイントがもらえるようになる！", "タイマーが+5秒/レベルで増える！", "たまにホイップが運ばれてくるようになる!"};
        std::vector<Card> cards;
        for (int i = 0; i < 6; ++i)
        {
            Card card;
            card.icon = cache.Get(keys[i]);
            card.text = std::make_shared<PowerUpStringTexture>();
            Check(card.text->Create(DXGI_FORMAT_R8G8B8A8_UNORM, 512, 240, nullptr, true));
            card.name = names[i];
            card.info = info[i];
            card.position = {1.4f / 3 * (i / 2) - 0.5f, -0.4f / 2 * (i % 2) + 0.1f};
            card.scale = {1.0f / 3, 1.0f / 3};
            card.level = i == 3 || i == 5 ? 0 : 2;
            card.maximum = i == 3 || i == 5 ? 1 : 5;
            card.cost = 1000 + i * 100;
            cards.push_back(std::move(card));
        }
        return cards;
    }

    void Draw(PowerUpRenderer& renderer, const Camera& camera,
        std::vector<Card>& cards, const std::shared_ptr<Texture2D>& cursor)
    {
        const float clear[] = {0.12f, 0.16f, 0.22f, 1.0f};
        context->ClearRenderTargetView(target->GetView(), clear);
        renderer.DrawHud(camera);
        renderer.DrawCost(12345, camera);
        renderer.DrawCursor(cursor, {cards[0].position.x + 0.0015f, cards[0].position.y + 0.014f},
            cards[0].scale, camera, 0.75f);
        for (auto& card : cards)
        {
            PowerUpRenderer::NodeRenderData data(card.icon, card.text, card.position, card.scale,
                card.dirty, card.name, card.info, static_cast<float>(card.level),
                static_cast<float>(card.maximum), static_cast<float>(card.cost));
            renderer.DrawNode(data, camera);
            card.dirty = false;
        }
    }

    std::uint64_t SaveFrame(const std::filesystem::path& directory)
    {
        ComPtr<ID3D11Resource> resource;
        target->GetView()->GetResource(resource.GetAddressOf());
        DirectX::ScratchImage captured;
        Check(DirectX::CaptureTexture(device.Get(), context.Get(), resource.Get(), captured));
        const auto* frame = captured.GetImage(0, 0, 0);
        assert(frame);
        const auto png = directory / "frame.png";
        Check(DirectX::SaveToWICFile(*frame, DirectX::WIC_FLAGS_NONE,
            DirectX::GetWICCodec(DirectX::WIC_CODEC_PNG), png.c_str(), &GUID_WICPixelFormat24bppBGR));
        std::ofstream raw(directory / "frame.rgba", std::ios::binary);
        std::uint64_t hash = 1469598103934665603ULL;
        for (std::size_t y = 0; y < frame->height; ++y)
        {
            const auto* row = frame->pixels + frame->rowPitch * y;
            raw.write(reinterpret_cast<const char*>(row), frame->width * 4);
            for (std::size_t x = 0; x < frame->width * 4; ++x) hash = (hash ^ row[x]) * 1099511628211ULL;
        }
        return hash;
    }
}

ID3D11Device* GetDevice() { return device.Get(); }
ID3D11DeviceContext* GetContext() { return context.Get(); }
IDXGISwapChain* GetSwapChain() { return nullptr; }
RenderTarget* GetDefaultRTV() { return target.get(); }
DepthStencil* GetDefaultDSV() { return nullptr; }
namespace DebugLog { void DebugMessage(const std::string& message) { std::cerr << message; } }

// ミカ：旧描画が更新していた二つの未使用VSを同じサイズの定数バッファで再現する
void ShaderList::SetWVP(DirectX::XMFLOAT4X4* matrices)
{
    for (const auto& shader : globalShaders) shader->WriteBuffer(0, matrices);
}
void SetBlendMode(BlendMode mode)
{
    assert(mode == BLEND_ALPHA);
    context->OMSetBlendState(blend.Get(), nullptr, 0xffffffff);
}
void SetSamplerState(SamplerState mode)
{
    assert(mode == SAMPLER_LINEAR);
    ID3D11SamplerState* state = sampler.Get();
    context->PSSetSamplers(0, 1, &state);
}

int main(int argc, char** argv)
{
    assert(argc == 2);
    const auto output = std::filesystem::absolute(argv[1]);
    std::filesystem::create_directories(output);
    Check(CoInitializeEx(nullptr, COINIT_MULTITHREADED));
    HRESULT creation = D3D11CreateDevice(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, 0,
        nullptr, 0, D3D11_SDK_VERSION, device.GetAddressOf(), nullptr, context.GetAddressOf());
    const bool hardware = SUCCEEDED(creation);
    if (!hardware)
    {
        Check(D3D11CreateDevice(nullptr, D3D_DRIVER_TYPE_WARP, nullptr, 0,
            nullptr, 0, D3D11_SDK_VERSION, device.GetAddressOf(), nullptr, context.GetAddressOf()));
    }
    ComPtr<IDXGIDevice> dxgi;
    Check(device.As(&dxgi));
    ComPtr<IDXGIAdapter> adapter;
    Check(dxgi->GetAdapter(adapter.GetAddressOf()));
    DXGI_ADAPTER_DESC adapterDesc{};
    Check(adapter->GetDesc(&adapterDesc));
    std::wstring wideName(adapterDesc.Description);
    const std::string adapterName(wideName.begin(), wideName.end());

    target = std::make_unique<RenderTarget>();
    Check(target->Create(DXGI_FORMAT_R8G8B8A8_UNORM, 960, 540));
    ID3D11RenderTargetView* view = target->GetView();
    context->OMSetRenderTargets(1, &view, nullptr);
    const D3D11_VIEWPORT viewport{0, 0, 960, 540, 0, 1};
    context->RSSetViewports(1, &viewport);
    D3D11_RASTERIZER_DESC rasterDesc{};
    rasterDesc.FillMode = D3D11_FILL_SOLID;
    rasterDesc.CullMode = D3D11_CULL_NONE;
    rasterDesc.DepthClipEnable = TRUE;
    ComPtr<ID3D11RasterizerState> raster;
    Check(device->CreateRasterizerState(&rasterDesc, raster.GetAddressOf()));
    context->RSSetState(raster.Get());
    D3D11_SAMPLER_DESC samplerDesc{};
    samplerDesc.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
    samplerDesc.AddressU = samplerDesc.AddressV = samplerDesc.AddressW = D3D11_TEXTURE_ADDRESS_WRAP;
    samplerDesc.MaxLOD = D3D11_FLOAT32_MAX;
    Check(device->CreateSamplerState(&samplerDesc, sampler.GetAddressOf()));
    D3D11_BLEND_DESC blendDesc{};
    auto& color = blendDesc.RenderTarget[0];
    color.BlendEnable = TRUE;
    color.SrcBlend = D3D11_BLEND_SRC_ALPHA;
    color.DestBlend = D3D11_BLEND_INV_SRC_ALPHA;
    color.BlendOp = D3D11_BLEND_OP_ADD;
    color.SrcBlendAlpha = D3D11_BLEND_ONE;
    color.DestBlendAlpha = D3D11_BLEND_ZERO;
    color.BlendOpAlpha = D3D11_BLEND_OP_ADD;
    color.RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;
    Check(device->CreateBlendState(&blendDesc, blend.GetAddressOf()));
    D3D11_DEPTH_STENCIL_DESC depthDesc{};
    depthDesc.DepthEnable = FALSE;
    Check(device->CreateDepthStencilState(&depthDesc, depth.GetAddressOf()));
    context->OMSetDepthStencilState(depth.Get(), 0);
    D3D11_QUERY_DESC queryDesc{D3D11_QUERY_EVENT, 0};
    Check(device->CreateQuery(&queryDesc, completion.GetAddressOf()));
    SetBlendMode(BLEND_ALPHA);
    SetSamplerState(SAMPLER_LINEAR);

    constexpr const char* globalCode = R"(
cbuffer Matrix : register(b0) { float4x4 world; float4x4 view; float4x4 projection; };
float4 main(float3 position : POSITION) : SV_POSITION {
    return mul(mul(mul(float4(position,1),world),view),projection);
})";
    for (auto& shader : globalShaders)
    {
        shader = std::make_unique<VertexShader>();
        Check(shader->Compile(globalCode));
    }
    auto& cache = TextureCache::Instance();
    LoadImages(cache);
    PowerUpRenderer renderer(cache);
    PowerUpHudLayoutConfig layout;
    layout.textureHeight = 240;
    layout.marginPixels = 32;
    layout.fontSizePixels = 28;
    assert(renderer.Initialize(layout));
    auto cards = CreateCards(cache);
    Camera camera;
    const auto cursor = cache.Get("Star");
    for (int i = 0; i < 120; ++i)
    {
        Draw(renderer, camera, cards, cursor);
        WaitForGpu();
    }
    std::vector<double> times;
    times.reserve(600);
    for (int i = 0; i < 600; ++i)
    {
        g_drawCalls = g_uploadCalls = 0;
        const auto start = std::chrono::steady_clock::now();
        Draw(renderer, camera, cards, cursor);
        const auto finish = std::chrono::steady_clock::now();
        times.push_back(std::chrono::duration<double, std::milli>(finish - start).count());
        // ミカ：GPU待機は測定外に置き、蓄積したキューによる待ち時間とCPU送信を分ける
        WaitForGpu();
    }
    const auto hash = SaveFrame(output);
    std::ofstream samples(output / "samples.csv");
    samples << "frame,cpu_submission_ms\n";
    for (std::size_t i = 0; i < times.size(); ++i) samples << i << ',' << std::setprecision(9) << times[i] << '\n';
    std::sort(times.begin(), times.end());
    std::ofstream report(output / "result.json");
    report << std::fixed << std::setprecision(6)
        << "{\"adapter\":\"" << adapterName << "\",\"hardware\":" << (hardware ? "true" : "false")
        << ",\"width\":960,\"height\":540,\"frames\":600,\"cpu_median_ms\":" << times[times.size() / 2]
        << ",\"cpu_p95_ms\":" << times[times.size() * 95 / 100]
        << ",\"draw_calls\":" << g_drawCalls << ",\"constant_buffer_uploads\":" << g_uploadCalls
        << ",\"pixel_hash\":\"" << hash << "\"}\n";
    std::cout << adapterName << " hardware=" << hardware << " CPU median=" << times[times.size()/2]
        << "ms p95=" << times[times.size()*95/100] << "ms draws=" << g_drawCalls
        << " uploads=" << g_uploadCalls << " hash=" << hash << '\n';
    renderer.Shutdown();
    context->ClearState();
    WaitForGpu();
}
