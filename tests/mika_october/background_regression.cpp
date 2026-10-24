#include "BackgroundRenderer.h"
#include "DirectX.h"
#include "DirectXTex.h"
#include "Sprite.h"

#include <wrl/client.h>
#include <wincodec.h>
#include <cassert>
#include <cstdint>
#include <filesystem>
#include <iostream>
#include <set>
#include <string>
#include <vector>

namespace
{
    using Microsoft::WRL::ComPtr;
    ComPtr<ID3D11Device> device;
    ComPtr<ID3D11DeviceContext> context;
    ComPtr<ID3D11SamplerState> samplers[SAMPLER_MAX];
    ComPtr<ID3D11BlendState> blends[2];
    ComPtr<ID3D11DepthStencilState> depthStates[2];
    std::filesystem::path outputDirectory;

    void Require(HRESULT hr)
    {
        if (FAILED(hr))
        {
            std::cerr << "D3D/WIC failure: 0x" << std::hex << hr << '\n';
            std::abort();
        }
    }

    struct Frame
    {
        std::uint64_t hash = 1469598103934665603ULL;
        std::vector<std::uint8_t> pixels;
    };

    Frame RenderFrame(const BackgroundRenderer& renderer, float height,
        UINT width, UINT viewportHeight, const std::string& name)
    {
        D3D11_TEXTURE2D_DESC desc{};
        desc.Width = width;
        desc.Height = viewportHeight;
        desc.MipLevels = 1;
        desc.ArraySize = 1;
        desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
        desc.SampleDesc.Count = 1;
        desc.Usage = D3D11_USAGE_DEFAULT;
        desc.BindFlags = D3D11_BIND_RENDER_TARGET;
        ComPtr<ID3D11Texture2D> target;
        Require(device->CreateTexture2D(&desc, nullptr, target.GetAddressOf()));
        ComPtr<ID3D11RenderTargetView> view;
        Require(device->CreateRenderTargetView(target.Get(), nullptr, view.GetAddressOf()));
        ID3D11RenderTargetView* targetView = view.Get();
        context->OMSetRenderTargets(1, &targetView, nullptr);
        D3D11_VIEWPORT viewport{0, 0, static_cast<float>(width),
            static_cast<float>(viewportHeight), 0, 1};
        context->RSSetViewports(1, &viewport);
        const float clearColor[] = {1, 0, 1, 0};
        context->ClearRenderTargetView(view.Get(), clearColor);
        SetCullingMode(D3D11_CULL_NONE);
        SetBlendMode(BLEND_NONE);
        SetSamplerState(SAMPLER_LINEAR);
        renderer.Draw(height, 30.0f);
        context->OMSetRenderTargets(0, nullptr, nullptr);

        DirectX::ScratchImage readback;
        Require(DirectX::CaptureTexture(device.Get(), context.Get(), target.Get(), readback));
        const DirectX::Image* image = readback.GetImage(0, 0, 0);
        assert(image && image->width == width && image->height == viewportHeight);
        const auto outputPath = outputDirectory / (name + ".png");
        // ミカ：画面表示と同じRGBを保存し、ブレンド後のバックバッファのアルファを透過に使わない
        Require(DirectX::SaveToWICFile(*image, DirectX::WIC_FLAGS_NONE,
            DirectX::GetWICCodec(DirectX::WIC_CODEC_PNG), outputPath.c_str(),
            &GUID_WICPixelFormat24bppBGR));

        Frame frame;
        frame.pixels.reserve(static_cast<std::size_t>(width) * viewportHeight * 4);
        std::size_t uncovered = 0;
        for (UINT y = 0; y < viewportHeight; ++y)
        {
            const auto* row = image->pixels + image->rowPitch * y;
            for (UINT x = 0; x < width; ++x)
            {
                uncovered += row[x * 4] == 255 && row[x * 4 + 1] == 0 && row[x * 4 + 2] == 255;
                for (UINT component = 0; component < 4; ++component)
                {
                    const auto value = row[x * 4 + component];
                    frame.pixels.push_back(value);
                    frame.hash = (frame.hash ^ value) * 1099511628211ULL;
                }
            }
        }
        assert(uncovered == 0);
        std::cout << name << ": hash=" << frame.hash << " coverage=100%\n";
        return frame;
    }
}

ID3D11Device* GetDevice() { return device.Get(); }
ID3D11DeviceContext* GetContext() { return context.Get(); }
IDXGISwapChain* GetSwapChain() { return nullptr; }
RenderTarget* GetDefaultRTV() { return nullptr; }
DepthStencil* GetDefaultDSV() { return nullptr; }
namespace DebugLog { void DebugMessage(const std::string& message) { std::cerr << message; } }

