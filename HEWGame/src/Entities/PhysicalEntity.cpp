#include "Entities/PhysicalEntity.h"

#include <cmath>
#include <memory>
#include <utility>

#include <box2d/box2d.h>
#include <box2d/math_functions.h>

#include "Systems/PhysicsMachine.h"

namespace
{
    float gWorldToBox2DScale = 1.0f;
    float gBox2DToWorldScale = 1.0f / gWorldToBox2DScale;
}

namespace Entities
{
    PhysicalEntity::PhysicalEntity(const Transform& transform, std::shared_ptr<Collision::ICollider> collider, Collision::CollisionLayer layer, std::shared_ptr<Render::Model> model, Systems::PhysicsMachine* physicsMachine, Core::UpdatePhase phase, int priority, std::string tag)
        : CollisionEntity(transform, std::move(collider), layer, std::move(model), phase, priority, std::move(tag))
        , m_physical()
        , m_physicsMachine(physicsMachine)
    {
        AttachPhysicsMachine(m_physicsMachine);
    }

    PhysicalEntity::PhysicalEntity(
        const std::string& tag,
        const float3& pos,
        const float3& rot,
        const float3& scl,
        std::shared_ptr<Collision::ICollider> collider,
        Collision::CollisionLayer layer,
        Core::UpdatePhase phase,
        int priority)
        : CollisionEntity(Transform{ pos, rot, scl }, std::move(collider), layer, nullptr, phase, priority)
        , m_physical()
        , m_physicsMachine(nullptr)
    {
        m_tag = tag;
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
                m_physicsMachine->RegisterBody(*this);
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
            m_physicsMachine->RegisterBody(*this);
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
        if (!m_physicsMachine)
        {
            return;
        }

        if (m_physical.HasBody())
        {
            m_physicsMachine->UnregisterBody(*this);
            m_physical.Reset();
        }

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

