#pragma once

#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "Entities/PhysicalEntity.h"
#include "Entities/PlayerModel.h"
#include "Entities/PlayerMovement.h"
#include "Entities/PlayerPhysicsBinder.h"
#include "Entities/PlayerPickupSystem.h"
#include "PickupConfig.h"
#include "Sweets/SweetsTypes.h"

namespace SweetsSpace
{
    class BaseSweets;
    class SweetsManager;
    struct PickupQueryResult;
}

namespace Entities
{
    class Player : public PhysicalEntity
    {
    public:
        Player(const Transform& transform,
            std::shared_ptr<Render::Model> model = nullptr,
            Systems::PhysicsMachine* physicsMachine = nullptr,
            SweetsSpace::SweetsManager* sweetsManager = nullptr,
            Collision::CollisionLayer collisionLayer = Collision::CollisionLayer::Player,
            bool kinematic = false,
            Core::UpdatePhase phase = Core::UpdatePhase::Gameplay,
            int priority = 0,
            std::string tag = "player");
        ~Player() override;

        void OnUpdate(Core::UpdateContext& ctx) override;
        void Draw(Render::ModelRenderer& renderer, const ::CameraSpace::ICamera& camera) override;

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
        void SetPickupArea(float width, float height, const Stage::PickupOffset& offset);
        void SetPickupMaxDistance(float maxDistance);
        void SetPickupDepthThreshold(float depthThreshold);
        void SetPickupOriginOffset(const float3& offset);
        void SetColliderFootprint(const float3& halfExtents, const float3& centerOffset);

        const SweetsSpace::PickupQueryResult& GetLastPickupResult() const noexcept;

        Transform GetRenderTransform() const override;

    private:
        friend class PlayerModel;
        friend class PlayerMovement;
        friend class PlayerPickupSystem;
        friend class PlayerPhysicsBinder;

        PlayerModel m_modelSystem;
        PlayerMovement m_movementSystem;
        PlayerPickupSystem m_pickupSystem;
        PlayerPhysicsBinder m_physicsBinder;
    };
}
