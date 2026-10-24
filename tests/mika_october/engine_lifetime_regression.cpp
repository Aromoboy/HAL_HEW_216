#include <d3d11.h>
#include <d3dcompiler.h>
#include <wrl/client.h>
#include <cassert>
#include <cstring>
#include <iostream>
#include <map>
#include <array>
#include <cmath>
#include <vector>

namespace
{
    Microsoft::WRL::ComPtr<ID3D11Device> device;
    Microsoft::WRL::ComPtr<ID3D11DeviceContext> context;
    std::vector<ID3D11ShaderReflection*> reflections;
    int reflectionCalls = 0;
    int failReflectionCall = 0;
    int contextCalls = 0;

    HRESULT TrackReflection(LPCVOID data, SIZE_T size, REFIID iid, void** result)
    {
        ++reflectionCalls;
        if (reflectionCalls == failReflectionCall)
        {
            *result = nullptr;
            return E_FAIL;
        }
        const HRESULT hr = D3DReflect(data, size, iid, result);
        if (SUCCEEDED(hr))
        {
            auto* reflection = static_cast<ID3D11ShaderReflection*>(*result);
            reflection->AddRef();
            reflections.push_back(reflection);
        }
        return hr;
    }

    void CheckReleasedReflections(std::size_t expected)
    {
        assert(reflections.size() == expected);
        std::map<ID3D11ShaderReflection*, ULONG> retained;
        for (auto* reflection : reflections)
        {
            ++retained[reflection];
        }
        for (auto* reflection : reflections)
        {
            const ULONG expectedRemaining = --retained[reflection];
            const ULONG actualRemaining = reflection->Release();
            assert(actualRemaining == expectedRemaining);
        }
        reflections.clear();
    }
}

// ミカ：実装本体の反射生成だけを観測し、テストが保持した参照以外の解放を検証する
#define D3DReflect TrackReflection
#include "../../HEWGame/Engine/Render/Shader.cpp"
#undef D3DReflect
#include "../../HEWGame/Engine/Render/MeshBuffer.h"
#include "../../HEWGame/Engine/Render/Geometory.h"

ID3D11Device* GetDevice() { return device.Get(); }
ID3D11DeviceContext* GetContext() { ++contextCalls; return context.Get(); }
ID3D11ShaderResourceView* Texture::GetResource() const { return m_pSRV; }
namespace DebugLog { void DebugMessage(const std::string&) {} }

namespace
{
    using Color = std::array<float, 4>;
    using Microsoft::WRL::ComPtr;

    struct RenderTarget
    {
        ComPtr<ID3D11Texture2D> color;
        ComPtr<ID3D11Texture2D> staging;
        ComPtr<ID3D11RenderTargetView> view;

        RenderTarget()
        {
            D3D11_TEXTURE2D_DESC desc{};
            desc.Width = desc.Height = 16;
            desc.MipLevels = desc.ArraySize = 1;
            desc.Format = DXGI_FORMAT_R32G32B32A32_FLOAT;
            desc.SampleDesc.Count = 1;
            desc.BindFlags = D3D11_BIND_RENDER_TARGET;
            assert(SUCCEEDED(device->CreateTexture2D(&desc, nullptr, color.GetAddressOf())));
            assert(SUCCEEDED(device->CreateRenderTargetView(color.Get(), nullptr, view.GetAddressOf())));
            desc.BindFlags = 0;
            desc.Usage = D3D11_USAGE_STAGING;
            desc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
            assert(SUCCEEDED(device->CreateTexture2D(&desc, nullptr, staging.GetAddressOf())));
        }

        void BindAndClear()
        {
            const float clear[] = {0, 0, 0, 0};
            context->ClearRenderTargetView(view.Get(), clear);
            auto* renderView = view.Get();
            context->OMSetRenderTargets(1, &renderView, nullptr);
            const D3D11_VIEWPORT viewport{0, 0, 16, 16, 0, 1};
            context->RSSetViewports(1, &viewport);
            D3D11_RASTERIZER_DESC desc{};
            desc.FillMode = D3D11_FILL_SOLID;
            desc.CullMode = D3D11_CULL_NONE;
            desc.DepthClipEnable = TRUE;
            ComPtr<ID3D11RasterizerState> rasterizer;
            assert(SUCCEEDED(device->CreateRasterizerState(&desc, rasterizer.GetAddressOf())));
            context->RSSetState(rasterizer.Get());
        }

