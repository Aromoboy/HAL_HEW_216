#include "Entities/PhysicalEntity.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <memory>
#include <sstream>
#include <utility>

#include <box2d/box2d.h>
#include <box2d/math_functions.h>

#if defined(_WIN32)
#    ifndef NOMINMAX
#        define NOMINMAX
#    endif
#    include <windows.h>
#endif

#include "Systems/Physics/PhysicsMachine.h"
#include "Systems/Physics/ShapeConstants.h"

namespace
{
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

    bool AreFloat3Equal(const float3& lhs, const float3& rhs)
    {
        return lhs.x == rhs.x && lhs.y == rhs.y && lhs.z == rhs.z;
    }

    bool HasPhysicsDefinition(const Entities::PhysicalEntity& entity)
    {
        const auto& collider = entity.GetBoxColliderComponent();
        return static_cast<bool>(collider);
    }
}

namespace
{
    float gWorldToBox2DScale = 1.0f;
    float gBox2DToWorldScale = 1.0f / gWorldToBox2DScale;
}

namespace Entities
{
    PhysicalEntity::PhysicalEntity(const Transform& transform, std::optional<Physics::Box2DShapeConfig> shapeConfig, Collision::CollisionLayer layer, std::shared_ptr<Render::Model> model, Systems::PhysicsMachine* physicsMachine, Core::UpdatePhase phase, int priority, std::string tag, bool allowMeshPhysics)
        : Entity(transform, std::move(model), phase, priority, std::move(tag))
        , m_physical()
        , m_physicsMachine(physicsMachine)
        , m_collisionLayer(layer)
        , m_allowMeshPhysics(allowMeshPhysics)
    {
        m_box2dShapeConfig = std::move(shapeConfig);
        SyncColliderBounds();
        AttachPhysicsMachine(m_physicsMachine);
    }

    PhysicalEntity::PhysicalEntity(
        const std::string& tag,
        const float3& pos,
        const float3& rot,
        const float3& scl,
        std::optional<Physics::Box2DShapeConfig> shapeConfig,
        Collision::CollisionLayer layer,
        Core::UpdatePhase phase,
        int priority,
        bool allowMeshPhysics)
        : PhysicalEntity(Transform{ pos, rot, scl }, std::move(shapeConfig), layer, nullptr, nullptr, phase, priority, tag, allowMeshPhysics)
    {
    }


    PhysicalEntity::~PhysicalEntity()
    {
        DetachPhysicsMachine();
    }

    Physics::Physical& PhysicalEntity::GetPhysical()
    {
        return m_physical;
    }

    const Physics::Physical& PhysicalEntity::GetPhysical() const
    {
        return m_physical;
    }

    Systems::PhysicsMachine* PhysicalEntity::GetPhysicsMachine() const
    {
        return m_physicsMachine;
    }

    Collision::CollisionLayer PhysicalEntity::GetCollisionLayer() const
    {
        return m_collisionLayer;
    }

    void PhysicalEntity::SetCollisionLayer(Collision::CollisionLayer layer)
    {
        m_collisionLayer = layer;
    }

    void PhysicalEntity::SetTransform(const Transform& transform)
    {
        const bool positionChanged = !AreFloat3Equal(transform.position, m_transform.position);
        const bool rotationChanged = !AreFloat3Equal(transform.rotation, m_transform.rotation);
        const bool scaleChanged = !AreFloat3Equal(transform.scale, m_transform.scale);

        if (!positionChanged && !rotationChanged && !scaleChanged)
        {
            return;
        }

        MarkTransformDirty();
        m_transform = transform;
        SyncColliderBounds();
    }

    void PhysicalEntity::SetPosition(const float3& position)
    {
        if (AreFloat3Equal(position, m_transform.position))
        {
            return;
        }

        MarkTransformDirty();
        m_transform.position = position;
        SyncColliderBounds();
    }

