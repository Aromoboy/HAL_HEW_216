#include "Entities/CollisionEntity.h"

namespace Entities
{
    CollisionEntity::CollisionEntity(const Transform& transform, std::shared_ptr<Collision::ICollider> collider, Collision::CollisionLayer layer, std::shared_ptr<Render::Model> model, Systems::ContactLiteSystem* contactSystem, Core::UpdatePhase phase, int priority, std::string tag)
        : Entity(transform, std::move(model), phase, priority, std::move(tag))
        , m_collider(std::move(collider))
        , m_collisionLayer(layer)
        , m_contactSystem(contactSystem)
    {
    }

    CollisionEntity::CollisionEntity(const std::string& tag, const float3& pos, const float3& rot, const float3& scl, std::shared_ptr<Collision::ICollider> collider, Collision::CollisionLayer layer, Core::UpdatePhase phase, int priority)
        : CollisionEntity(Transform{ pos, rot, scl }, std::move(collider), layer, nullptr, nullptr, phase, priority, tag)
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

