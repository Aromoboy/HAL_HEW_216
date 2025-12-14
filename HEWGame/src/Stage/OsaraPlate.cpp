#include "Stage/OsaraPlate.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <optional>
#include <sstream>
#include <utility>
#include <vector>

#if defined(_WIN32) && defined(_DEBUG)
#    include <windows.h>
#endif

#include "AABB.h"
#include "BoxColliderComponent.h"
#include "CollisionLayer.h"
#include "ModelCache.h"
#include "ModelDX.h"
#include "ModelPivot.h"
#include "Physics/ShapeConstants.h"
#include "Systems/PhysicsMachine.h"

namespace
{
    constexpr const char* kLogPrefix = "[OsaraPlate] ";
    constexpr const char* kPlateModelPath = "Assets/Model/Osara.fbx";
    constexpr float kScaleReferenceMeters = 1.0f;
    constexpr float kMinWorldScale = 0.02f;
    constexpr float kMaxWorldScale = 50.0f;

#if defined(_WIN32) && defined(_DEBUG)
    void DebugTrace(const std::string& message)
    {
        ::OutputDebugStringA(message.c_str());
    }
#else
    void DebugTrace(const std::string& message)
    {
        (void)message;
    }
#endif

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

    float ComputeNormalizationScale(const Render::Model& model)
    {
        if (!model.HasLocalBounds())
        {
            return 1.0f;
        }

        const AABB bounds = model.GetLocalBounds();
        const float3 extents = bounds.Extents();
        const float largestDimension = std::max({ extents.x, extents.y, extents.z }) * 2.0f;
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
            oss << kLogPrefix << "[ScaleNorm] plate extents(full)=(" << (extents.x * 2.0f) << ',' << (extents.y * 2.0f)
                << ',' << (extents.z * 2.0f) << ") raw=" << rawScale
                << " referenceMeters=" << kScaleReferenceMeters << "\n";
            DebugTrace(oss.str());
        }

        return rawScale;
    }

    float SanitizeDimensionUsingBox2D(float worldLength)
    {
        const float minimum = Entities::PhysicalEntity::ToBox2DLength(Physics::kMinimumShapeDimensionMeters);
        const float box2dLength = Entities::PhysicalEntity::ToBox2DLength(std::max(std::fabs(worldLength), Physics::kMinimumShapeDimensionMeters));
        return std::max(box2dLength, minimum) * Entities::PhysicalEntity::GetBox2DToWorldScale();
    }

    Physics::BoxColliderComponent BuildPlateCollider(const float3& worldScale)
    {
        Physics::BoxColliderComponent component{};
        component.shapeDef.density = 0.0f;
        component.shapeDef.material.friction = 0.7f;
        component.shapeDef.material.restitution = 0.0f;

        const float halfWidth = SanitizeDimensionUsingBox2D(worldScale.x * 0.5f);
        const float rimHeight = SanitizeDimensionUsingBox2D(worldScale.y * 0.15f);
        const float footHalfWidth = SanitizeDimensionUsingBox2D(worldScale.x * 0.20f);
        const float footHeight = SanitizeDimensionUsingBox2D(worldScale.y * 0.20f);

        // Rim / plate surface
        component.AddPolygon({
            b2Vec2{ -halfWidth, 0.0f },
            b2Vec2{ halfWidth, 0.0f },
            b2Vec2{ halfWidth * 0.85f, rimHeight },
            b2Vec2{ -halfWidth * 0.85f, rimHeight },
        });

        // Foot
        component.AddPolygon({
            b2Vec2{ -footHalfWidth, -footHeight },
            b2Vec2{ footHalfWidth, -footHeight },
            b2Vec2{ footHalfWidth * 1.1f, 0.0f },
            b2Vec2{ -footHalfWidth * 1.1f, 0.0f },
        });

        return component;
    }
}

namespace Stage
{
    OsaraPlate::OsaraPlate(const std::string& tag,
        const Transform& anchor,
        Systems::PhysicsMachine* physicsMachine,
        Core::UpdatePhase phase,
        int priority)
        : Entities::PhysicalEntity(tag,
            anchor.position,
            anchor.rotation,
            ClampWorldScale(anchor.scale),
            std::nullopt,
            Collision::CollisionLayer::Floor,
            physicsMachine,
            phase,
            priority)
        , m_anchorTransform(anchor)
    {
        m_worldScale = ClampWorldScale(anchor.scale);
        m_anchorTransform.scale = m_worldScale;

        LoadPlateModel();
        m_normalizationScale = (m_model) ? ComputeNormalizationScale(*m_model) : 1.0f;

        RebuildCollider();

        SetGravityScale(0.0f);
        SetCollisionLayer(Collision::CollisionLayer::Floor);
        SyncVisualTransform();
        SyncColliderBounds();

#if defined(_DEBUG)
        DebugTrace(std::string(kLogPrefix) + "Initialized Osara plate.\n");
#endif
    }

