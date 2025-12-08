#include "Stage/FoundationPlatform.h"

#include <algorithm>
#include <optional>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

#include "Collision2D.h"
#include "MaterialDX.h"
#include "MeshDX.h"
#include "ModelCache.h"
#include "ModelDX.h"
#include "BoxColliderComponent.h"
#include "Systems/PhysicsMachine.h"

namespace
{
    void DebugTrace(const std::string& message)
    {
        (void)message;
    }

    constexpr const char* kLogPrefix = "[FoundationPlatform] ";
    constexpr const char* kPlatformModelPath = "Assets/Model/cake_high.fbx";
    DirectX::XMFLOAT4 SanitizeColor(const DirectX::XMFLOAT4& color)
    {
        auto clampComponent = [](float value)
        {
            return std::clamp(value, 0.0f, 1.0f);
        };
        return DirectX::XMFLOAT4(
            clampComponent(color.x),
            clampComponent(color.y),
            clampComponent(color.z),
            clampComponent(color.w));
    }

    struct ModelCloneStats
    {
        unsigned int meshCount{ 0 };
        unsigned int materialCount{ 0 };
        unsigned int nullMeshes{ 0 };
        unsigned int nullMaterials{ 0 };
    };

    Physics::BoxColliderComponent CreateColliderFromBounds(const Transform& transform, const AABB& bounds)
    {
        (void)transform;

        Physics::BoxColliderComponent component;
        component.shapeDef.density = 0.0f;
        component.shapeDef.material.friction = 0.8f;

        const float3 center = bounds.Center();
        const float3 extents = bounds.Extents();
        // Preserve raw mesh extents; PhysicsMachine::CreateBody applies the entity's transform.scale
        // when it builds Box2D fixtures.
        const float halfWidth  = std::max(0.01f, extents.x);
        const float halfHeight = std::max(0.01f, extents.y);

        component.AddBox(
            halfWidth,
            halfHeight,
            b2Vec2{ center.x, center.y }
        );

        return component;
    }

    AABB BuildDescriptorLocalBounds(const Transform& transform)
    {
        (void)transform;

        constexpr float kHalfExtent = 0.5f;
        return AABB(
            float3{ kHalfExtent, kHalfExtent, kHalfExtent },
            float3{ -kHalfExtent, -kHalfExtent, -kHalfExtent });
    }

    std::vector<std::shared_ptr<MeshDX>> CopyMeshes(const ModelDX& source, unsigned int& maxMaterialId, unsigned int& nullMeshes)
    {
        const unsigned int meshCount = source.GetMeshCount();
        std::vector<std::shared_ptr<MeshDX>> meshes;
        meshes.reserve(meshCount);

        maxMaterialId = 0;
        nullMeshes = 0;
        for (unsigned int meshIndex = 0; meshIndex < meshCount; ++meshIndex)
        {
            auto mesh = source.GetMesh(static_cast<int>(meshIndex)).lock();
            if (mesh)
            {
                maxMaterialId = std::max(maxMaterialId, mesh->GetMaterialID());
            }
            else
            {
                ++nullMeshes;
            }
            meshes.push_back(mesh);
        }

        return meshes;
    }

    std::vector<std::shared_ptr<MaterialDX>> CopyMaterials(const ModelDX& source, unsigned int materialCount, unsigned int& nullMaterials)
    {
        std::vector<std::shared_ptr<MaterialDX>> materials;
        materials.reserve(materialCount);

        nullMaterials = 0;
        for (unsigned int materialIndex = 0; materialIndex < materialCount; ++materialIndex)
        {
            auto baseMaterial = source.GetMaterial(static_cast<int>(materialIndex)).lock();
            if (!baseMaterial)
            {
                ++nullMaterials;
                materials.push_back(nullptr);
                continue;
            }

            materials.push_back(std::make_shared<MaterialDX>(
                baseMaterial->GetDiffuse(),
                baseMaterial->GetAmbient(),
                baseMaterial->GetSpecular(),
                baseMaterial->GetTextureResource()));
        }

        return materials;
    }