// ミカ：ウィンドウ依存の状態操作だけを同じD3D11デバイスへ接続する
void SetCullingMode(D3D11_CULL_MODE mode)
{
    D3D11_RASTERIZER_DESC desc{};
    desc.FillMode = D3D11_FILL_SOLID;
    desc.CullMode = mode;
    desc.DepthClipEnable = TRUE;
    ComPtr<ID3D11RasterizerState> state;
    Require(device->CreateRasterizerState(&desc, state.GetAddressOf()));
    context->RSSetState(state.Get());
}

void SetSamplerState(SamplerState state)
{
    assert(state >= 0 && state < SAMPLER_MAX);
    if (!samplers[state])
    {
        D3D11_SAMPLER_DESC desc{};
        desc.Filter = state == SAMPLER_POINT ? D3D11_FILTER_MIN_MAG_MIP_POINT : D3D11_FILTER_MIN_MAG_MIP_LINEAR;
        desc.AddressU = desc.AddressV = desc.AddressW = state == SAMPLER_POSTPROCESS
            ? D3D11_TEXTURE_ADDRESS_CLAMP : D3D11_TEXTURE_ADDRESS_WRAP;
        desc.MaxLOD = D3D11_FLOAT32_MAX;
        Require(device->CreateSamplerState(&desc, samplers[state].GetAddressOf()));
    }
    ID3D11SamplerState* sampler = samplers[state].Get();
    context->PSSetSamplers(0, 1, &sampler);
}

void SetBlendMode(BlendMode mode)
{
    assert(mode == BLEND_NONE || mode == BLEND_ALPHA);
    const int index = mode == BLEND_ALPHA;
    if (!blends[index])
    {
        D3D11_BLEND_DESC desc{};
        auto& target = desc.RenderTarget[0];
        target.BlendEnable = index != 0;
        target.SrcBlend = D3D11_BLEND_SRC_ALPHA;
        target.DestBlend = D3D11_BLEND_INV_SRC_ALPHA;
        target.BlendOp = D3D11_BLEND_OP_ADD;
        target.SrcBlendAlpha = D3D11_BLEND_ONE;
        target.DestBlendAlpha = D3D11_BLEND_ZERO;
        target.BlendOpAlpha = D3D11_BLEND_OP_ADD;
        target.RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;
        Require(device->CreateBlendState(&desc, blends[index].GetAddressOf()));
    }
    context->OMSetBlendState(blends[index].Get(), nullptr, 0xffffffff);
}

void SetDepthTest(bool enabled)
{
    const int index = enabled;
    if (!depthStates[index])
    {
        D3D11_DEPTH_STENCIL_DESC desc{};
        desc.DepthEnable = enabled;
        desc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ALL;
        desc.DepthFunc = D3D11_COMPARISON_LESS_EQUAL;
        Require(device->CreateDepthStencilState(&desc, depthStates[index].GetAddressOf()));
    }
    context->OMSetDepthStencilState(depthStates[index].Get(), 0);
}

