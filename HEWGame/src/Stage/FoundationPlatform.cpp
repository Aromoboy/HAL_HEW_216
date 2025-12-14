#include "Stage/FoundationPlatform.h"

#include <algorithm>
#include <cmath>
#include <optional>
#include <limits>
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

    AABB ComputeWorldAABB(const AABB& localBounds, const Transform& transform)
    {
        const float3 localMin{
            std::min(localBounds.min.x, localBounds.max.x),
            std::min(localBounds.min.y, localBounds.max.y),
            std::min(localBounds.min.z, localBounds.max.z),
        };
        const float3 localMax{
            std::max(localBounds.min.x, localBounds.max.x),
            std::max(localBounds.min.y, localBounds.max.y),
            std::max(localBounds.min.z, localBounds.max.z),
        };

        const float3 scale{
            std::fabs(transform.scale.x),
            std::fabs(transform.scale.y),
            std::fabs(transform.scale.z),
        };

        float3 min{
            std::numeric_limits<float>::max(),
            std::numeric_limits<float>::max(),
            std::numeric_limits<float>::max(),
        };
        float3 max{
            std::numeric_limits<float>::lowest(),
            std::numeric_limits<float>::lowest(),
            std::numeric_limits<float>::lowest(),
        };

        auto accumulate = [&](float localX, float localY, float localZ)
            {
                const float3 world{
                    transform.position.x + localX * scale.x,
                    transform.position.y + localY * scale.y,
                    transform.position.z + localZ * scale.z,
                };

                min.x = std::min(min.x, world.x);
                min.y = std::min(min.y, world.y);
                min.z = std::min(min.z, world.z);
                max.x = std::max(max.x, world.x);
                max.y = std::max(max.y, world.y);
                max.z = std::max(max.z, world.z);
            };

        accumulate(localMin.x, localMin.y, localMin.z);
        accumulate(localMin.x, localMin.y, localMax.z);
        accumulate(localMin.x, localMax.y, localMin.z);
        accumulate(localMin.x, localMax.y, localMax.z);
        accumulate(localMax.x, localMin.y, localMin.z);
        accumulate(localMax.x, localMin.y, localMax.z);
        accumulate(localMax.x, localMax.y, localMin.z);
        accumulate(localMax.x, localMax.y, localMax.z);

        return AABB(max, min);
    }

    Physics::BoxColliderComponent CreateColliderFromWorldDimensions(const float3& worldScale)
    {
        Physics::BoxColliderComponent component;
        component.shapeDef.density = 0.0f;
        component.shapeDef.material.friction = 0.8f;

        const float safeWidth = std::max(0.01f, std::fabs(worldScale.x));
        const float safeHeight = std::max(0.01f, std::fabs(worldScale.y));

        // PhysicsMachine::CreateBody で transform.scale が適用されるため、ここでは world サイズを正規化して保持する
        const float halfWidth = safeWidth * 0.5f / safeWidth;
        const float halfHeight = safeHeight * 0.5f / safeHeight;

        component.AddBox(
            halfWidth,
            halfHeight,
            b2Vec2{ 0.0f, 0.0f }
        );

        return component;
    }

    AABB BuildDescriptorLocalBounds(const float3& worldDimensions)
    {
        auto computeHalfExtent = [](float worldDimension)
        {
            const float clamped = std::max(0.01f, std::fabs(worldDimension));
            return (clamped * 0.5f) / clamped;
        };

        const float3 halfExtents{
            computeHalfExtent(worldDimensions.x),
            computeHalfExtent(worldDimensions.y),
            computeHalfExtent(worldDimensions.z),
        };

        return AABB(halfExtents, float3{ -halfExtents.x, -halfExtents.y, -halfExtents.z });
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
        int priority,
        bool createCollider)
        : Entities::PhysicalEntity(tag,
            transform.position,
            transform.rotation,
            ClampWorldScale(transform.scale),
            std::nullopt,
            Collision::CollisionLayer::Floor,
            phase,
            priority)
        , m_anchorTransform(transform)
        , m_model(nullptr)
        , m_debugColor(SanitizeColor(color))
    {
        m_descriptorDimensions = float3{
            std::fabs(transform.scale.x),
            std::fabs(transform.scale.y),
            std::fabs(transform.scale.z),
        };
        m_worldScale = ClampWorldScale(m_descriptorDimensions);
        m_anchorTransform.scale = m_worldScale;
        m_descriptorLocalBounds = BuildDescriptorLocalBounds(m_descriptorDimensions);

        LoadPlatformModel();

        m_normalizationScale = (m_model) ? ComputeNormalizationScale(*m_model) : 1.0f;

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

        SetCollisionLayer(Collision::CollisionLayer::Floor);
        SetDebugColor(m_debugColor);
        SetAnchorTransform(m_anchorTransform);

        if (createCollider)
        {
            Physics::BoxColliderComponent colliderComponent{};
            if (boxCollider)
            {
                colliderComponent = *boxCollider;
            }
            else
            {
                colliderComponent = CreateColliderFromWorldDimensions(m_worldScale);
                colliderComponent.shapeDef.material.friction = 0.8f;
            }

            colliderComponent.shapeDef.density = 0.0f;

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
        }
        else
        {
            SetBox2DShapeConfig(std::nullopt);
            SetBoxColliderComponent(std::nullopt);
        }

        SyncVisualTransform();
        SyncColliderBounds();

#if defined(_DEBUG)
        std::ostringstream boundsOss;
        const Transform renderTransform = GetRenderTransform();
        const bool hasModelBounds = m_model && m_model->HasLocalBounds();
        const AABB& visualSourceBounds = hasModelBounds ? m_model->GetLocalBounds() : m_descriptorLocalBounds;
        const AABB visualWorldBounds = ComputeWorldAABB(visualSourceBounds, renderTransform);
        const float3 visualSize{
            visualWorldBounds.max.x - visualWorldBounds.min.x,
            visualWorldBounds.max.y - visualWorldBounds.min.y,
            visualWorldBounds.max.z - visualWorldBounds.min.z,
        };

        const AABB colliderBounds = GetAABB();
        const float3 colliderSize{
            colliderBounds.max.x - colliderBounds.min.x,
            colliderBounds.max.y - colliderBounds.min.y,
            colliderBounds.max.z - colliderBounds.min.z,
        };

        boundsOss << kLogPrefix << "[WorldAABB] visualSize="
            << visualSize.x << ',' << visualSize.y << ',' << visualSize.z
            << " minY=" << visualWorldBounds.min.y
            << " colliderSize="
            << colliderSize.x << ',' << colliderSize.y << ',' << colliderSize.z
            << " minY=" << colliderBounds.min.y
            << " source=" << (hasModelBounds ? "model" : "descriptor")
            << '\n';
        DebugTrace(boundsOss.str());
#endif
        DebugTrace(std::string(kLogPrefix) + "Initialized foundation platform.\n");
    }

    void FoundationPlatform::SetAnchorTransform(const Transform& transform)
    {
        m_anchorTransform = transform;
        const Transform alignedTransform = GetPivotAlignedTransform(m_anchorTransform);
        SetTransform(alignedTransform);
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
            SetTransform(GetPivotAlignedTransform(m_anchorTransform));
        }

        SyncVisualTransform();
    }

    Transform FoundationPlatform::GetPivotAlignedTransform(const Transform& transform) const
    {
        Transform aligned = transform;
        aligned.position.y += m_visualPivotOffset;
        return aligned;
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
        m_visual.scale = m_worldScale;
        RefreshRenderTransform();
    }

    void FoundationPlatform::RefreshRenderTransform()
    {
        // 描画時に再構築せずに、レンダリング呼び出しを同期化された視覚変換と整合させる
        m_renderTransform = Transform{ m_visual.position, m_visual.rotation, m_visual.scale };
    }
}