    void PhysicalEntity::SetRotation(const float3& rotation)
    {
        if (AreFloat3Equal(rotation, m_transform.rotation))
        {
            return;
        }

        MarkTransformDirty();
        m_transform.rotation = rotation;
        SyncColliderBounds();
    }

    void PhysicalEntity::SetScale(const float3& scale)
    {
        if (AreFloat3Equal(scale, m_transform.scale))
        {
            return;
        }

        MarkTransformDirty();
        m_transform.scale = scale;
        SyncColliderBounds();
    }

    void PhysicalEntity::SetAllowMeshPhysics(bool allow)
    {
        if (m_allowMeshPhysics == allow)
        {
            return;
        }

        m_allowMeshPhysics = allow;
        RecreateBox2DBody();
    }

    void PhysicalEntity::SyncColliderBounds()
    {
        const b2BodyId bodyId = m_physicsMachine ? m_physicsMachine->FindBody(*this) : b2_nullBodyId;
        const bool validBody = b2Body_IsValid(bodyId);
        const int shapeCount = validBody ? b2Body_GetShapeCount(bodyId) : 0;
        const b2Transform bodyTransform = validBody ? b2Body_GetTransform(bodyId) : b2Transform{};
        // ミカ：実ボディと定義が同じなら、眠っている物体や同一フレームの境界再計算を省く
        if (m_bodyBoundsCached && validBody && shapeCount > 0 && !m_transformDirty
            && B2_ID_EQUALS(bodyId, m_boundsBodyId) && shapeCount == m_boundsShapeCount
            && m_boundsDefinitionRevision == m_physicsDefinitionRevision
            && m_boundsWorldScale == GetBox2DToWorldScale()
            && AreFloat3Equal(m_transform.position, m_lastBoundsTransform.position)
            && AreFloat3Equal(m_transform.rotation, m_lastBoundsTransform.rotation)
            && AreFloat3Equal(m_transform.scale, m_lastBoundsTransform.scale)
            && bodyTransform.p.x == m_boundsBodyTransform.p.x && bodyTransform.p.y == m_boundsBodyTransform.p.y
            && bodyTransform.q.c == m_boundsBodyTransform.q.c && bodyTransform.q.s == m_boundsBodyTransform.q.s)
        {
            return;
        }
        float3 min{ std::numeric_limits<float>::max(), std::numeric_limits<float>::max(), std::numeric_limits<float>::max() };
        float3 max{ std::numeric_limits<float>::lowest(), std::numeric_limits<float>::lowest(), std::numeric_limits<float>::lowest() };
        bool hasPoints = false;

        const auto accumulate = [&](float x, float y, float z)
            {
                min.x = std::min(min.x, x);
                min.y = std::min(min.y, y);
                min.z = std::min(min.z, z);
                max.x = std::max(max.x, x);
                max.y = std::max(max.y, y);
                max.z = std::max(max.z, z);
                hasPoints = true;
            };

        const auto accumulatePoint = [&](const float3& point)
            {
                accumulate(point.x, point.y, point.z);
            };

        const Transform& transform = m_transform;

        b2AABB bodyAabb{};
        if (GetBox2DBodyAABB(bodyAabb))
        {
            const float3 lower = ToWorldVector(bodyAabb.lowerBound);
            const float3 upper = ToWorldVector(bodyAabb.upperBound);
            accumulate(lower.x, lower.y, transform.position.z);
            accumulate(upper.x, upper.y, transform.position.z);
        }
        else if (m_boxColliderComponent)
        {
            for (const auto& shape : m_boxColliderComponent->shapes)
            {
                switch (shape.type)
                {
                case Physics::ColliderShapeType::Circle:
                {
                    const b2Vec2 centerOffset = shape.vertices.empty() ? b2Vec2{ 0.0f, 0.0f } : shape.vertices.front();
                    const float radius = shape.radius * std::max(transform.scale.x, transform.scale.y);
                    const float3 center{
                        transform.position.x + centerOffset.x * transform.scale.x,
                        transform.position.y + centerOffset.y * transform.scale.y,
                        transform.position.z,
                    };

                    accumulate(center.x - radius, center.y - radius, center.z);
                    accumulate(center.x + radius, center.y + radius, center.z);
                    break;
                }
                case Physics::ColliderShapeType::Box:
                case Physics::ColliderShapeType::Polygon:
                default:
                {
                    for (const auto& vertex : shape.vertices)
                    {
                        const float3 point{
                            transform.position.x + vertex.x * transform.scale.x,
                            transform.position.y + vertex.y * transform.scale.y,
                            transform.position.z,
                        };
                        accumulatePoint(point);
                    }
                    break;
                }
                }
            }
        }

        if (!hasPoints && m_box2dShapeConfig)
        {
            switch (m_box2dShapeConfig->type)
            {
            case Physics::Box2DShapeConfig::Type::Box:
            {
                const float hx = std::max(Physics::kMinimumShapeDimensionMeters, 0.5f * transform.scale.x);
                const float hy = std::max(Physics::kMinimumShapeDimensionMeters, 0.5f * transform.scale.y);
                accumulate(transform.position.x - hx, transform.position.y - hy, transform.position.z);
                accumulate(transform.position.x + hx, transform.position.y + hy, transform.position.z);
                break;
            }
            case Physics::Box2DShapeConfig::Type::Polygon:
            {
                for (auto vertex : m_box2dShapeConfig->vertices)
                {
                    const float3 point{
                        transform.position.x + vertex.x * transform.scale.x,
                        transform.position.y + vertex.y * transform.scale.y,
                        transform.position.z,
                    };
                    accumulatePoint(point);
                }
                break;
            }
            case Physics::Box2DShapeConfig::Type::Circle:
            {
                const float radius = std::max(Physics::kMinimumShapeDimensionMeters, m_box2dShapeConfig->radius * std::max(transform.scale.x, transform.scale.y));
                accumulate(transform.position.x - radius, transform.position.y - radius, transform.position.z);
                accumulate(transform.position.x + radius, transform.position.y + radius, transform.position.z);
                break;
            }
            default:
                break;
            }
        }

        if (!hasPoints)
        {
            const float hx = std::max(0.0f, transform.scale.x * 0.5f);
            const float hy = std::max(0.0f, transform.scale.y * 0.5f);
            const float hz = std::max(0.0f, transform.scale.z * 0.5f);
            accumulate(transform.position.x - hx, transform.position.y - hy, transform.position.z - hz);
            accumulate(transform.position.x + hx, transform.position.y + hy, transform.position.z + hz);
        }

        m_aabb.min = min;
        m_aabb.max = max;
        m_lastBoundsTransform = transform;
        m_transformDirty = false;
        m_bodyBoundsCached = validBody && shapeCount > 0;
        m_boundsBodyId = bodyId;
        m_boundsBodyTransform = bodyTransform;
        m_boundsDefinitionRevision = m_physicsDefinitionRevision;
        m_boundsWorldScale = GetBox2DToWorldScale();
        m_boundsShapeCount = shapeCount;
    }

