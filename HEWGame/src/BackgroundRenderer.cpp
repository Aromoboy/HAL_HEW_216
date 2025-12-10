#include "BackgroundRenderer.h"

#include "DirectX.h"
#include "ICamera.h"
#include "MaterialDX.h"
#include "MeshDX.h"
#include "ModelDX.h"
#include "ModelRender.h"
#include "TextureCache.h"
#include "Transform.h"
#include "Vector.h"

#include <array>
#include <memory>
#include <vector>

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

    std::shared_ptr<MeshDX> CreateQuadMesh()
    {
        struct QuadVertex
        {
            DirectX::XMFLOAT3 pos;
            DirectX::XMFLOAT2 uv;
            DirectX::XMFLOAT3 normal;
        };

        const std::array<QuadVertex, 4> vertices{
            QuadVertex{ { -0.5f,  0.5f, 0.0f }, { 0.0f, 0.0f }, { 0.0f, 0.0f, -1.0f } },
            QuadVertex{ {  0.5f,  0.5f, 0.0f }, { 1.0f, 0.0f }, { 0.0f, 0.0f, -1.0f } },
            QuadVertex{ { -0.5f, -0.5f, 0.0f }, { 0.0f, 1.0f }, { 0.0f, 0.0f, -1.0f } },
            QuadVertex{ {  0.5f, -0.5f, 0.0f }, { 1.0f, 1.0f }, { 0.0f, 0.0f, -1.0f } },
        };

        MeshDX::Description desc{};
        desc.pVtx = vertices.data();
        desc.vtxSize = sizeof(QuadVertex);
        desc.vtxCount = static_cast<UINT>(vertices.size());
        desc.topology = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP;

        return std::make_shared<MeshDX>(desc, 0);
    }
}

BackgroundRenderer::BackgroundRenderer() = default;

BackgroundRenderer::~BackgroundRenderer()
{
    Shutdown();
}

bool BackgroundRenderer::Initialize(const BackgroundRendererConfig& config)
{
    m_config = config;
    if (m_config.texturePath.empty())
    {
        return false;
    }

    if (!LoadTexture(m_config.texturePath))
    {
        return false;
    }

    m_modelRenderer = std::make_unique<ModelRender>();
    m_quadMesh = CreateQuadMesh();
    m_material = std::make_shared<MaterialDX>(
        m_config.tintColor,
        DirectX::XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f),
        DirectX::XMFLOAT4(0.0f, 0.0f, 0.0f, 0.0f),
        m_texture);
    m_model = std::make_unique<Render::Model>(
        std::vector<std::shared_ptr<MeshDX>>{ m_quadMesh },
        std::vector<std::shared_ptr<MaterialDX>>{ m_material });

    m_ready = true;
    return true;
}

void BackgroundRenderer::Shutdown()
{
    m_texture.reset();
    m_model.reset();
    m_material.reset();
    m_quadMesh.reset();
    m_modelRenderer.reset();
    m_ready = false;
}

void BackgroundRenderer::Draw() const
{
    if (!m_ready || !m_texture || !m_model || !m_modelRenderer)
    {
        return;
    }

    SetDepthTest(false);
    SetBlendMode(BLEND_ALPHA);

    static IdentityCamera identityCamera{};
    Transform transform{};
    transform.position = float3{ 0.0f, 0.0f, 0.0f };
    transform.rotation = float3{ 0.0f, 0.0f, 0.0f };
    transform.scale = float3{ 2.0f, 2.0f, 1.0f };

    m_modelRenderer->Draw(*m_model, transform, &identityCamera);

    SetDepthTest(true);
}

bool BackgroundRenderer::LoadTexture(const std::string& path)
{
    const auto texture = TextureCache::Instance().Load(
        TextureCache::MakeFileKey(path),
        GetDevice(),
        std::wstring(path.begin(), path.end()));
    m_texture = texture;
    return m_texture != nullptr;
}
