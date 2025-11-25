#include "Entities/CollisionEntity.h"

namespace Entities
{
    CollisionEntity::CollisionEntity(const Transform& transform, std::shared_ptr<Collision::ICollider> collider, Collision::CollisionLayer layer, std::shared_ptr<Render::Model> model, Systems::ContactLiteSystem* contactSystem, Core::UpdatePhase phase, int priority)
        : Entity(transform, std::move(model), phase, priority)
        , m_collider(std::move(collider))
        , m_collisionLayer(layer)
        , m_contactSystem(contactSystem)
    {
    }

    Collision::ICollider* CollisionEntity::GetCollider()
    {
        return m_collider.get();
    }

    const Collision::ICollider* CollisionEntity::GetCollider() const
    {
        return m_collider.get();
    }

    Collision::CollisionLayer CollisionEntity::GetCollisionLayer() const
    {
        return m_collisionLayer;
    }

    Systems::ContactLiteSystem* CollisionEntity::GetContactSystem() const
    {
        return m_contactSystem;
    }
}

