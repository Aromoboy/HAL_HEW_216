#pragma once

#include <optional>
#include <string>
#include <vector>

#include <box2d/box2d.h>

#include "Entities/PhysicalEntity.h"
#include "Input.h"

namespace SweetsSpace
{
    class BaseSweets;
    class SweetsManager;
}

namespace Entities
{
    class Player : public PhysicalEntity
    {
    public:
        Player(const Transform& transform,
            std::shared_ptr<Render::Model> model = nullptr,
            std::shared_ptr<Collision::ICollider> collider = nullptr,
            Physics::Physical physical = Physics::Physical{},
            Systems::PhysicsMachine* physicsMachine = nullptr,
            Systems::ContactLiteSystem* contactSystem = nullptr,
            SweetsSpace::SweetsManager* sweetsManager = nullptr,
            Collision::CollisionLayer collisionLayer = Collision::CollisionLayer::Player,
            bool kinematic = false,
            Core::UpdatePhase phase = Core::UpdatePhase::Gameplay,
            int priority = 0,
            std::string tag = "player");
        ~Player() override = default;

        void OnUpdate(Core::UpdateContext& ctx) override;
        void Draw(Render::ModelRenderer& renderer, const CameraSpace::ICamera& camera) override;

        void SetSweetsManager(SweetsSpace::SweetsManager* manager);
        bool IsCarryingSweets() const;
        const std::vector<std::shared_ptr<SweetsSpace::BaseSweets>>& GetCarriedSweets() const;
        void ClearCarriedSweets();
        bool IsKinematic() const;
        void SetMoveSpeed(float speed);
        void SetMoveSpeeds(float horizontal, float vertical);
        void SetBaseMoveSpeed(float horizontal, float vertical);
        void SetSpeedMultiplier(float multiplier);
        float GetMoveSpeed() const;
        float GetHorizontalMoveSpeed() const;
        float GetVerticalMoveSpeed() const;
        void SetMinimumY(float minY);
        float GetMinimumY() const;
        bool HasMinimumY() const;
        void SetScreenBounds(float minX, float maxX, std::optional<float> maxY);
        void SetPickupBox(float width, float height);
        void SetPickupArea(float width, float height, float forwardOffset);

    private:
        void SyncTransformFromBody();
        bool HasValidBody() const;
        void InitializeBody(const Transform& transform);
        b2Vec2 GetBodyLinearVelocity() const;
        void SetBox2DBodyId(b2BodyId bodyId) override;

        std::shared_ptr<Render::Model> m_fairyModel;
        Systems::ContactLiteSystem* m_contactSystem;
        SweetsSpace::SweetsManager* m_sweetsManager;
        std::vector<std::shared_ptr<SweetsSpace::BaseSweets>> m_carrieds;
        int m_maxCarry;
        float m_pickupHalfWidth;
        float m_pickupHalfHeight;
        float m_pickupForwardOffset;
        float3 m_carryOffset;
        float m_dropUpwardBoost;
        bool m_isKinematic;
        float m_baseHorizontalMoveSpeed;
        float m_baseVerticalMoveSpeed;
        float m_speedMultiplier;
        bool m_hasMinimumY;
        float m_minimumY;
        bool m_missingModelWarned;
        bool m_hasScreenBounds;
        float m_minScreenX;
        float m_maxScreenX;
        bool m_hasMaximumY;
        float m_maximumY;
        b2BodyId m_bodyId{ b2_nullBodyId };

        float3 ComputeCarryAnchor() const;
        void UpdateCarriedSweets();
        void HandleCarryInput(Core::UpdateContext& ctx);
        void TryPickupNearest();
        void DropCarriedSweets();
        void ClampVerticalPosition();
        void ClampScreenBounds();
    };
}