    std::shared_ptr<ModelDX> CloneModelWithUniqueMaterials(const ModelDX& source, ModelCloneStats& stats)
    {
        unsigned int maxMaterialId = 0;
        stats.meshCount = source.GetMeshCount();

        std::vector<std::shared_ptr<MeshDX>> meshes = CopyMeshes(source, maxMaterialId, stats.nullMeshes);
        std::vector<std::shared_ptr<MaterialDX>> materials = CopyMaterials(source, maxMaterialId + 1, stats.nullMaterials);
        stats.materialCount = static_cast<unsigned int>(materials.size());

        std::optional<AABB> clonedBounds = source.HasLocalBounds() ? std::optional<AABB>(source.GetLocalBounds()) : std::nullopt;
        return std::make_shared<ModelDX>(std::move(meshes), std::move(materials), clonedBounds);
    }
}

namespace Stage
{
    FoundationPlatform::FoundationPlatform(const std::string& tag,
        const Transform& transform,
        std::optional<Physics::BoxColliderComponent> boxCollider,
        Systems::PhysicsMachine* physicsMachine,
        const DirectX::XMFLOAT4& color,
        Core::UpdatePhase phase,
        int priority)
        : Entities::PhysicalEntity(tag,
            transform.position,
            transform.rotation,
            transform.scale,
            std::nullopt,
            Collision::CollisionLayer::Floor,
            phase,
            priority)
        , m_anchorTransform(transform)
        , m_model(nullptr)
        , m_debugColor(SanitizeColor(color))
        , m_descriptorLocalBounds(BuildDescriptorLocalBounds(transform))
    {
        LoadPlatformModel();

        Physics::BoxColliderComponent colliderComponent = boxCollider.value_or(Physics::BoxColliderComponent{});
        if (!boxCollider)
        {
            colliderComponent.shapeDef.material.friction = 0.8f;
        }
        colliderComponent.shapeDef.density = 0.0f;

        const bool modelHasBounds = m_model && m_model->HasLocalBounds();
        if (modelHasBounds)
        {
            colliderComponent = CreateColliderFromBounds(transform, m_model->GetLocalBounds());
        }
        else
        {
            std::ostringstream oss;
            oss << kLogPrefix << "Platform model is unavailable or missing local bounds; using descriptor dimensions for collider.\n";
            DebugTrace(oss.str());

            colliderComponent = CreateColliderFromBounds(transform, m_descriptorLocalBounds);
        }

        SetCollisionLayer(Collision::CollisionLayer::Floor);
        SetDebugColor(m_debugColor);
        SetAnchorTransform(transform);

        Physics::Box2DShapeConfig shapeConfig{};
        shapeConfig.type = Physics::Box2DShapeConfig::Type::Box;
        shapeConfig.bodyType = b2_staticBody;
        shapeConfig.density = 0.0f;
        shapeConfig.friction = colliderComponent.shapeDef.material.friction;
        shapeConfig.restitution = colliderComponent.shapeDef.material.restitution;

        SetBox2DShapeConfig(shapeConfig);
        SetBoxColliderComponent(std::move(colliderComponent));
        AttachPhysicsMachine(physicsMachine);
        SetGravityScale(0.0f);
        DebugTrace(std::string(kLogPrefix) + "Initialized foundation platform.\n");
    }

    void FoundationPlatform::SetAnchorTransform(const Transform& transform)
    {
        m_anchorTransform = transform;
        SetTransform(m_anchorTransform);

#if defined(_DEBUG)
        std::ostringstream oss;
        oss << kLogPrefix << "Anchor transform updated for platform '" << GetTag() << "'.\n";
        DebugTrace(oss.str());
#endif
    }

    const Transform& FoundationPlatform::GetAnchorTransform() const noexcept
    {
        return m_anchorTransform;
    }

    void FoundationPlatform::OnUpdate(Core::UpdateContext& ctx)
    {
        (void)ctx;
        SetTransform(m_anchorTransform);
    }

