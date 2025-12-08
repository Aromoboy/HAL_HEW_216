#include "Entities/PhysicalEntity.h"

#include <memory>
#include <utility>

#include <box2d/box2d.h>

namespace
{
    constexpr float kWorldToBox2DScale = 1.0f;
    constexpr float kBox2DToWorldScale = 1.0f / kWorldToBox2DScale;
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

    void PhysicalEntity::SyncTransformFromBox2DBody()
    {
        if (!HasBox2DBody())
        {
            return;
        }

        const b2Vec2 position = b2Body_GetPosition(m_physical.bodyId);
        const float angle = b2Body_GetAngle(m_physical.bodyId);

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
        return b2Vec2{ worldPosition.x * kWorldToBox2DScale, worldPosition.y * kWorldToBox2DScale };
    }

    float3 PhysicalEntity::ToWorldPosition(const b2Vec2& box2dPosition)
    {
        return float3{ box2dPosition.x * kBox2DToWorldScale, box2dPosition.y * kBox2DToWorldScale, 0.0f };
    }

    b2Vec2 PhysicalEntity::ToBox2DVector(const float3& worldVector)
    {
        return b2Vec2{ worldVector.x * kWorldToBox2DScale, worldVector.y * kWorldToBox2DScale };
    }

    float3 PhysicalEntity::ToWorldVector(const b2Vec2& box2dVector)
    {
        return float3{ box2dVector.x * kBox2DToWorldScale, box2dVector.y * kBox2DToWorldScale, 0.0f };
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
        return worldLength * kWorldToBox2DScale;
    }

    float PhysicalEntity::ToWorldLength(float box2dLength)
    {
        return box2dLength * kBox2DToWorldScale;
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
            const b2Vec2 targetPosition = b2Vec2Add(currentPosition, PhysicalEntity::ToBox2DVector(delta));

            b2Body_SetTransform(bodyId, targetPosition, b2Body_GetAngle(bodyId));
            entity.SyncTransformFromBox2DBody();
            return;
        }

        entity.ApplyTranslation(delta);
        entity.SyncBox2DBodyFromTransform();
    }
}

