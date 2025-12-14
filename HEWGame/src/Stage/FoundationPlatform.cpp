#include "Stage/FoundationPlatform.h"

#include <algorithm>
#include <cmath>
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
#include "ModelPivot.h"
#include "BoxColliderComponent.h"
#include "Systems/PhysicsMachine.h"

#if defined(_WIN32) && defined(_DEBUG)
#    include <windows.h>
#endif

namespace
{
    void DebugTrace(const std::string& message)
    {
#if defined(_WIN32) && defined(_DEBUG)
        ::OutputDebugStringA(message.c_str());
#else
        (void)message;
#endif
    }

    constexpr const char* kLogPrefix = "[FoundationPlatform] ";
    constexpr const char* kPlatformModelPath = "Assets/Model/cake_high.fbx";
    // モデルを正規化し、最大の完全な次元がこの基準サイズに一致するようにする
    constexpr float kScaleReferenceMeters = 1.0f;
    constexpr float kMinWorldScale = 0.02f;
    constexpr float kMaxWorldScale = 50.0f;
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

    Physics::BoxColliderComponent CreateColliderFromBounds(const Transform& transform, const AABB& bounds, float pivotOffsetNormalizedY)
    {
        (void)transform;

        Physics::BoxColliderComponent component;
        component.shapeDef.density = 0.0f;
        component.shapeDef.material.friction = 0.8f;

        const float3 center = bounds.Center();
        const float3 extents = bounds.Extents();
        // 生のメッシュ範囲を保持する。PhysicsMachine::CreateBodyはBox2Dフィクスチャ構築時にエンティティのtransform.scaleを適用する
        const float halfWidth  = std::max(0.01f, extents.x);
        const float halfHeight = std::max(0.01f, extents.y);

        component.AddBox(
            halfWidth,
            halfHeight,
            b2Vec2{ center.x, center.y + pivotOffsetNormalizedY }
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

    float ComputeNormalizationScale(const Render::Model& model)
    {
        if (!model.HasLocalBounds())
        {
            return 1.0f;
        }

        const AABB bounds = model.GetLocalBounds();
        const float3 extents = bounds.Extents();
        const float largestDimension = std::max({ extents.x, extents.y, extents.z }) * 2.0f; // ハーフエクステントをフルサイズに変換する
        if (!std::isfinite(largestDimension) || largestDimension <= 0.0f)
        {
            return 1.0f;
        }

        const float rawScale = kScaleReferenceMeters / largestDimension;
        if (!std::isfinite(rawScale) || rawScale <= 0.0f)
        {
            return 1.0f;
        }

        if (std::fabs(rawScale - 1.0f) > 0.01f)
        {
            std::ostringstream oss;
            oss << kLogPrefix << "[ScaleNorm] platform extents(full)=(" << (extents.x * 2.0f) << ',' << (extents.y * 2.0f)
                << ',' << (extents.z * 2.0f) << ") raw=" << rawScale
                << " referenceMeters=" << kScaleReferenceMeters << "\n";
            DebugTrace(oss.str());
        }

        return rawScale;
    }

    float3 ClampWorldScale(const float3& scale)
    {
        float3 clamped{
            std::clamp(scale.x, kMinWorldScale, kMaxWorldScale),
            std::clamp(scale.y, kMinWorldScale, kMaxWorldScale),
            std::clamp(scale.z, kMinWorldScale, kMaxWorldScale),
        };

        const bool changed = (clamped.x != scale.x) || (clamped.y != scale.y) || (clamped.z != scale.z);
        if (changed)
        {
            std::ostringstream oss;
            oss << kLogPrefix << "[ScaleClamp] world scale from (" << scale.x << ',' << scale.y << ',' << scale.z
                << ") to (" << clamped.x << ',' << clamped.y << ',' << clamped.z << ") bounds=["
                << kMinWorldScale << ',' << kMaxWorldScale << "]\n";
            DebugTrace(oss.str());
        }

        return clamped;
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

        m_normalizationScale = (m_model) ? ComputeNormalizationScale(*m_model) : 1.0f;

        m_worldScale = ClampWorldScale(transform.scale);
        m_anchorTransform.scale = m_worldScale;

        const float3 visualScale{
            m_worldScale.x * m_normalizationScale,
            m_worldScale.y * m_normalizationScale,
            m_worldScale.z * m_normalizationScale,
        };
        const Render::PivotInfo pivotInfo = Render::ComputePivotInfoOriginBottom(m_model, visualScale);
        if (pivotInfo.offsetY != 0.0f)
        {
            m_visualPivotOffset = pivotInfo.offsetY;

            std::ostringstream oss;
            oss << kLogPrefix << "[Pivot] policy=" << Render::kPivotPolicyName
                << " offsetY(raw/clamped)=" << pivotInfo.rawOffsetY << '/' << pivotInfo.offsetY
                << " halfHeight=" << pivotInfo.halfHeight
                << " scaleY=" << visualScale.y << '\n';
            DebugTrace(oss.str());
        }

        Physics::BoxColliderComponent colliderComponent = boxCollider.value_or(Physics::BoxColliderComponent{});
        if (!boxCollider)
        {
            colliderComponent.shapeDef.material.friction = 0.8f;
        }
        colliderComponent.shapeDef.density = 0.0f;

        std::ostringstream oss;
        oss << kLogPrefix << "Platform collider uses descriptor bounds; model bounds reserved for visual normalization.\n";
        DebugTrace(oss.str());

        const float pivotOffsetNormalized = (visualScale.y != 0.0f) ? (pivotInfo.offsetY / visualScale.y) : 0.0f;
        colliderComponent = CreateColliderFromBounds(m_anchorTransform, m_descriptorLocalBounds, pivotOffsetNormalized);

        SetCollisionLayer(Collision::CollisionLayer::Floor);
        SetDebugColor(m_debugColor);
        SetAnchorTransform(m_anchorTransform);

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
        SyncVisualTransform();
        DebugTrace(std::string(kLogPrefix) + "Initialized foundation platform.\n");
    }

    void FoundationPlatform::SetAnchorTransform(const Transform& transform)
    {
        m_anchorTransform = transform;
        SetTransform(m_anchorTransform);
        SyncBox2DBodyFromTransform();
        SyncVisualTransform();

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
        if (HasBox2DBody())
        {
            SyncTransformFromBox2DBody();
        }
        else
        {
            SetTransform(m_anchorTransform);
        }

        SyncVisualTransform();
    }

    Transform FoundationPlatform::GetRenderTransform() const
    {
        return m_renderTransform;
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

    void FoundationPlatform::SyncVisualTransform()
    {
        m_worldScale = ClampWorldScale(m_transform.scale);
        m_visual.position = m_transform.position;
        m_visual.rotation = m_transform.rotation;
        m_visual.scale = float3{
            m_worldScale.x * m_normalizationScale,
            m_worldScale.y * m_normalizationScale,
            m_worldScale.z * m_normalizationScale,
        };
        m_visual.position.y += m_visualPivotOffset;
        RefreshRenderTransform();
    }

    void FoundationPlatform::RefreshRenderTransform()
    {
        // 描画時に再構築せずに、レンダリング呼び出しを同期化された視覚変換と整合させる
        m_renderTransform = Transform{ m_visual.position, m_visual.rotation, m_visual.scale };
    }
}