    void PhysicalEntity::MarkTransformDirty()
    {
        m_lastTransform = m_transform;
        m_transformDirty = true;
    }

    bool PhysicalEntity::IsTransformDirty() const
    {
        return m_transformDirty;
    }

    void PhysicalEntity::ApplyTranslation(const float3& delta)
    {
        if (delta.x == 0.0f && delta.y == 0.0f && delta.z == 0.0f)
        {
            return;
        }

        MarkTransformDirty();
        m_transform.position.x += delta.x;
        m_transform.position.y += delta.y;
        m_transform.position.z += delta.z;
        SyncColliderBounds();
    }

    void PhysicalEntity::OnUpdate(const Core::UpdateContext& ctx)
    {
        Entity::OnUpdate(ctx);

        const bool transformChanged =
            !AreFloat3Equal(m_transform.position, m_lastBoundsTransform.position)
            || !AreFloat3Equal(m_transform.rotation, m_lastBoundsTransform.rotation)
            || !AreFloat3Equal(m_transform.scale, m_lastBoundsTransform.scale);

        if (m_transformDirty || transformChanged)
        {
            SyncColliderBounds();
        }
    }

    void PhysicalEntity::SetBox2DBodyId(b2BodyId bodyId)
    {
        m_physical.bodyId = bodyId;
    }

