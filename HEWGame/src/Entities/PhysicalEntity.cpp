#include "Entities/PhysicalEntity.h"

#include <memory>
#include <utility>

namespace Entities
{
    PhysicalEntity::PhysicalEntity(const Transform& transform, std::shared_ptr<Collision::ICollider> collider, Collision::CollisionLayer layer, Physics::Physical physical, std::shared_ptr<Render::Model> model, Systems::PhysicsMachine* physicsMachine, Systems::ContactLiteSystem* contactSystem, Core::UpdatePhase phase, int priority, std::string tag)
        : CollisionEntity(transform, std::move(collider), layer, std::move(model), contactSystem, phase, priority, std::move(tag))
        , m_physical(physical)
        , m_physicsMachine(physicsMachine)
    {
        AttachPhysicsMachine(m_physicsMachine);
    }

    PhysicalEntity::PhysicalEntity(const std::string& tag, const float3& pos, const float3& rot, const float3& scl, std::shared_ptr<Collision::ICollider> collider, Collision::CollisionLayer layer, const float3& velocity, const float3& force, float weight, float damping, Core::UpdatePhase phase, int priority)
        : CollisionEntity(Transform{ pos, rot, scl }, std::move(collider), layer, nullptr, nullptr, phase, priority)
        , m_physical(Physics::Physical{ velocity, force, weight, damping })
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
        m_box2dBodyId = bodyId;
    }

    b2BodyId PhysicalEntity::GetBox2DBodyId() const
    {
        return m_box2dBodyId;
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
            if (m_physicsMachine && !b2Body_IsValid(m_box2dBodyId))
            {
                m_physicsMachine->RegisterBody(*this);
            }
            return;
        }

        if (m_physicsMachine)
        {
            m_physicsMachine->UnregisterBody(*this);
            m_box2dBodyId = b2_nullBodyId;
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
        m_box2dBodyId = b2_nullBodyId;
    }

    void PhysicalEntity::RecreateBox2DBody()
    {
        if (!m_physicsMachine)
        {
            return;
        }

        if (b2Body_IsValid(m_box2dBodyId))
        {
            m_physicsMachine->UnregisterBody(*this);
            m_box2dBodyId = b2_nullBodyId;
        }

        m_physicsMachine->RegisterBody(*this);
    }
}

namespace Entities
{
    void TranslatePhysicalEntity(PhysicalEntity& entity, const float3& delta)
    {
        entity.ApplyTranslation(delta);
    }
}

