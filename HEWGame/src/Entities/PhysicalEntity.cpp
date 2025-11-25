#include "Entities/PhysicalEntity.h"

namespace Entities
{
    PhysicalEntity::PhysicalEntity(const Transform& transform, std::shared_ptr<Collision::ICollider> collider, Collision::CollisionLayer layer, Physics::Physical physical, std::shared_ptr<Render::Model> model, Systems::PhysicsMachine* physicsMachine, Systems::ContactLiteSystem* contactSystem, Core::UpdatePhase phase, int priority)
        : CollisionEntity(transform, std::move(collider), layer, std::move(model), contactSystem, phase, priority)
        , m_physical(physical)
        , m_physicsMachine(physicsMachine)
    {
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
}