        std::vector<Color> Read()
        {
            context->CopyResource(staging.Get(), color.Get());
            D3D11_MAPPED_SUBRESOURCE mapped{};
            assert(SUCCEEDED(context->Map(staging.Get(), 0, D3D11_MAP_READ, 0, &mapped)));
            std::vector<Color> pixels(16 * 16);
            for (unsigned y = 0; y < 16; ++y)
                std::memcpy(&pixels[y * 16], static_cast<const char*>(mapped.pData) + y * mapped.RowPitch,
                    16 * sizeof(Color));
            context->Unmap(staging.Get(), 0);
            return pixels;
        }
    };

    ComPtr<ID3D11ShaderResourceView> MakeTexture(const Color& color)
    {
        D3D11_TEXTURE2D_DESC desc{};
        desc.Width = desc.Height = desc.MipLevels = desc.ArraySize = 1;
        desc.Format = DXGI_FORMAT_R32G32B32A32_FLOAT;
        desc.SampleDesc.Count = 1;
        desc.Usage = D3D11_USAGE_IMMUTABLE;
        desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
        const D3D11_SUBRESOURCE_DATA data{color.data(), sizeof(Color), 0};
        ComPtr<ID3D11Texture2D> texture;
        ComPtr<ID3D11ShaderResourceView> resource;
        assert(SUCCEEDED(device->CreateTexture2D(&desc, &data, texture.GetAddressOf())));
        assert(SUCCEEDED(device->CreateShaderResourceView(texture.Get(), nullptr, resource.GetAddressOf())));
        return resource;
    }

    void CheckColor(const Color& actual, const Color& expected)
    {
        for (unsigned component = 0; component < 4; ++component)
            assert(std::abs(actual[component] - expected[component]) < 0.00001f);
    }

    void CheckBuffer(ID3D11Buffer* buffer, const Color& expected)
    {
        assert(buffer);
        D3D11_BUFFER_DESC desc{};
        buffer->GetDesc(&desc);
        desc.Usage = D3D11_USAGE_STAGING;
        desc.BindFlags = 0;
        desc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
        ComPtr<ID3D11Buffer> staging;
        assert(SUCCEEDED(device->CreateBuffer(&desc, nullptr, staging.GetAddressOf())));
        context->CopyResource(staging.Get(), buffer);
        D3D11_MAPPED_SUBRESOURCE mapped{};
        assert(SUCCEEDED(context->Map(staging.Get(), 0, D3D11_MAP_READ, 0, &mapped)));
        Color actual{};
        std::memcpy(actual.data(), mapped.pData, sizeof(actual));
        context->Unmap(staging.Get(), 0);
        CheckColor(actual, expected);
    }