    void OsaraPlate::SetAnchorTransform(const Transform& transform)
    {
        const bool scaleChanged = (transform.scale.x != m_anchorTransform.scale.x)
            || (transform.scale.y != m_anchorTransform.scale.y)
            || (transform.scale.z != m_anchorTransform.scale.z);

        m_anchorTransform = transform;
        m_worldScale = ClampWorldScale(m_anchorTransform.scale);
        m_anchorTransform.scale = m_worldScale;

        if (scaleChanged)
        {
            RebuildCollider();
        }

        SetTransform(m_anchorTransform);
        SyncBox2DBodyFromTransform();
        SyncVisualTransform();
    }

    const Transform& OsaraPlate::GetAnchorTransform() const noexcept
    {
        return m_anchorTransform;
    }

    void OsaraPlate::OnUpdate(Core::UpdateContext& ctx)
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

    Transform OsaraPlate::GetRenderTransform() const
    {
        return m_renderTransform;
    }

    Render::Model* OsaraPlate::GetRenderableModel()
    {
        return m_model.get();
    }

    const Render::Model* OsaraPlate::GetRenderableModel() const
    {
        return m_model.get();
    }

    void OsaraPlate::LoadPlateModel()
    {
        Render::ModelCache& cache = Render::ModelCache::Instance();
        m_model = cache.Get(kPlateModelPath);
        if (!m_model)
        {
            std::ostringstream oss;
            oss << kLogPrefix << "Failed to load render model at '" << kPlateModelPath << "'.\n";
            DebugTrace(oss.str());
            return;
        }

        if (!m_model->HasLocalBounds())
        {
            std::ostringstream oss;
            oss << kLogPrefix << "Render model missing local bounds; scale normalization may be skipped.\n";
            DebugTrace(oss.str());
        }
        else
        {
            const auto bounds = m_model->GetLocalBounds();
            const float3 extents = bounds.Extents();
            std::ostringstream oss;
            oss << kLogPrefix << "Loaded plate model with extents(full)=(" << (extents.x * 2.0f) << ','
                << (extents.y * 2.0f) << ',' << (extents.z * 2.0f) << ").\n";
            DebugTrace(oss.str());
        }
    }

    void OsaraPlate::SyncVisualTransform()
    {
        m_renderTransform.position = m_anchorTransform.position;
        m_renderTransform.rotation = m_anchorTransform.rotation;
        m_renderTransform.scale = float3{
            m_worldScale.x * m_normalizationScale,
            m_worldScale.y * m_normalizationScale,
            m_worldScale.z * m_normalizationScale,
        };

        if (m_model)
        {
            const float3 visualScale = m_renderTransform.scale;
            const Render::PivotInfo pivotInfo = Render::ComputePivotInfoOriginBottom(m_model, visualScale);
            m_visualPivotOffset = pivotInfo.offsetY;
            m_renderTransform.position.y += m_visualPivotOffset;
        }

        RefreshRenderTransform();
    }

    void OsaraPlate::RebuildCollider()
    {
        Physics::BoxColliderComponent colliderComponent = BuildPlateCollider(m_worldScale);
        Physics::Box2DShapeConfig shapeConfig{};
        shapeConfig.type = Physics::Box2DShapeConfig::Type::Polygon;
        shapeConfig.bodyType = b2_staticBody;
        shapeConfig.density = 0.0f;
        shapeConfig.friction = colliderComponent.shapeDef.material.friction;
        shapeConfig.restitution = colliderComponent.shapeDef.material.restitution;
        shapeConfig.fixedRotation = true;
        shapeConfig.allowRotation = false;
        shapeConfig.angularDamping = 0.0f;

        SetBoxColliderComponent(std::move(colliderComponent));
        SetBox2DShapeConfig(shapeConfig);
        AttachPhysicsMachine(GetPhysicsMachine());
        SyncColliderBounds();
    }

    void OsaraPlate::RefreshRenderTransform()
    {
        // Keep render transform isolated from collider updates.
        m_renderTransform = Transform{ m_renderTransform.position, m_renderTransform.rotation, m_renderTransform.scale };
    }
}

