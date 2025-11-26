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
            int priority = 0,
            std::string tag = {});
        CollisionEntity(const std::string& tag,
            const float3& pos = float3{ 0.0f, 0.0f, 0.0f },
            const float3& rot = float3{ 0.0f, 0.0f, 0.0f },
            const float3& scl = float3{ 1.0f, 1.0f, 1.0f },
            std::shared_ptr<Collision::ICollider> collider = nullptr,
            Collision::CollisionLayer layer = Collision::CollisionLayer::Default,
            Core::UpdatePhase phase = Core::UpdatePhase::Gameplay,
            int priority = 0);
        ~CollisionEntity() override = default;

        Collision::ICollider* GetCollider();
        const Collision::ICollider* GetCollider() const;

        Collision::CollisionLayer GetCollisionLayer() const;
        void SetCollisionLayer(Collision::CollisionLayer layer);

        void SyncColliderBounds();

        void ApplyTranslation(const float3& delta);

        Systems::ContactLiteSystem* GetContactSystem() const;

    protected:
        std::shared_ptr<Collision::ICollider> m_collider;
        Collision::CollisionLayer m_collisionLayer;
        Systems::ContactLiteSystem* m_contactSystem;
    };
}

namespace SceneSpace
{
    using CollisionEntity = Entities::CollisionEntity;
}

