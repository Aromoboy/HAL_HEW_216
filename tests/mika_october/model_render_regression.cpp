#include "ModelRender.h"
#include "ModelDX.h"
#include "ShaderList.h"
#include "DrawMatrix.h"
#include "Animation.h"
#include "DirectXTex.h"
#include <assimp/cimport.h>
#include <assimp/scene.h>
#include <assimp/postprocess.h>
#include <wrl/client.h>
#include <wincodec.h>
#include <algorithm>
#include <array>
#include <cassert>
#include <chrono>
#include <cmath>
#include <filesystem>
#include <functional>
#include <iostream>
#include <limits>
#include <string>
#include <vector>

// ミカ：変更前の実装も同じデバイスとシェーダーで描画して画素を比較する
class BaselineModelRender
{
public:
    BaselineModelRender();
    ~BaselineModelRender();
    void Draw(Render::Model&, Transform, const CameraSpace::ICamera*, Animation*);
private:
    void MeshDraw(MeshDX&);
};

namespace
{
    using Microsoft::WRL::ComPtr;
    ComPtr<ID3D11Device> device;
    ComPtr<ID3D11DeviceContext> context;
    ComPtr<ID3D11Texture2D> target;
    ComPtr<ID3D11RenderTargetView> targetView;
    ComPtr<ID3D11DepthStencilView> depthView;
    constexpr UINT width = 1280, height = 720;
    std::filesystem::path output;

    void Require(HRESULT result)
    {
        if (FAILED(result)) { std::cerr << "HRESULT " << std::hex << result << '\n'; std::abort(); }
    }

    class Camera : public CameraSpace::ICamera
    {
    public:
        DirectX::XMFLOAT4X4 view{}, projection{};
        Camera()
        {
            DirectX::XMStoreFloat4x4(&view, DirectX::XMMatrixIdentity());
            DrawMatrix::CreateProjectionMatrix(projection, DirectX::XM_PIDIV2, float(width) / height, 1, 100);
        }
        const DirectX::XMFLOAT4X4& GetView() const override { return view; }
        const DirectX::XMFLOAT4X4& GetProjection() const override { return projection; }
        void RecalculateView() override {}
    } camera;

    ComPtr<ID3D11Query> Query(D3D11_QUERY kind)
    {
        D3D11_QUERY_DESC desc{kind, 0};
        ComPtr<ID3D11Query> query;
        Require(device->CreateQuery(&desc, &query));
        return query;
    }

    template<class T> T Read(ID3D11Query* query)
    {
        T value{};
        HRESULT result;
        while ((result = context->GetData(query, &value, sizeof(value), 0)) == S_FALSE) Sleep(0);
        Require(result);
        return value;
    }

    struct Measurement
    {
        double cpu = 0, gpu = 0;
        UINT64 vertices = 0, primitives = 0;
    };

    Measurement Frame(const std::function<void()>& draw)
    {
        ID3D11RenderTargetView* view = targetView.Get();
        context->OMSetRenderTargets(1, &view, depthView.Get());
        const float clear[] = {0.03f, 0.05f, 0.08f, 1};
        context->ClearRenderTargetView(view, clear);
        context->ClearDepthStencilView(depthView.Get(), D3D11_CLEAR_DEPTH, 1, 0);
        auto disjoint = Query(D3D11_QUERY_TIMESTAMP_DISJOINT);
        auto start = Query(D3D11_QUERY_TIMESTAMP);
        auto end = Query(D3D11_QUERY_TIMESTAMP);
        auto statistics = Query(D3D11_QUERY_PIPELINE_STATISTICS);
        context->Begin(disjoint.Get());
        context->Begin(statistics.Get());
        context->End(start.Get());
        const auto cpuStart = std::chrono::steady_clock::now();
        draw();
        const auto cpuEnd = std::chrono::steady_clock::now();
        context->End(end.Get());
        context->End(statistics.Get());
        context->End(disjoint.Get());
        const auto frequency = Read<D3D11_QUERY_DATA_TIMESTAMP_DISJOINT>(disjoint.Get());
        const auto from = Read<UINT64>(start.Get()), to = Read<UINT64>(end.Get());
        const auto stats = Read<D3D11_QUERY_DATA_PIPELINE_STATISTICS>(statistics.Get());
        assert(!frequency.Disjoint && frequency.Frequency);
        return {std::chrono::duration<double, std::milli>(cpuEnd - cpuStart).count(),
            1000.0 * double(to - from) / double(frequency.Frequency), stats.VSInvocations, stats.IAPrimitives};
    }