    void CheckSparseBindings()
    {
        // ミカ：b1を最適化で除去し、b3とt2およびt5配列が実際のGPU描画に届くことを確認する
        constexpr const char* vertexSource = R"(
cbuffer Scale : register(b0) { float4 scale; };
cbuffer Unused : register(b1) { float4 unused; };
cbuffer Offset : register(b3) { float4 offset; };
float4 main(float3 position : POSITION) : SV_POSITION {
    return float4(position.xy * scale.xy + offset.xy, position.z, 1);
})";
        constexpr const char* pixelSource = R"(
cbuffer FirstColor : register(b0) { float4 first; };
cbuffer LastColor : register(b3) { float4 last; };
Texture2D<float4> singleTexture : register(t2);
Texture2D<float4> textureArray[2] : register(t5);
float4 main() : SV_TARGET {
    return first + last + singleTexture.Load(int3(0, 0, 0))
        + textureArray[0].Load(int3(0, 0, 0)) + textureArray[1].Load(int3(0, 0, 0));
})";
        VertexShader vertex;
        PixelShader pixel;
        assert(SUCCEEDED(vertex.Compile(vertexSource)));
        assert(SUCCEEDED(pixel.Compile(pixelSource)));
        CheckReleasedReflections(3);
        Color scale{0.5f, 0.5f, 0, 0};
        Color offset{0.5f, 0.5f, 0, 0};
        Color first{0.125f, 0, 0, 0.125f};
        Color last{0, 0.25f, 0, 0.125f};
        vertex.WriteBuffer(0, scale.data());
        vertex.WriteBuffer(3, offset.data());
        vertex.WriteBuffer(1, offset.data());
        pixel.WriteBuffer(0, first.data());
        pixel.WriteBuffer(3, last.data());
        pixel.WriteBuffer(2, last.data());
        auto texture2 = MakeTexture({0, 0, 0.5f, 0.25f});
        auto texture5 = MakeTexture({0.125f, 0.125f, 0, 0.25f});
        auto texture6 = MakeTexture({0.25f, 0.125f, 0.25f, 0.25f});
        pixel.SetTexture(2, texture2.Get());
        pixel.SetTexture(5, texture5.Get());
        pixel.SetTexture(6, texture6.Get());
        context->ClearState();
        vertex.Bind();
        pixel.Bind();
        ID3D11Buffer* buffers[4]{};
        context->VSGetConstantBuffers(0, 4, buffers);
        CheckBuffer(buffers[0], scale);
        CheckBuffer(buffers[3], offset);
        assert(!buffers[1] && !buffers[2]);
        for (auto* buffer : buffers) if (buffer) buffer->Release();
        context->PSGetConstantBuffers(0, 4, buffers);
        CheckBuffer(buffers[0], first);
        CheckBuffer(buffers[3], last);
        assert(!buffers[1] && !buffers[2]);
        for (auto* buffer : buffers) if (buffer) buffer->Release();
        ID3D11ShaderResourceView* resources[7]{};
        context->PSGetShaderResources(0, 7, resources);
        assert(resources[2] == texture2.Get() && resources[5] == texture5.Get() && resources[6] == texture6.Get());
        assert(!resources[0] && !resources[1] && !resources[3] && !resources[4]);
        for (auto* resource : resources) if (resource) resource->Release();

        const float vertices[] = {-1, -1, 0.5f, -1, 3, 0.5f, 3, -1, 0.5f};
        MeshBuffer mesh;
        MeshBuffer::Description desc{};
        desc.pVtx = vertices;
        desc.vtxSize = 3 * sizeof(float);
        desc.vtxCount = 3;
        desc.topology = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
        assert(SUCCEEDED(mesh.Create(desc)));
        RenderTarget target;
        target.BindAndClear();
        mesh.Draw();
        const auto before = target.Read();
        CheckColor(before[2 * 16 + 13], {0.5f, 0.5f, 0.75f, 1});
        CheckColor(before[13 * 16 + 2], {0, 0, 0, 0});

        // ミカ：再作成の途中失敗でも既存のシェーダーと定数を維持する
        reflectionCalls = 0;
        failReflectionCall = 2;
        assert(FAILED(vertex.Compile(vertexSource)));
        CheckReleasedReflections(1);
        failReflectionCall = 0;
        assert(FAILED(pixel.Compile("invalid HLSL")));
        vertex.Bind();
        pixel.Bind();
        target.BindAndClear();
        mesh.Draw();
        assert(before == target.Read());
        std::cout << "PASS: WARP b0/b3 holes, t2/t5[2] Load, GPU buffer readback, pixels, failed replacement\n";
    }

    void CheckLines()
    {
        RenderTarget target;
        target.BindAndClear();
        const int emptyCalls = contextCalls;
        Geometory::DrawLines();
        assert(contextCalls == emptyCalls);
        Geometory::AddLine({-0.75f, 0, 0.5f}, {0.75f, 0, 0.5f}, {1, 0, 0, 1});
        D3D11_QUERY_DESC desc{D3D11_QUERY_PIPELINE_STATISTICS, 0};
        ComPtr<ID3D11Query> query;
        assert(SUCCEEDED(device->CreateQuery(&desc, query.GetAddressOf())));
        context->Begin(query.Get());
        Geometory::DrawLines();
        context->End(query.Get());
        const auto pixels = target.Read();
        unsigned redPixels = 0;
        for (const auto& pixel : pixels) if (pixel[0] == 1 && pixel[3] == 1) ++redPixels;
        assert(redPixels > 0);
        D3D11_QUERY_DATA_PIPELINE_STATISTICS stats{};
        assert(context->GetData(query.Get(), &stats, sizeof(stats), 0) == S_OK);
        assert(stats.IAVertices == 2 && stats.IAPrimitives == 1);
        const int finishedCalls = contextCalls;
        Geometory::DrawLines();
        assert(contextCalls == finishedCalls);
        assert(pixels == target.Read());
        std::cout << "PASS: empty lines issue no context calls; queued line draws two vertices and resets\n";
    }
}

