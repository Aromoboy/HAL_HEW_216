// ミカ：本番の動画シェーダーを描画し、平面境界と未使用領域による色の混入を検証する
#include <windows.h>
#include <d3d11.h>
#include <d3dcompiler.h>
#include <wrl/client.h>
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <stdexcept>
#include <vector>

using Microsoft::WRL::ComPtr;

void Check(bool condition, const char* message)
{
    if (!condition) throw std::runtime_error(message);
}

ComPtr<ID3DBlob> Compile(const wchar_t* path)
{
    ComPtr<ID3DBlob> code, error;
    const HRESULT result = D3DCompileFromFile(path, nullptr, D3D_COMPILE_STANDARD_FILE_INCLUDE,
        "PS", "ps_5_0", D3DCOMPILE_OPTIMIZATION_LEVEL3, 0, &code, &error);
    if (FAILED(result) && error) std::fputs(static_cast<const char*>(error->GetBufferPointer()), stderr);
    Check(SUCCEEDED(result), "pixel shader compilation");
    return code;
}

struct Renderer
{
    ComPtr<ID3D11Device> device;
    ComPtr<ID3D11DeviceContext> context;
    ComPtr<ID3D11VertexShader> vs;
    ComPtr<ID3D11PixelShader> before, after;

    Renderer()
    {
        D3D_FEATURE_LEVEL level;
        Check(SUCCEEDED(D3D11CreateDevice(nullptr, D3D_DRIVER_TYPE_WARP, nullptr, 0,
            nullptr, 0, D3D11_SDK_VERSION, &device, &level, &context)), "WARP device creation");
        const char source[] =
            "struct Output { float4 Pos:SV_POSITION; float2 Tex:TEXCOORD0; float4 Color:COLOR; };"
            "Output VS(uint id:SV_VertexID) { Output o; float2 uv=float2((id<<1)&2,id&2);"
            "o.Pos=float4(uv.x*2-1,1-uv.y*2,0,1);o.Tex=uv;o.Color=1;return o;}";
        ComPtr<ID3DBlob> code;
        Check(SUCCEEDED(D3DCompile(source, sizeof(source) - 1, nullptr, nullptr, nullptr,
            "VS", "vs_5_0", D3DCOMPILE_OPTIMIZATION_LEVEL3, 0, &code, nullptr)), "test vertex shader compilation");
        Check(SUCCEEDED(device->CreateVertexShader(code->GetBufferPointer(), code->GetBufferSize(), nullptr, &vs)), "vertex shader creation");
        code = Compile(L"build/mika_october/mika-followup-before/HEWGame/shaders/video.hlsl");
        Check(SUCCEEDED(device->CreatePixelShader(code->GetBufferPointer(), code->GetBufferSize(), nullptr, &before)), "baseline pixel shader creation");
        code = Compile(L"HEWGame/shaders/video.hlsl");
        Check(SUCCEEDED(device->CreatePixelShader(code->GetBufferPointer(), code->GetBufferSize(), nullptr, &after)), "current pixel shader creation");
        D3D11_RASTERIZER_DESC raster{};
        raster.FillMode = D3D11_FILL_SOLID;
        raster.CullMode = D3D11_CULL_NONE;
        raster.DepthClipEnable = TRUE;
        ComPtr<ID3D11RasterizerState> state;
        Check(SUCCEEDED(device->CreateRasterizerState(&raster, &state)), "rasterizer creation");
        context->RSSetState(state.Get());
    }

