#pragma once

#include <memory>
#include <string>

#include "AABB.h"
#include "CollisionLayer.h"
#include "Entities/Entity.h"
#include "ModelDX.h"
#include "Transform.h"

namespace Collision
{
    class ICollider;
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
            Core::UpdatePhase phase = Core::UpdatePhase::Gameplay,
            int priority = 0,
            std::string tag = {});
        CollisionEntity(const std::string& tag,
            const float3& pos,
            const float3& rot,
            const float3& scl,
            std::shared_ptr<Collision::ICollider> collider,
            Collision::CollisionLayer layer = Collision::CollisionLayer::Default,
            Core::UpdatePhase phase = Core::UpdatePhase::Gameplay,
            int priority = 0);
        ~CollisionEntity() override = default;

        Collision::ICollider* GetCollider();
        const Collision::ICollider* GetCollider() const;

        Collision::CollisionLayer GetCollisionLayer() const;
        void SetCollisionLayer(Collision::CollisionLayer layer);

        void SetTransform(const Transform& transform);
        void SetPosition(const float3& position);
        void SetRotation(const float3& rotation);
        void SetScale(const float3& scale);

        void SyncColliderBounds();
        void MarkTransformDirty();
        bool IsTransformDirty() const;
        void ApplyTranslation(const float3& delta);

        void OnUpdate(Core::UpdateContext& ctx) override;

        void DebugDraw();

        void UpdateSleepState(const float3& velocity);
        void Wake();

    protected:
        std::shared_ptr<Collision::ICollider> m_collider;
        Collision::CollisionLayer m_collisionLayer{ Collision::CollisionLayer::Default };

        AABB m_aabb{};
        Transform m_lastBoundsTransform{};
        Transform m_lastTransform{};
        bool m_transformDirty{ false };

        bool m_isSleeping{ false };
        int m_stationaryFrameCount{ 0 };
        float3 m_lastVelocity{ 0.0f, 0.0f, 0.0f };
    };
}