    std::vector<uint8_t> Pixels(const std::string& name = {})
    {
        DirectX::ScratchImage image;
        Require(DirectX::CaptureTexture(device.Get(), context.Get(), target.Get(), image));
        const auto* frame = image.GetImage(0, 0, 0);
        if (!name.empty())
            Require(DirectX::SaveToWICFile(*frame, DirectX::WIC_FLAGS_NONE,
                DirectX::GetWICCodec(DirectX::WIC_CODEC_PNG), (output / (name + ".png")).c_str(),
                &GUID_WICPixelFormat24bppBGR));
        std::vector<uint8_t> pixels;
        for (UINT y = 0; y < height; ++y)
            pixels.insert(pixels.end(), frame->pixels + frame->rowPitch * y,
                frame->pixels + frame->rowPitch * y + width * 4);
        return pixels;
    }

    std::shared_ptr<MaterialDX> Material()
    {
        return std::make_shared<MaterialDX>(DirectX::XMFLOAT4{0.8f, 0.5f, 0.2f, 1},
            DirectX::XMFLOAT4{0.4f, 0.4f, 0.4f, 1}, DirectX::XMFLOAT4{0, 0, 0, 1}, nullptr);
    }

    std::shared_ptr<MeshDX> Triangle(bool bone, float x = 0)
    {
        MeshDX::Vertex vertices[3]{};
        vertices[0].pos = {x - 0.75f, -0.75f, 0};
        vertices[1].pos = {x, 0.75f, 0};
        vertices[2].pos = {x + 0.75f, -0.75f, 0};
        for (auto& vertex : vertices)
        {
            vertex.normal = {0, 0, -1}; vertex.color = {1, 1, 1, 1};
            vertex.weight[0] = bone ? 1.0f : 0.0f;
        }
        MeshDX::Description desc{};
        desc.pVtx = vertices; desc.vtxSize = sizeof(vertices[0]); desc.vtxCount = 3;
        if (bone)
        {
            BoneDX value{}; value.nodeIndex = 0;
            DirectX::XMStoreFloat4x4(&value.invOffset, DirectX::XMMatrixIdentity());
            desc.bones.push_back(value);
        }
        return std::make_shared<MeshDX>(desc, 0);
    }

    std::shared_ptr<ModelDX> MakeModel(std::shared_ptr<MeshDX> mesh, std::optional<AABB> bounds)
    {
        return std::make_shared<ModelDX>(std::vector{mesh}, std::vector{Material()}, nullptr, bounds);
    }

