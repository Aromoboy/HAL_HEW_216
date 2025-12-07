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
        bool IsTransformDirty() const;

        void ApplyTranslation(const float3& delta);

        void DebugDraw();

        void UpdateSleepState(const float3& velocity);
        void Wake();
        bool IsSleeping() const { return m_isSleeping; }

        const AABB& GetAABB() const { return m_aabb; }

        Systems::ContactLiteSystem* GetContactSystem() const;

    protected:
        std::shared_ptr<Collision::ICollider> m_collider;
        Collision::CollisionLayer m_collisionLayer;
        Systems::ContactLiteSystem* m_contactSystem;
        Transform m_lastTransform;
        Transform m_lastBoundsTransform;
        float3 m_lastVelocity;
        AABB m_aabb{};
        int m_stationaryFrameCount{ 0 };
        bool m_transformDirty{ true };
        bool m_isSleeping{ false };
    };
}

