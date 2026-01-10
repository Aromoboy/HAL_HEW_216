#include "GuidelineActor.h"

#include "AABB.h"
#include "MaterialDX.h"
#include "MeshDX.h"
#include "ModelDX.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>
#include <memory>
#include <optional>
#include <vector>

namespace
{
    constexpr const char* kGuidelineTag = "Guideline";
    const DirectX::XMFLOAT4 kGuidelineColor{ 0.80f, 0.65f, 0.20f, 0.8f };

    std::optional<AABB> ComputeBounds(const std::vector<MeshDX::Vertex>& vertices)
    {
        if (vertices.empty())
        {
            return std::nullopt;
        }

        float3 min
        {
            std::numeric_limits<float>::max(),
            std::numeric_limits<float>::max(),
            std::numeric_limits<float>::max(),
        };
        float3 max
        {
            std::numeric_limits<float>::lowest(),
            std::numeric_limits<float>::lowest(),
            std::numeric_limits<float>::lowest(),
        };

        for (const auto& vertex : vertices)
        {
            min.x = std::min(min.x, vertex.pos.x);
            min.y = std::min(min.y, vertex.pos.y);
            min.z = std::min(min.z, vertex.pos.z);

            max.x = std::max(max.x, vertex.pos.x);
            max.y = std::max(max.y, vertex.pos.y);
            max.z = std::max(max.z, vertex.pos.z);
        }

        return AABB(max, min);
    }

    std::shared_ptr<Render::Model> CreateGuidelineModel()
    {
        static std::weak_ptr<Render::Model> cached;
        if (auto reused = cached.lock())
        {
            return reused;
        }

        std::vector<MeshDX::Vertex> vertices;
        vertices.reserve(8);

        const std::array<DirectX::XMFLOAT2, 4> uvs{
            DirectX::XMFLOAT2(0, 1), DirectX::XMFLOAT2(1, 1),
            DirectX::XMFLOAT2(1, 0), DirectX::XMFLOAT2(0, 0)
        };

        auto addVertex = [&](const DirectX::XMFLOAT3& pos, const DirectX::XMFLOAT3& normal,
            const DirectX::XMFLOAT2& uv, const DirectX::XMFLOAT4& color)
            {
                MeshDX::Vertex v{};
                v.pos = pos;
                v.normal = normal;
                v.uv = uv;
                v.color = color;
                for (float& w : v.weight) w = 0.0f;
                for (unsigned& i : v.index) i = 0;
                vertices.push_back(v);
            };

        const std::array<DirectX::XMFLOAT3, 4> quad{
            DirectX::XMFLOAT3(-0.5f, -0.5f, 0.0f),
            DirectX::XMFLOAT3(0.5f, -0.5f, 0.0f),
            DirectX::XMFLOAT3(0.5f, 0.5f, 0.0f),
            DirectX::XMFLOAT3(-0.5f, 0.5f, 0.0f)
        };

        const DirectX::XMFLOAT3 normalFront{ 0.0f, 0.0f, 1.0f };
        const DirectX::XMFLOAT3 normalBack{ 0.0f, 0.0f, -1.0f };

        for (size_t i = 0; i < quad.size(); ++i)
        {
            addVertex(quad[i], normalFront, uvs[i], kGuidelineColor);
        }
        for (size_t i = 0; i < quad.size(); ++i)
        {
            addVertex(quad[i], normalBack, uvs[i], kGuidelineColor);
        }

        std::vector<unsigned long> indices;
        indices.reserve(12);
        const unsigned long front[] = { 0, 1, 2, 0, 2, 3 };
        const unsigned long back[] = { 4, 6, 5, 4, 7, 6 };
        indices.insert(indices.end(), std::begin(front), std::end(front));
        indices.insert(indices.end(), std::begin(back), std::end(back));

        MeshDX::Description desc{};
        desc.pVtx = vertices.data();
        desc.vtxSize = sizeof(MeshDX::Vertex);
        desc.vtxCount = static_cast<UINT>(vertices.size());
        desc.pIdx = indices.data();
        desc.idxSize = sizeof(unsigned long);
        desc.idxCount = static_cast<UINT>(indices.size());
        desc.topology = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;

        auto mesh = std::make_shared<MeshDX>(desc, 0);
        if (!mesh->GetVertexBuffer().IsValid() || !mesh->HasIndexBuffer())
        {
            return nullptr;
        }

        auto material = std::make_shared<MaterialDX>(
            kGuidelineColor,
            kGuidelineColor,
            DirectX::XMFLOAT4(0.1f, 0.1f, 0.1f, 1.0f),
            nullptr);

        auto model = std::make_shared<ModelDX>(
            std::vector<std::shared_ptr<MeshDX>>{ mesh },
            std::vector<std::shared_ptr<MaterialDX>>{ material });
        const auto bounds = ComputeBounds(vertices);
        if (bounds)
        {
            model->SetLocalBounds(*bounds);
        }

        cached = model;
        return model;
    }
}

GuidelineActor::GuidelineActor()
    : Entities::Entity(Transform{}, CreateGuidelineModel(), Core::UpdatePhase::Gameplay, 0, kGuidelineTag)
{
}

GuidelineActor::GuidelineActor(std::shared_ptr<Render::Model> model)
    : Entities::Entity(Transform{}, std::move(model), Core::UpdatePhase::Gameplay, 0, kGuidelineTag)
{
}

void GuidelineActor::UpdateGuideline(float centerX, float height, float planeZ, float width, float thickness)
{
    const float safeWidth = (std::isfinite(width) && width > 0.0f) ? width : 0.01f;
    const float safeThickness = (std::isfinite(thickness) && thickness > 0.0f) ? thickness : 0.01f;
    float3 scale{ safeWidth, safeThickness, 1.0f };
    const Render::Model* model = GetRenderableModel();
    if (model && model->HasLocalBounds())
    {
        const AABB& bounds = model->GetLocalBounds();
        const float3 size
        {
            bounds.max.x - bounds.min.x,
            bounds.max.y - bounds.min.y,
            bounds.max.z - bounds.min.z,
        };
        if (std::isfinite(size.x) && size.x > 0.0f
            && std::isfinite(size.y) && size.y > 0.0f)
        {
            // Scale relative to the model's local bounding box so width/thickness match the model extents.
            scale = float3{ safeWidth / size.x, safeThickness / size.y, 1.0f };
        }
    }

    m_transform.position = float3{ centerX, height, planeZ };
    m_transform.rotation = float3{ 30.0f, 0.0f, 0.0f };
    m_transform.scale = scale;
}
