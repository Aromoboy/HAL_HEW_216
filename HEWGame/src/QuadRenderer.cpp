#include "QuadRenderer.h"

#include "MaterialDX.h"
#include "MeshDX.h"
#include "ModelDX.h"
#include "ModelRender.h"
#include "TextureDX.h"

#include <array>
#include <vector>

namespace
{
    std::shared_ptr<MeshDX> CreateQuadMesh()
    {
        const std::array<MeshDX::Vertex, 4> vertices{
            MeshDX::Vertex{ DirectX::XMFLOAT3{ -0.5f,  0.5f, 0.0f }, DirectX::XMFLOAT2{ 0.0f, 0.0f } },
            MeshDX::Vertex{ DirectX::XMFLOAT3{  0.5f,  0.5f, 0.0f }, DirectX::XMFLOAT2{ 1.0f, 0.0f } },
            MeshDX::Vertex{ DirectX::XMFLOAT3{ -0.5f, -0.5f, 0.0f }, DirectX::XMFLOAT2{ 0.0f, 1.0f } },
            MeshDX::Vertex{ DirectX::XMFLOAT3{  0.5f, -0.5f, 0.0f }, DirectX::XMFLOAT2{ 1.0f, 1.0f } },
        };

        MeshDX::Description desc{};
        desc.pVtx = vertices.data();
        desc.vtxSize = sizeof(MeshDX::Vertex);
        desc.vtxCount = static_cast<UINT>(vertices.size());
        desc.topology = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP;

        return std::make_shared<MeshDX>(desc, 0);
    }
}

QuadRenderer::QuadRenderer() = default;

QuadRenderer::QuadRenderer(std::shared_ptr<MaterialDX> material)
    : m_material(std::move(material))
{
}

QuadRenderer::~QuadRenderer() = default;

void QuadRenderer::SetMaterial(std::shared_ptr<MaterialDX> material)
{
    m_material = std::move(material);
}

bool QuadRenderer::SetTexture(const std::shared_ptr<Texture2D>& texture, const DirectX::XMFLOAT4& tint)
{
    if (!texture)
    {
        return false;
    }

    m_material = std::make_shared<MaterialDX>(
        tint,
        DirectX::XMFLOAT4(0.0f, 0.0f, 0.0f, 0.0f),
        DirectX::XMFLOAT4(0.0f, 0.0f, 0.0f, 0.0f),
        texture);
    return true;
}

std::shared_ptr<MaterialDX> QuadRenderer::GetMaterial() const
{
    return m_material;
}

void QuadRenderer::Draw(ModelRender& renderer, const Transform& transform, const CameraSpace::ICamera* camera) const
{
    const auto quadMesh = GetSharedQuadMesh();
    if (!quadMesh || !m_material)
    {
        return;
    }

    Render::Model model(
        std::vector<std::shared_ptr<MeshDX>>{ quadMesh },
        std::vector<std::shared_ptr<MaterialDX>>{ m_material });

    renderer.Draw(model, transform, camera);
}

std::shared_ptr<MeshDX> QuadRenderer::GetSharedQuadMesh()
{
    static std::weak_ptr<MeshDX> weakMesh;
    auto mesh = weakMesh.lock();
    if (mesh)
    {
        return mesh;
    }

    mesh = CreateQuadMesh();
    weakMesh = mesh;
    return mesh;
}