    b2BodyId PhysicalEntity::GetBox2DBodyId() const
    {
        return m_physical.bodyId;
    }

    bool PhysicalEntity::HasBox2DBody() const
    {
        return m_physical.HasBody();
    }

    bool PhysicalEntity::CollectBox2DShapes(std::vector<b2ShapeId>& outShapes, b2Transform& outBodyTransform, b2BodyId* outBodyId) const
    {
        Systems::PhysicsMachine* physicsMachine = GetPhysicsMachine();
        if (!physicsMachine)
        {
            return false;
        }

        const b2BodyId bodyId = physicsMachine->FindBody(*this);
        if (!b2Body_IsValid(bodyId))
        {
            return false;
        }

        const int shapeCount = b2Body_GetShapeCount(bodyId);
        if (shapeCount <= 0)
        {
            return false;
        }

        outShapes.resize(static_cast<size_t>(shapeCount));
        const int writtenShapes = b2Body_GetShapes(bodyId, outShapes.data(), shapeCount);
        outShapes.resize(static_cast<size_t>(std::max(0, writtenShapes)));

        if (outShapes.empty())
        {
            return false;
        }

        outBodyTransform = b2Body_GetTransform(bodyId);
        if (outBodyId)
        {
            *outBodyId = bodyId;
        }

        return true;
    }

    bool PhysicalEntity::GetBox2DBodyAABB(b2AABB& outAabb) const
    {
        Systems::PhysicsMachine* physicsMachine = GetPhysicsMachine();
        if (!physicsMachine)
        {
            return false;
        }

        const b2BodyId bodyId = physicsMachine->FindBody(*this);
        if (!b2Body_IsValid(bodyId) || b2Body_GetShapeCount(bodyId) <= 0)
        {
            return false;
        }

        outAabb = b2Body_ComputeAABB(bodyId);
        return true;
    }

    float3 PhysicalEntity::GetLinearVelocity() const
    {
        if (!HasBox2DBody())
        {
            return float3{ 0.0f, 0.0f, 0.0f };
        }

        return ToWorldVector(b2Body_GetLinearVelocity(m_physical.bodyId));
    }

    void PhysicalEntity::SetLinearVelocity(const float3& velocity)
    {
        if (!HasBox2DBody())
        {
            return;
        }

        b2Body_SetLinearVelocity(m_physical.bodyId, ToBox2DVector(velocity));
    }

    void PhysicalEntity::SetAngularVelocity(float radiansPerSec)
    {
        if (!HasBox2DBody())
        {
            return;
        }

        b2Body_SetAngularVelocity(m_physical.bodyId, radiansPerSec);
    }

    float PhysicalEntity::GetGravityScale() const
    {
        return HasBox2DBody() ? b2Body_GetGravityScale(m_physical.bodyId) : 0.0f;
    }

    void PhysicalEntity::SetGravityScale(float scale)
    {
        if (HasBox2DBody())
        {
            b2Body_SetGravityScale(m_physical.bodyId, scale);
        }
    }

    bool PhysicalEntity::HasBodyCreationFailed() const
    {
        return m_bodyCreationFailed;
    }

    bool PhysicalEntity::CanAttemptBodyCreation() const
    {
        return !m_bodyCreationFailed || m_bodyCreationFailedRevision != m_physicsDefinitionRevision;
    }

    void PhysicalEntity::ResetBodyCreationFailure()
    {
        m_bodyCreationFailed = false;
        m_bodyCreationFailedRevision = m_physicsDefinitionRevision;
    }

    void PhysicalEntity::MarkBodyCreationFailed()
    {
        m_bodyCreationFailed = true;
        m_bodyCreationFailedRevision = m_physicsDefinitionRevision;
    }