    std::shared_ptr<ModelDX> Fruit(const char* name)
    {
        const auto path = std::string("Assets/Model/") + name + ".fbx";
        const aiScene* scene = aiImportFile(path.c_str(), aiProcess_Triangulate | aiProcess_GenSmoothNormals);
        assert(scene && scene->mNumMeshes == 1);
        const aiMesh& source = *scene->mMeshes[0];
        std::vector<MeshDX::Vertex> vertices(source.mNumVertices);
        AABB bounds{{-FLT_MAX, -FLT_MAX, -FLT_MAX}, {FLT_MAX, FLT_MAX, FLT_MAX}};
        for (UINT i = 0; i < source.mNumVertices; ++i)
        {
            const auto& p = source.mVertices[i];
            vertices[i].pos = {p.x, p.y, p.z};
            const auto& n = source.mNormals[i]; vertices[i].normal = {n.x, n.y, n.z};
            vertices[i].color = {1, 1, 1, 1};
            bounds.min.x = std::min(bounds.min.x, p.x); bounds.max.x = std::max(bounds.max.x, p.x);
            bounds.min.y = std::min(bounds.min.y, p.y); bounds.max.y = std::max(bounds.max.y, p.y);
            bounds.min.z = std::min(bounds.min.z, p.z); bounds.max.z = std::max(bounds.max.z, p.z);
        }
        const auto center = bounds.Center();
        const float span = std::max({bounds.max.x - bounds.min.x, bounds.max.y - bounds.min.y, bounds.max.z - bounds.min.z});
        assert(span > 0);
        for (auto& vertex : vertices)
        {
            vertex.pos.x = (vertex.pos.x - center.x) / span;
            vertex.pos.y = (vertex.pos.y - center.y) / span;
            vertex.pos.z = (vertex.pos.z - center.z) / span;
        }
        bounds.min = {(bounds.min.x - center.x) / span, (bounds.min.y - center.y) / span, (bounds.min.z - center.z) / span};
        bounds.max = {(bounds.max.x - center.x) / span, (bounds.max.y - center.y) / span, (bounds.max.z - center.z) / span};
        std::vector<UINT> indices;
        for (UINT face = 0; face < source.mNumFaces; ++face)
            indices.insert(indices.end(), source.mFaces[face].mIndices, source.mFaces[face].mIndices + 3);
        MeshDX::Description desc{};
        desc.pVtx = vertices.data(); desc.vtxSize = sizeof(vertices[0]); desc.vtxCount = UINT(vertices.size());
        desc.pIdx = indices.data(); desc.idxSize = sizeof(UINT); desc.idxCount = UINT(indices.size());
        auto result = MakeModel(std::make_shared<MeshDX>(desc, 0), bounds);
        std::cout << "ASSET " << name << " vertices=" << vertices.size() << " triangles=" << indices.size() / 3 << '\n';
        aiReleaseImport(scene);
        return result;
    }

    void Compare(const char* name, ModelDX& model, Transform transform, Animation* animation,
        bool expectCulled, bool save = false)
    {
        BaselineModelRender before;
        ModelRender after;
        const auto baseline = Frame([&] { before.Draw(model, transform, &camera, animation); });
        const auto pixels = Pixels(save ? std::string(name) + "_before" : "");
        if (std::string(name) == "center" || std::string(name) == "animated_bounds_are_not_culled")
        {
            bool covered = false;
            for (std::size_t i = 4; i < pixels.size(); i += 4)
                covered |= pixels[i] != pixels[0] || pixels[i + 1] != pixels[1] || pixels[i + 2] != pixels[2];
            assert(covered);
        }
        const auto current = Frame([&] { after.Draw(model, transform, &camera, animation); });
        assert(pixels == Pixels(save ? std::string(name) + "_after" : ""));
        assert(baseline.vertices > 0);
        assert(expectCulled ? current.vertices == 0 : current.vertices == baseline.vertices);
        std::cout << "PASS " << name << " exact_pixels vertices=" << baseline.vertices << "->" << current.vertices << '\n';
    }
}

ID3D11Device* GetDevice() { return device.Get(); }
ID3D11DeviceContext* GetContext() { return context.Get(); }
IDXGISwapChain* GetSwapChain() { return nullptr; }
namespace DebugLog { void DebugMessage(const std::string& message) { std::cerr << message; } }