int main()
{
    // ミカ：初期化前の空キューでもGPUや未作成資源へ触れない
    Geometory::DrawLines();
    assert(contextCalls == 0);
    const HRESULT deviceResult = D3D11CreateDevice(nullptr, D3D_DRIVER_TYPE_WARP,
        nullptr, 0, nullptr, 0, D3D11_SDK_VERSION, device.GetAddressOf(), nullptr,
        context.GetAddressOf());
    assert(SUCCEEDED(deviceResult));

    constexpr const char* vertexSource = R"(
cbuffer TransformBuffer { float4x4 transform; };
float4 main(float3 position : POSITION) : SV_POSITION {
    return mul(float4(position, 1), transform);
})";
    constexpr const char* pixelSource = R"(
cbuffer ColorBuffer { float4 color; };
float4 main() : SV_TARGET { return color; }
)";

    for (int cycle = 0; cycle < 20; ++cycle)
    {
        VertexShader vertex;
        PixelShader pixel;
        assert(SUCCEEDED(vertex.Compile(vertexSource)));
        assert(SUCCEEDED(pixel.Compile(pixelSource)));
        CheckReleasedReflections(3);
    }

    reflectionCalls = 0;
    failReflectionCall = 2;
    {
        VertexShader vertex;
        assert(FAILED(vertex.Compile(vertexSource)));
        CheckReleasedReflections(1);
    }
    failReflectionCall = 0;
    {
        PixelShader pixel;
        assert(FAILED(pixel.Compile("this is not HLSL")));
        CheckReleasedReflections(0);
    }

    CheckSparseBindings();

    // ミカ：実際のCPUコピーとGPUバッファを作り、通常終了と空の解放経路を通す
    {
        MeshBuffer empty;
        MeshBuffer mesh;
        const float vertices[] = {0, 0, 0, 1, 0, 0, 0, 1, 0};
        const unsigned short indices[] = {0, 1, 2};
        MeshBuffer::Description desc{};
        desc.pVtx = vertices;
        desc.vtxSize = 3 * sizeof(float);
        desc.vtxCount = 3;
        desc.pIdx = indices;
        desc.idxSize = sizeof(unsigned short);
        desc.idxCount = 3;
        desc.topology = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
        assert(SUCCEEDED(mesh.Create(desc)));
        const auto copy = mesh.GetDesc();
        assert(copy.pVtx != vertices && copy.pIdx != indices);
        assert(std::memcmp(copy.pVtx, vertices, sizeof(vertices)) == 0);
        assert(std::memcmp(copy.pIdx, indices, sizeof(indices)) == 0);
    }
    Geometory::Init();
    CheckReleasedReflections(6);
    CheckLines();
    Geometory::Uninit();
    Geometory::Uninit();
    context->ClearState();
    std::cout << "PASS: real shader reflection references, failure cleanup, typed array destruction\n";
}