    void PhysicalEntity::MarkPhysicsDefinitionChanged()
    {
        ++m_physicsDefinitionRevision;
        if (m_bodyCreationFailed && m_bodyCreationFailedRevision != m_physicsDefinitionRevision)
        {
            ResetBodyCreationFailure();
        }
    }

    void PhysicalEntity::SyncTransformFromBox2DBody()
    {
        if (!HasBox2DBody())
        {
            return;
        }

        const b2Vec2 position = b2Body_GetPosition(m_physical.bodyId);
        const float angle = b2Rot_GetAngle(b2Body_GetRotation(m_physical.bodyId));

        Transform updatedTransform = m_transform;
        updatedTransform.position = ToWorldPosition(position);
        updatedTransform.rotation.z = ToWorldAngle(angle);
        SetTransform(updatedTransform);

    }

    void PhysicalEntity::SyncBox2DBodyFromTransform()
    {
        if (!HasBox2DBody())
        {
            return;
        }

        const b2Vec2 position = ToBox2DPosition(m_transform.position);
        const float angle = ToBox2DAngle(m_transform.rotation.z);
        b2Body_SetTransform(m_physical.bodyId, position, b2MakeRot(angle));
    }

    b2Vec2 PhysicalEntity::ToBox2DPosition(const float3& worldPosition)
    {
        return b2Vec2{ worldPosition.x * gWorldToBox2DScale, worldPosition.y * gWorldToBox2DScale };
    }

    float3 PhysicalEntity::ToWorldPosition(const b2Vec2& box2dPosition)
    {
        return float3{ box2dPosition.x * gBox2DToWorldScale, box2dPosition.y * gBox2DToWorldScale, 0.0f };
    }

    b2Vec2 PhysicalEntity::ToBox2DVector(const float3& worldVector)
    {
        return b2Vec2{ worldVector.x * gWorldToBox2DScale, worldVector.y * gWorldToBox2DScale };
    }

    float3 PhysicalEntity::ToWorldVector(const b2Vec2& box2dVector)
    {
        return float3{ box2dVector.x * gBox2DToWorldScale, box2dVector.y * gBox2DToWorldScale, 0.0f };
    }

    float PhysicalEntity::ToBox2DAngle(float worldRadians)
    {
        return worldRadians;
    }

    float PhysicalEntity::ToWorldAngle(float box2dRadians)
    {
        return box2dRadians;
    }

    float PhysicalEntity::ToBox2DLength(float worldLength)
    {
        return worldLength * gWorldToBox2DScale;
    }

    float PhysicalEntity::ToWorldLength(float box2dLength)
    {
        return box2dLength * gBox2DToWorldScale;
    }

    void PhysicalEntity::SetWorldToBox2DScale(float scale)
    {
        const float sanitized = (std::isfinite(scale) && (scale > 0.0f)) ? scale : 1.0f;
        gWorldToBox2DScale = sanitized;
        gBox2DToWorldScale = 1.0f / gWorldToBox2DScale;
    }

    float PhysicalEntity::GetWorldToBox2DScale()
    {
        return gWorldToBox2DScale;
    }

    float PhysicalEntity::GetBox2DToWorldScale()
    {
        return gBox2DToWorldScale;
    }

    void PhysicalEntity::SetTransformViaPhysics(const float3& position, const float3& rotation)
    {
        if (HasBox2DBody())
        {
            b2Body_SetTransform(m_physical.bodyId, ToBox2DPosition(position), b2MakeRot(ToBox2DAngle(rotation.z)));
            SyncTransformFromBox2DBody();
            return;
        }

        Transform updated = GetTransform();
        updated.position = position;
        updated.rotation = rotation;
        SetTransform(updated);
    }

    void PhysicalEntity::SetPositionViaPhysics(const float3& position)
    {
        if (HasBox2DBody())
        {
            const float currentAngle = ToBox2DAngle(GetTransform().rotation.z);
            b2Body_SetTransform(m_physical.bodyId, ToBox2DPosition(position), b2MakeRot(currentAngle));
            SyncTransformFromBox2DBody();
            return;
        }

        SetPosition(position);
    }