    std::vector<unsigned char> Draw(bool baseline, unsigned width, unsigned height,
        unsigned outputWidth, unsigned outputHeight, std::array<unsigned char, 3> yuv,
        unsigned char poison, D3D11_TEXTURE_ADDRESS_MODE address)
    {
        std::vector<unsigned char> data(width * height * 2, poison);
        std::fill_n(data.begin(), width * height, yuv[0]);
        for (unsigned y = height; y < height * 2; ++y)
            std::fill_n(data.begin() + y * width, width / 2, y < height * 3 / 2 ? yuv[1] : yuv[2]);
        D3D11_TEXTURE2D_DESC desc{};
        desc.Width = width;
        desc.Height = height * 2;
        desc.MipLevels = desc.ArraySize = 1;
        desc.Format = DXGI_FORMAT_R8_UNORM;
        desc.SampleDesc.Count = 1;
        desc.Usage = D3D11_USAGE_IMMUTABLE;
        desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
        D3D11_SUBRESOURCE_DATA initial{data.data(), width, 0};
        ComPtr<ID3D11Texture2D> texture;
        Check(SUCCEEDED(device->CreateTexture2D(&desc, &initial, &texture)), "packed YUV texture");
        ComPtr<ID3D11ShaderResourceView> srv;
        Check(SUCCEEDED(device->CreateShaderResourceView(texture.Get(), nullptr, &srv)), "YUV view");

        desc.Width = outputWidth;
        desc.Height = outputHeight;
        desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
        desc.Usage = D3D11_USAGE_DEFAULT;
        desc.BindFlags = D3D11_BIND_RENDER_TARGET;
        ComPtr<ID3D11Texture2D> target, staging;
        Check(SUCCEEDED(device->CreateTexture2D(&desc, nullptr, &target)), "render target");
        ComPtr<ID3D11RenderTargetView> rtv;
        Check(SUCCEEDED(device->CreateRenderTargetView(target.Get(), nullptr, &rtv)), "target view");
        desc.Usage = D3D11_USAGE_STAGING;
        desc.BindFlags = 0;
        desc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
        Check(SUCCEEDED(device->CreateTexture2D(&desc, nullptr, &staging)), "readback texture");
        D3D11_SAMPLER_DESC samplerDesc{};
        samplerDesc.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
        samplerDesc.AddressU = samplerDesc.AddressV = samplerDesc.AddressW = address;
        samplerDesc.MaxLOD = D3D11_FLOAT32_MAX;
        ComPtr<ID3D11SamplerState> sampler;
        Check(SUCCEEDED(device->CreateSamplerState(&samplerDesc, &sampler)), "sampler");
        D3D11_VIEWPORT viewport{0, 0, static_cast<float>(outputWidth), static_cast<float>(outputHeight), 0, 1};
        context->RSSetViewports(1, &viewport);
        ID3D11RenderTargetView* view = rtv.Get();
        context->OMSetRenderTargets(1, &view, nullptr);
        context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
        context->VSSetShader(vs.Get(), nullptr, 0);
        context->PSSetShader(baseline ? before.Get() : after.Get(), nullptr, 0);
        ID3D11ShaderResourceView* resource = srv.Get();
        context->PSSetShaderResources(0, 1, &resource);
        ID3D11SamplerState* sampling = sampler.Get();
        context->PSSetSamplers(0, 1, &sampling);
        context->Draw(3, 0);
        context->CopyResource(staging.Get(), target.Get());
        D3D11_MAPPED_SUBRESOURCE map{};
        Check(SUCCEEDED(context->Map(staging.Get(), 0, D3D11_MAP_READ, 0, &map)), "pixel readback");
        std::vector<unsigned char> pixels(outputWidth * outputHeight * 4);
        for (unsigned y = 0; y < outputHeight; ++y)
            std::memcpy(pixels.data() + y * outputWidth * 4,
                static_cast<unsigned char*>(map.pData) + y * map.RowPitch, outputWidth * 4);
        context->Unmap(staging.Get(), 0);
        context->ClearState();
        return pixels;
    }
};

int ColorError(const std::vector<unsigned char>& pixels, const std::array<unsigned char, 3>& yuv)
{
    const float y = yuv[0] / 255.0f - 0.0625f;
    const float u = yuv[1] / 255.0f - 0.5f;
    const float v = yuv[2] / 255.0f - 0.5f;
    const std::array<float, 4> rgb{1.164f * y + 1.596f * v,
        1.164f * y - 0.392f * u - 0.813f * v, 1.164f * y + 2.017f * u, 1.0f};
    int maximum = 0;
    for (size_t i = 0; i < pixels.size(); ++i)
    {
        const int expected = static_cast<int>(std::lround(std::clamp(rgb[i % 4], 0.0f, 1.0f) * 255));
        maximum = std::max(maximum, std::abs(static_cast<int>(pixels[i]) - expected));
    }
    return maximum;
}

int main()
{
    try
    {
        Renderer renderer;
        int baselineFailures = 0;
        const std::array<std::array<unsigned, 4>, 4> sizes{{{2, 2, 17, 13}, {16, 12, 64, 48},
            {16, 12, 16, 12}, {32, 24, 9, 7}}};
        for (const auto size : sizes)
            for (const auto address : {D3D11_TEXTURE_ADDRESS_WRAP, D3D11_TEXTURE_ADDRESS_CLAMP})
                for (const auto color : {std::array<unsigned char, 3>{128, 90, 180}, {200, 128, 128}})
                {
                    const auto before = renderer.Draw(true, size[0], size[1], size[2], size[3], color, 0, address);
                    const auto dark = renderer.Draw(false, size[0], size[1], size[2], size[3], color, 0, address);
                    const auto bright = renderer.Draw(false, size[0], size[1], size[2], size[3], color, 255, address);
                    Check(dark == bright, "unused YUV texture region affects current output");
                    Check(ColorError(dark, color) <= 1, "current output has plane-edge color bleed");
                    if (ColorError(before, color) > 1) ++baselineFailures;
                }
        Check(baselineFailures > 0, "negative control did not expose original edge bleed");
        std::printf("PASS: 16 video sampling cases, native WARP, current color error <= 1; baseline failed %d cases\n", baselineFailures);
        return 0;
    }
    catch (const std::exception& error)
    {
        std::fprintf(stderr, "FAIL: %s\n", error.what());
        return 1;
    }
}
