#include "BackgroundRenderer.h"

#include "DirectX.h"
#include "MaterialDX.h"
#include "MeshDX.h"
#include "ModelDX.h"
#include "ModelRender.h"
#include "TextureCache.h"
#include "RenderStateManager.h"
#include "Transform.h"
#include "Vector.h"

#include <array>
#include <memory>
#include <vector>

namespace
{
    std::shared_ptr<MeshDX> CreateQuadMesh(float width, float height)
    {
        if (width <= 0.0f) width = 1.0f;
        if (height <= 0.0f) height = 1.0f;

        const float halfWidth = 0.5f * width;
        const float halfHeight = 0.5f * height;
        const std::array<MeshDX::Vertex, 4> vertices{
            MeshDX::Vertex{ { -halfWidth,  halfHeight, 0.0f }, { 0.0f, 0.0f } },
            MeshDX::Vertex{ {  halfWidth,  halfHeight, 0.0f }, { 1.0f, 0.0f } },
            MeshDX::Vertex{ { -halfWidth, -halfHeight, 0.0f }, { 0.0f, 1.0f } },
            MeshDX::Vertex{ {  halfWidth, -halfHeight, 0.0f }, { 1.0f, 1.0f } },
        };

        MeshDX::Description desc{};
        desc.pVtx = vertices.data();
        desc.vtxSize = sizeof(MeshDX::Vertex);
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
    UpdateSwapChainSize();
    m_quadMesh = CreateQuadMesh(m_swapChainWidth, m_swapChainHeight);
    m_cachedMeshWidth = m_swapChainWidth > 0.0f ? m_swapChainWidth : 1.0f;
    m_cachedMeshHeight = m_swapChainHeight > 0.0f ? m_swapChainHeight : 1.0f;
    m_material = std::make_shared<MaterialDX>(
        m_config.tintColor,
        DirectX::XMFLOAT4(0.0f, 0.0f, 0.0f, 0.0f),
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

void BackgroundRenderer::Draw()
{
    if (!m_ready || !m_texture || !m_model || !m_modelRenderer)
    {
        return;
    }

    Transform transform{};
    transform.position = float3{ 0.0f, 0.0f, 0.0f };
    transform.rotation = float3{ 0.0f, 0.0f, 0.0f };
    transform.scale = float3{ 1.0f, 1.0f, 1.0f };

    struct IdentityCamera : public CameraSpace::ICamera
    {
        IdentityCamera(const DirectX::XMFLOAT4X4& view, const DirectX::XMFLOAT4X4& proj)
            : m_view(view)
            , m_proj(proj)
        {
        }

        const DirectX::XMFLOAT4X4& GetView() const override { return m_view; }
        const DirectX::XMFLOAT4X4& GetProjection() const override { return m_proj; }

    protected:
        void RecalculateView() override {}

    private:
        DirectX::XMFLOAT4X4 m_view{};
        DirectX::XMFLOAT4X4 m_proj{};
    };

    DirectX::XMFLOAT4X4 identityView{};
    DirectX::XMStoreFloat4x4(&identityView, DirectX::XMMatrixIdentity());
    UpdateSwapChainSize();
    RefreshQuadMesh();

    const DirectX::XMFLOAT4X4 ortho = MakeOrthoMatrix();

    IdentityCamera screenCamera(identityView, ortho);

    RenderStateManager::ScopedState stateGuard{};
    auto& stateManager = RenderStateManager::Instance();
    stateManager.SetDepthTest(false);
    stateManager.SetBlend(BlendMode::BLEND_ALPHA);

    m_modelRenderer->Draw(*m_model, transform, &screenCamera);
}

DirectX::XMFLOAT4X4 BackgroundRenderer::MakeOrthoMatrix() const
{
    DirectX::XMFLOAT4X4 ortho{};
    const float width = m_swapChainWidth > 0.0f ? m_swapChainWidth : 1.0f;
    const float height = m_swapChainHeight > 0.0f ? m_swapChainHeight : 1.0f;
    DirectX::XMStoreFloat4x4(
        &ortho,
        DirectX::XMMatrixOrthographicLH(width, height, 0.0f, 1.0f));
    return ortho;
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

void BackgroundRenderer::UpdateSwapChainSize()
{
    if (RenderTargetDX* rtv = GetDefaultRTV())
    {
        m_swapChainWidth = static_cast<float>(rtv->GetWidth());
        m_swapChainHeight = static_cast<float>(rtv->GetHeight());
    }
}

void BackgroundRenderer::RefreshQuadMesh()
{
    if (!m_quadMesh || !m_model)
    {
        return;
    }

    const float targetWidth = m_swapChainWidth > 0.0f ? m_swapChainWidth : 1.0f;
    const float targetHeight = m_swapChainHeight > 0.0f ? m_swapChainHeight : 1.0f;

    const bool sizeChanged = m_cachedMeshWidth != targetWidth || m_cachedMeshHeight != targetHeight;

    if (!sizeChanged)
    {
        return;
    }

    m_cachedMeshWidth = targetWidth;
    m_cachedMeshHeight = targetHeight;

    m_quadMesh = CreateQuadMesh(targetWidth, targetHeight);
    m_model = std::make_unique<Render::Model>(
        std::vector<std::shared_ptr<MeshDX>>{ m_quadMesh },
        std::vector<std::shared_ptr<MaterialDX>>{ m_material });
}