    Render::Model* FoundationPlatform::GetRenderableModel()
    {
        return m_model.get();
    }

    const Render::Model* FoundationPlatform::GetRenderableModel() const
    {
        return m_model.get();
    }

    void FoundationPlatform::SetDebugColor(const DirectX::XMFLOAT4& color)
    {
        const DirectX::XMFLOAT4 sanitized = SanitizeColor(color);
        m_debugColor = sanitized;
        ApplyColorToModelMaterials();

        std::ostringstream oss;
        oss << kLogPrefix << "Debug color set to RGBA("
            << m_debugColor.x << ',' << m_debugColor.y << ',' << m_debugColor.z << ',' << m_debugColor.w << ")";
        if (!m_model)
        {
            oss << " but model not yet available.";
        }
        oss << "\n";
        DebugTrace(oss.str());
    }

    const DirectX::XMFLOAT4& FoundationPlatform::GetDebugColor() const noexcept
    {
        return m_debugColor;
    }

    void FoundationPlatform::LoadPlatformModel()
    {
        Render::ModelCache& cache = Render::ModelCache::Instance();
        std::shared_ptr<Render::Model> baseModel = cache.Get(kPlatformModelPath);
        if (!baseModel)
        {
            std::ostringstream oss;
            oss << kLogPrefix << "Failed to load render model at '" << kPlatformModelPath << "'.\n";
            DebugTrace(oss.str());
            return;
        }

        if (!baseModel->HasLocalBounds())
        {
            std::ostringstream oss;
            oss << kLogPrefix << "Render model missing local bounds; will fallback to descriptor dimensions.\n";
            DebugTrace(oss.str());
        }

        ModelCloneStats cloneStats{};
        m_model = CloneModelWithUniqueMaterials(*baseModel, cloneStats);

        if (!m_model)
        {
            std::ostringstream oss;
            oss << kLogPrefix << "Failed to clone platform model for unique materials.\n";
            DebugTrace(oss.str());
            return;
        }

        if (!m_model->HasLocalBounds())
        {
            m_model->SetLocalBounds(m_descriptorLocalBounds);

            const float3 extents = m_descriptorLocalBounds.Extents();
            std::ostringstream oss;
            oss << kLogPrefix << "Applied descriptor local bounds extents ("
                << extents.x << ',' << extents.y << ',' << extents.z
                << ") to platform model due to missing FBX bounds.\n";
            DebugTrace(oss.str());
        }

        std::ostringstream oss;
        oss << kLogPrefix << "Loaded platform model from '" << kPlatformModelPath
            << "' (meshes=" << m_model->GetMeshCount()
            << ", materials=" << cloneStats.materialCount
            << ", nullMeshes=" << cloneStats.nullMeshes
            << ", nullMaterials=" << cloneStats.nullMaterials << ").\n";
        DebugTrace(oss.str());
    }

    void FoundationPlatform::ApplyColorToModelMaterials()
    {
        if (!m_model)
        {
            return;
        }

        const unsigned int meshCount = m_model->GetMeshCount();
        unsigned int appliedCount = 0;
        unsigned int skippedMeshes = 0;
        for (unsigned int meshIndex = 0; meshIndex < meshCount; ++meshIndex)
        {
            auto mesh = m_model->GetMesh(static_cast<int>(meshIndex)).lock();
            if (!mesh)
            {
                ++skippedMeshes;
                continue;
            }

            const int materialIndex = static_cast<int>(mesh->GetMaterialID());
            auto material = m_model->GetMaterial(materialIndex).lock();
            if (!material)
            {
                ++skippedMeshes;
                continue;
            }

            material->SetDiffuse(m_debugColor);
            DirectX::XMFLOAT4 ambient = m_debugColor;
            ambient.w = 1.0f;
            material->SetAmbient(ambient);
            ++appliedCount;
        }

        std::ostringstream oss;
        oss << kLogPrefix << "Applied debug color to " << appliedCount << '/' << meshCount
            << " materials (" << skippedMeshes << " skipped).\n";
        DebugTrace(oss.str());
    }
}