    void PhysicalEntity::SetBox2DMesh(const std::vector<std::vector<b2Vec2>>& polygons, const std::vector<b2Vec2>& boundary)
    {
        m_box2dPolygons = polygons;
        m_box2dBoundary = boundary;

        RecreateBox2DBody();
    }

    bool PhysicalEntity::HasBox2DMesh() const
    {
        return !m_box2dPolygons.empty();
    }

    const std::vector<std::vector<b2Vec2>>& PhysicalEntity::GetBox2DPolygons() const
    {
        return m_box2dPolygons;
    }

    const std::vector<b2Vec2>& PhysicalEntity::GetBox2DBoundary() const
    {
        return m_box2dBoundary;
    }

    void PhysicalEntity::SetBox2DShapeConfig(std::optional<Physics::Box2DShapeConfig> config)
    {
        m_box2dShapeConfig = std::move(config);

        RecreateBox2DBody();
    }

    void PhysicalEntity::AttachPhysicsMachine(Systems::PhysicsMachine* physicsMachine)
    {
        if (physicsMachine == m_physicsMachine)
        {
            if (m_physicsMachine && !m_physical.HasBody())
            {
                if (HasPhysicsDefinition(*this))
                {
                    m_physicsMachine->RegisterBody(*this);
                }
                else
                {
#if defined(_DEBUG)
                    std::ostringstream oss;
                    oss << "[PhysicalEntity] Deferring physics registration for tag='" << GetTag()
                        << "' until collider/shape/mesh is ready.\n";
                    DebugTrace(oss.str());
#endif
                }
            }
            return;
        }

        if (m_physicsMachine)
        {
            m_physicsMachine->UnregisterBody(*this);
            m_physical.Reset();
        }

        m_physicsMachine = physicsMachine;

        if (m_physicsMachine)
        {
            ResetBodyCreationFailure();
            if (HasPhysicsDefinition(*this))
            {
                m_physicsMachine->RegisterBody(*this);
            }
            else
            {
#if defined(_DEBUG)
                std::ostringstream oss;
                oss << "[PhysicalEntity] Deferring physics registration for tag='" << GetTag()
                    << "' until collider/shape/mesh is ready.\n";
                DebugTrace(oss.str());
#endif
            }
        }
    }

    void PhysicalEntity::DetachPhysicsMachine()
    {
        if (!m_physicsMachine)
        {
            return;
        }

        m_physicsMachine->UnregisterBody(*this);
        m_physicsMachine = nullptr;
        m_physical.Reset();
    }

    void PhysicalEntity::RecreateBox2DBody()
    {
        MarkPhysicsDefinitionChanged();

        if (!m_physicsMachine)
        {
            return;
        }

        if (m_physical.HasBody())
        {
            // ミカ：再作成ではコライダー定義を残し、新しいボディにも同じ形状を登録する
            m_physicsMachine->UnregisterBody(*this, true);
            m_physical.Reset();
        }

        ResetBodyCreationFailure();
        m_physicsMachine->RegisterBody(*this);
    }
}

namespace Entities
{
    void TranslatePhysicalEntity(PhysicalEntity& entity, const float3& delta)
    {
        const b2BodyId bodyId = entity.GetBox2DBodyId();
        if (b2Body_IsValid(bodyId))
        {
            const b2Vec2 currentPosition = b2Body_GetPosition(bodyId);
            const b2Vec2 deltaPosition = PhysicalEntity::ToBox2DVector(delta);
            const b2Vec2 targetPosition{ currentPosition.x + deltaPosition.x, currentPosition.y + deltaPosition.y };

            b2Body_SetTransform(bodyId, targetPosition, b2Body_GetRotation(bodyId));
            entity.SyncTransformFromBox2DBody();
            return;
        }

        entity.ApplyTranslation(delta);
        entity.SyncBox2DBodyFromTransform();
    }
}