int main(int argc, char** argv)
{
    assert(argc >= 2);
    output = std::filesystem::absolute(argv[1]);
    std::filesystem::create_directories(output);
    Require(CoInitializeEx(nullptr, COINIT_MULTITHREADED));
    const auto driver = argc > 2 && std::string(argv[2]) == "warp" ? D3D_DRIVER_TYPE_WARP : D3D_DRIVER_TYPE_HARDWARE;
    Require(D3D11CreateDevice(nullptr, driver, nullptr, 0, nullptr, 0, D3D11_SDK_VERSION, &device, nullptr, &context));
    ComPtr<IDXGIDevice> dxgi; Require(device.As(&dxgi));
    ComPtr<IDXGIAdapter> adapter; Require(dxgi->GetAdapter(&adapter));
    DXGI_ADAPTER_DESC adapterDesc{}; Require(adapter->GetDesc(&adapterDesc));
    const std::wstring adapterName(adapterDesc.Description);
    std::cout << "DEVICE " << std::string(adapterName.begin(), adapterName.end()) << '\n';
    D3D11_TEXTURE2D_DESC desc{};
    desc.Width = width; desc.Height = height; desc.MipLevels = 1; desc.ArraySize = 1;
    desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM; desc.SampleDesc.Count = 1;
    desc.Usage = D3D11_USAGE_DEFAULT; desc.BindFlags = D3D11_BIND_RENDER_TARGET;
    Require(device->CreateTexture2D(&desc, nullptr, &target));
    Require(device->CreateRenderTargetView(target.Get(), nullptr, &targetView));
    desc.Format = DXGI_FORMAT_D32_FLOAT; desc.BindFlags = D3D11_BIND_DEPTH_STENCIL;
    ComPtr<ID3D11Texture2D> depth; Require(device->CreateTexture2D(&desc, nullptr, &depth));
    Require(device->CreateDepthStencilView(depth.Get(), nullptr, &depthView));
    D3D11_VIEWPORT viewport{0, 0, float(width), float(height), 0, 1}; context->RSSetViewports(1, &viewport);
    D3D11_RASTERIZER_DESC raster{}; raster.FillMode = D3D11_FILL_SOLID;
    raster.CullMode = D3D11_CULL_NONE; raster.DepthClipEnable = TRUE;
    ComPtr<ID3D11RasterizerState> rasterState; Require(device->CreateRasterizerState(&raster, &rasterState));
    context->RSSetState(rasterState.Get());
    ShaderList::Init();
    ShaderList::SetLightingMode(1);
    ShaderList::SetCameraPos({0, 0, 0});
    {
        const AABB bounds{{0.75f, 0.75f, 0}, {-0.75f, -0.75f, 0}};
        auto triangle = MakeModel(Triangle(false), bounds);
        const Transform center{{0, 0, 5}, {}, {1, 1, 1}};
        Compare("center", *triangle, center, nullptr, false, true);
        for (const auto& position : std::array<float3, 6>{{{-100, 0, 5}, {100, 0, 5},
            {0, -100, 5}, {0, 100, 5}, {0, 0, 0.1f}, {0, 0, 110}}})
            Compare("outside_plane", *triangle, {position, {}, {1, 1, 1}}, nullptr, true);
        for (const auto& position : std::array<float3, 4>{{{-8.9f, 0, 5}, {8.9f, 0, 5}, {0, -5, 5}, {0, 5, 5}}})
            Compare("edge_overlap", *triangle, {position, {}, {2, 2, 2}}, nullptr, false);
        Compare("rotated_negative_scale", *triangle, {{8.9f, 0, 5}, {0.1f, 0.3f, 0.8f}, {-2, 3, 0.5f}}, nullptr, false, true);
        Compare("near_plane_overlap", *triangle, {{0, 0, 1}, {0.8f, 0, 0}, {1, 1, 1}}, nullptr, false);
        Compare("far_plane_overlap", *triangle, {{0, 0, 100}, {0.8f, 0, 0}, {1, 1, 1}}, nullptr, false);
        DrawMatrix::CreateViewMatrix(camera.view, {3, 2, -1}, {0, 0, 5}, {0, 1, 0});
        Compare("camera_pan_and_rotation", *triangle, center, nullptr, false);
        Compare("camera_pan_outside", *triangle, {{-100, -100, 5}, {}, {1, 1, 1}}, nullptr, true);
        DirectX::XMStoreFloat4x4(&camera.view, DirectX::XMMatrixIdentity());
        const auto projection = camera.projection;
        camera.projection._11 = NAN;
        Compare("nonfinite_camera_fail_open", *triangle, center, nullptr, false);
        camera.projection = projection;
        auto unknown = MakeModel(Triangle(false), std::nullopt);
        Compare("unknown_bounds", *unknown, center, nullptr, false);
        auto invalid = MakeModel(Triangle(false), AABB{{0, 0, 0}, {1, 1, 1}});
        Compare("invalid_bounds", *invalid, center, nullptr, false);
        auto nan = MakeModel(Triangle(false), AABB{{NAN, 0, 0}, {-1, -1, -1}});
        Compare("nonfinite_bounds", *nan, center, nullptr, false);

        NodeDX node{}; node.mat = DirectX::XMMatrixTranslation(-100, 0, 0);
        Animation animation({}, {}, {}, {node});
        auto animated = MakeModel(Triangle(true, 100), AABB{{101, 1, 0}, {99, -1, 0}});
        Compare("animated_bounds_are_not_culled", *animated, center, &animation, false, true);
        auto staticMesh = Triangle(false, 2);
        auto mixed = std::make_shared<ModelDX>(std::vector{animated->GetMesh(0).lock(), staticMesh}, std::vector{Material()});
        auto staticModel = MakeModel(staticMesh, std::nullopt);
        ModelRender renderer;
        Frame([&] { renderer.Draw(*animated, center, &camera, &animation); renderer.Draw(*staticModel, center, &camera, nullptr); });
        const auto separate = Pixels();
        Frame([&] { renderer.Draw(*mixed, center, &camera, &animation); });
        assert(separate == Pixels("mixed_meshes"));
        std::cout << "PASS mixed_meshes static_shader_restored\n";

        std::vector<std::shared_ptr<ModelDX>> fruits;
        for (const char* name : {"cream", "strawberry", "Kiwi_High", "Mikan_high", "Masukatoo_high",
            "meron_high", "blueberry", "pineapple", "banana"}) fruits.push_back(Fruit(name));
        for (std::size_t i = 0; i < fruits.size(); ++i)
        {
            Compare("fruit_visible", *fruits[i], center, nullptr, false);
            Compare("fruit_edge_rotation", *fruits[i], {{8.8f, 0, 5}, {0.3f, 0.9f, 1.2f}, {3, -2, 1}}, nullptr, false);
        }

        // ミカ：実FBXの形状を同じ配置で比較し、描画だけの時間をゲーム全体のFPSと混同しない
        for (int count : {100, 500, 1000})
        {
            std::vector<Transform> transforms;
            for (int i = 0; i < count; ++i)
            {
                const int slot = i % 50;
                transforms.push_back({{float(slot % 10 - 5) * 0.9f, i < 50 ? float(slot / 10 - 2) * 0.9f : -100.0f - i,
                    10.0f}, {0.1f, 0.4f, float(i % 6)}, {1, 1, 1}});
            }
            BaselineModelRender before;
            const auto draw = [&](bool original)
            {
                for (int i = 0; i < count; ++i)
                    if (original) before.Draw(*fruits[i % fruits.size()], transforms[i], &camera, nullptr);
                    else renderer.Draw(*fruits[i % fruits.size()], transforms[i], &camera, nullptr);
            };
            Frame([&] { draw(true); }); const auto beforePixels = Pixels(count == 1000 ? "fruits1000_before" : "");
            Frame([&] { draw(false); }); assert(beforePixels == Pixels(count == 1000 ? "fruits1000_after" : ""));
            for (int warm = 0; warm < 4; ++warm) { Frame([&] { draw(true); }); Frame([&] { draw(false); }); }
            std::array<std::vector<double>, 4> samples;
            std::array<Measurement, 2> last{};
            for (int sample = 0; sample < 21; ++sample)
                for (int side = 0; side < 2; ++side)
                {
                    last[side] = Frame([&] { draw(side == 0); });
                    samples[side * 2].push_back(last[side].cpu); samples[side * 2 + 1].push_back(last[side].gpu);
                }
            for (auto& values : samples) std::sort(values.begin(), values.end());
            for (int side = 0; side < 2; ++side)
                std::cout << "BENCH count=" << count << " visible=50 side=" << (side ? "after" : "before")
                    << " cpu_median_ms=" << samples[side * 2][10] << " cpu_p95_ms=" << samples[side * 2][19]
                    << " gpu_median_ms=" << samples[side * 2 + 1][10] << " gpu_p95_ms=" << samples[side * 2 + 1][19]
                    << " vertices=" << last[side].vertices << " triangles=" << last[side].primitives << '\n';
            assert(last[1].vertices < last[0].vertices);
        }
    }
    context->ClearState();
    ShaderList::Uninit();
    std::cout << "PASS actual model renderer: pixels, six planes, rotation, scale, animated path, mixed meshes, real fruit benchmark\n";
    CoUninitialize();
}
