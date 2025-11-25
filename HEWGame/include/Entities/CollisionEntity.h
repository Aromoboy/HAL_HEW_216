#pragma once

#include <memory>

#include "Entities/Entity.h"
#include "ICollider.h"
#include "CollisionLayer.h"

namespace Systems
{
    class ContactLiteSystem;
}

namespace Entities
{
    class CollisionEntity : public Entity
    {
    public:
        CollisionEntity(const Transform& transform,
            std::shared_ptr<Collision::ICollider> collider = nullptr,
            Collision::CollisionLayer layer = Collision::CollisionLayer::Default,
            std::shared_ptr<Render::Model> model = nullptr,
            Systems::ContactLiteSystem* contactSystem = nullptr,
            Core::UpdatePhase phase = Core::UpdatePhase::Gameplay,
            int priority = 0);
        ~CollisionEntity() override = default;

        Collision::ICollider* GetCollider();
        const Collision::ICollider* GetCollider() const;

        Collision::CollisionLayer GetCollisionLayer() const;

        Systems::ContactLiteSystem* GetContactSystem() const;

    protected:
        std::shared_ptr<Collision::ICollider> m_collider;
        Collision::CollisionLayer m_collisionLayer;
        Systems::ContactLiteSystem* m_contactSystem;
    };
}