int main(int argc, char** argv)
{
    assert(argc == 2);
    outputDirectory = std::filesystem::absolute(argv[1]);
    std::filesystem::create_directories(outputDirectory);
    Require(CoInitializeEx(nullptr, COINIT_MULTITHREADED));
    Require(D3D11CreateDevice(nullptr, D3D_DRIVER_TYPE_WARP, nullptr, 0,
        nullptr, 0, D3D11_SDK_VERSION, device.GetAddressOf(), nullptr,
        context.GetAddressOf()));
    Sprite::Init();

    BackgroundRendererConfig config;
    config.layeredScene = true;
    // ミカ：既存の画素比較では即時切替を使い、時間制限は別のケースで検証する
    config.minimumTransitionSeconds = 0;
    BackgroundRendererConfig::BackgroundLayer fallbackLayer;
    fallbackLayer.texturePath = std::filesystem::absolute("Assets/Texture/background.png").string();
    fallbackLayer.samplingMode = BackgroundRendererConfig::SamplingMode::Clamp;
    config.layers.push_back(fallbackLayer);

    BackgroundRenderer renderer;
    assert(renderer.Initialize(config));
    std::set<std::uint64_t> heightHashes;
    Frame initial;
    for (int height : {0, 10, 20, 30, 40})
    {
        auto frame = RenderFrame(renderer, static_cast<float>(height), 960, 540,
            "height_" + std::to_string(height));
        heightHashes.insert(frame.hash);
        if (height == 0) initial = std::move(frame);
    }
    assert(heightHashes.size() == 5);

    for (int frame = 0; frame < 480; ++frame) renderer.Update(1.0f / 60.0f, 0);
    const auto animated = RenderFrame(renderer, 0, 960, 540, "height_0_time_8");
    assert(animated.pixels != initial.pixels);
    RenderFrame(renderer, 20, 800, 600, "aspect_4_3");
    RenderFrame(renderer, 20, 1260, 540, "aspect_21_9");

    BackgroundRenderer bubbleBoundary;
    assert(bubbleBoundary.Initialize(config));
    bubbleBoundary.Update(144.0f / 7.0f - 0.005f, 25);
    const auto beforeWrap = RenderFrame(bubbleBoundary, 25, 960, 540, "bubble_boundary_before");
    bubbleBoundary.Update(0.01f, 25);
    const auto afterWrap = RenderFrame(bubbleBoundary, 25, 960, 540, "bubble_boundary_after");
    std::size_t largeChanges = 0;
    for (std::size_t pixel = 0; pixel < beforeWrap.pixels.size(); pixel += 4)
    {
        int change = 0;
        for (std::size_t component = 0; component < 3; ++component)
            change += std::abs(int(beforeWrap.pixels[pixel + component]) - int(afterWrap.pixels[pixel + component]));
        largeChanges += change > 24;
    }
    std::cout << "bubble boundary: large RGB changes=" << largeChanges << '\n';
    assert(largeChanges < 128);

    // ミカ：実際の描画でも隣の背景は13秒、二段階は26秒で到達することを確認する
    BackgroundRendererConfig timedConfig = config;
    timedConfig.minimumTransitionSeconds = BackgroundRendererConfig{}.minimumTransitionSeconds;
    assert(timedConfig.minimumTransitionSeconds == 13.0f);
    BackgroundRenderer timed;
    BackgroundRenderer immediate;
    assert(timed.Initialize(timedConfig));
    assert(immediate.Initialize(config));
    timed.Update(12.0f, 40);
    immediate.Update(12.0f, 20);
    const auto beforeClouds = RenderFrame(timed, 20, 640, 360, "transition_clouds_12s");
    assert(beforeClouds.pixels != RenderFrame(immediate, 20, 640, 360, "transition_clouds_reference_12s").pixels);
    timed.Update(1.0f, 40);
    immediate.Update(1.0f, 20);
    const auto clouds = RenderFrame(timed, 20, 640, 360, "transition_clouds_13s");
    assert(clouds.pixels == RenderFrame(immediate, 20, 640, 360, "transition_clouds_reference_13s").pixels);
    assert(clouds.pixels == RenderFrame(timed, 20, 640, 360, "transition_repeated_draw").pixels);
    timed.Update(12.0f, 40);
    immediate.Update(12.0f, 40);
    assert(RenderFrame(timed, 40, 640, 360, "transition_space_25s").pixels
        != RenderFrame(immediate, 40, 640, 360, "transition_space_reference_25s").pixels);
    timed.Update(1.0f, 40);
    immediate.Update(1.0f, 40);
    assert(RenderFrame(timed, 40, 640, 360, "transition_space_26s").pixels
        == RenderFrame(immediate, 40, 640, 360, "transition_space_reference_26s").pixels);
    timed.Update(12.0f, 20);
    immediate.Update(12.0f, 20);
    assert(RenderFrame(timed, 20, 640, 360, "transition_reverse_12s").pixels
        != RenderFrame(immediate, 20, 640, 360, "transition_reverse_reference_12s").pixels);
    timed.Update(1.0f, 20);
    immediate.Update(1.0f, 20);
    assert(RenderFrame(timed, 20, 640, 360, "transition_reverse_13s").pixels
        == RenderFrame(immediate, 20, 640, 360, "transition_reverse_reference_13s").pixels);

    // ミカ：共有アセットを動かさず、プロセスの作業ディレクトリでアトラス欠落を再現する
    BackgroundRendererConfig plainConfig = config;
    plainConfig.layeredScene = false;
    BackgroundRenderer plain;
    assert(plain.Initialize(plainConfig));
    const auto expectedFallback = RenderFrame(plain, 0, 640, 360, "fallback_reference");
    const auto originalDirectory = std::filesystem::current_path();
    std::filesystem::current_path(outputDirectory);
    BackgroundRenderer missingAtlas;
    assert(missingAtlas.Initialize(config));
    std::filesystem::current_path(originalDirectory);
    const auto actualFallback = RenderFrame(missingAtlas, 0, 640, 360, "fallback_missing_atlas");
    assert(actualFallback.pixels == expectedFallback.pixels);
    context->ClearState();
    std::cout << "PASS: actual background renderer, five heights, animation, aspect ratios, 13/26-second transitions, reverse, repeated draw, atlas fallback\n";
}
