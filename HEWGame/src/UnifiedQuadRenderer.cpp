#include "UnifiedQuadRenderer.h"

#include "DirectX.h"
#include "MaterialDX.h"
#include "MeshDX.h"
#include "ModelDX.h"
#include "ModelRender.h"
#include "RenderStateManager.h"
#include <array>
#include <vector>

namespace
{
    std::shared_ptr<MeshDX> CreateQuadMesh(const DirectX::XMFLOAT2& uvStart, const DirectX::XMFLOAT2& uvScale)
    {
        const std::array<MeshDX::Vertex, 4> vertices{
            MeshDX::Vertex{ DirectX::XMFLOAT3{ -0.5f,  0.5f, 0.0f }, DirectX::XMFLOAT2{ uvStart.x, uvStart.y } },
            MeshDX::Vertex{ DirectX::XMFLOAT3{  0.5f,  0.5f, 0.0f }, DirectX::XMFLOAT2{ uvStart.x + uvScale.x, uvStart.y } },
            MeshDX::Vertex{ DirectX::XMFLOAT3{ -0.5f, -0.5f, 0.0f }, DirectX::XMFLOAT2{ uvStart.x, uvStart.y + uvScale.y } },
            MeshDX::Vertex{ DirectX::XMFLOAT3{  0.5f, -0.5f, 0.0f }, DirectX::XMFLOAT2{ uvStart.x + uvScale.x, uvStart.y + uvScale.y } },
        };

        MeshDX::Description desc{};
        desc.pVtx = vertices.data();
        desc.vtxSize = sizeof(MeshDX::Vertex);
        desc.vtxCount = static_cast<UINT>(vertices.size());
        desc.topology = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP;

        return std::make_shared<MeshDX>(desc, 0);
    }
}

bool UnifiedQuadRenderer::UvKey::operator<(const UvKey& other) const
{
    if (startX != other.startX) return startX < other.startX;
    if (startY != other.startY) return startY < other.startY;
    if (sizeX != other.sizeX) return sizeX < other.sizeX;
    return sizeY < other.sizeY;
}

void UnifiedQuadRenderer::ScreenSpaceCamera::UpdateProjection(float width, float height)
{
    if (width == m_width && height == m_height)
    {
        return;
    }

    m_width = width;
    m_height = height;

    DirectX::XMStoreFloat4x4(&m_view, DirectX::XMMatrixIdentity());
    DirectX::XMStoreFloat4x4(&m_proj, DirectX::XMMatrixOrthographicOffCenterLH(0.0f, width, height, 0.0f, 0.0f, 1.0f));
}

UnifiedQuadRenderer::UnifiedQuadRenderer()
    : m_modelRenderer(std::make_unique<ModelRender>())
{
    ModelRender::EnsureDefaultShaders();
}

UnifiedQuadRenderer::~UnifiedQuadRenderer() = default;

bool UnifiedQuadRenderer::Draw(const DrawParams& params)
{
    if (!params.texture || !m_modelRenderer)
    {
        return false;
    }

    float viewportWidth = 0.0f;
    float viewportHeight = 0.0f;
    if (!TryGetViewportSize(viewportWidth, viewportHeight))
    {
        return false;
    }

    m_camera.UpdateProjection(viewportWidth, viewportHeight);

    const auto mesh = GetQuadMesh(params.uvStart, params.uvSize);
    if (!mesh)
    {
        return false;
    }

    auto material = std::make_shared<MaterialDX>(
        params.tint,
        DirectX::XMFLOAT4(0.0f, 0.0f, 0.0f, 0.0f),
        DirectX::XMFLOAT4(0.0f, 0.0f, 0.0f, 0.0f),
        params.texture);

    Render::Model model(
        std::vector<std::shared_ptr<MeshDX>>{ mesh },
        std::vector<std::shared_ptr<MaterialDX>>{ material });

    RenderStateManager::ScopedState stateGuard{};
    auto& stateManager = RenderStateManager::Instance();
    stateManager.SetDepthTest(!params.disableDepth);
    stateManager.SetBlend(params.enableBlending ? BlendMode::BLEND_ALPHA : BlendMode::BLEND_NONE);
    const Transform transform = BuildTransform(params);
    m_modelRenderer->Draw(model, transform, &m_camera, params.addressMode);
    return true;
}

bool UnifiedQuadRenderer::TryGetViewportSize(float& width, float& height) const
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

Transform UnifiedQuadRenderer::BuildTransform(const DrawParams& params) const
{
    Transform transform{};
    transform.position = float3{ params.positionPx.x + (params.sizePx.x * 0.5f), params.positionPx.y + (params.sizePx.y * 0.5f), 0.0f };
    transform.rotation = float3{ 0.0f, 0.0f, 0.0f };
    transform.scale = float3{ params.sizePx.x, params.sizePx.y, 1.0f };
    return transform;
}

std::shared_ptr<MeshDX> UnifiedQuadRenderer::GetQuadMesh(const DirectX::XMFLOAT2& uvStart, const DirectX::XMFLOAT2& uvSize)
{
    const UvKey key{ uvStart.x, uvStart.y, uvSize.x, uvSize.y };
    if (auto cached = m_quadCache[key].lock())
    {
        return cached;
    }

    auto mesh = CreateQuadMesh(uvStart, uvSize);
    m_quadCache[key] = mesh;
    return mesh;
}

