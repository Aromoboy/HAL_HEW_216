#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

#include "Entities/PhysicalEntity.h"
#include "Input.h"
#include "PickupConfig.h"
#include "SweetsManager.h"
#include "VisualTransform.h"
#include "AnimationCache.h"

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
        ~Player() override = default;

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
        enum class PlayerAnimationState : std::uint8_t
        {
            Idle,
            Move,
            Stab
        };

        struct ColliderFootprint
        {
            float3 halfExtents{ 0.0f, 0.0f, 0.0f };
            float3 centerOffset{ 0.0f, 0.0f, 0.0f };
        };

        struct PickupOrigin
        {
            float3 center{ 0.0f, 0.0f, 0.0f };
            float halfDepth = 0.05f;
            const char* source = "Unknown";
        };

        void SyncVisualTransformFromPhysics();
        void RefreshRenderTransform();

        VisualTransform m_visual{};
        Transform m_renderTransform{};
        std::shared_ptr<Render::Model> m_fairyModel;
        SweetsSpace::SweetsManager* m_sweetsManager;
        std::vector<std::shared_ptr<SweetsSpace::BaseSweets>> m_carrieds;
        std::unordered_map<SweetsSpace::BaseSweets*, float3> m_carriedRotations;
        int m_maxCarry;
        float m_pickupHalfWidth;
        float m_pickupHalfHeight;
        Stage::PickupOffset m_pickupOffset;
        float m_pickupMaxDistance;
        float m_pickupDepthThreshold;
        float3 m_pickupOriginOffset;
        float3 m_carryOffset;
        float m_dropUpwardBoost;
        bool m_isKinematic;
        float m_baseHorizontalMoveSpeed;
        float m_baseVerticalMoveSpeed;
        float m_speedMultiplier;
        float m_facingSign;
        bool m_hasMinimumY;
        float m_minimumY;
        bool m_missingBodyLogged;
        bool m_missingModelWarned;
        bool m_hasScreenBounds;
        float m_minScreenX;
        float m_maxScreenX;
        bool m_hasMaximumY;
        float m_maximumY;
        float m_moveSoundHandle;
        bool m_wasMovingLastFrame;
        bool m_moveSoundFadingOut;      
        float m_moveSoundFadeTimer;     
        float m_moveSoundFadeDuration;
        std::optional<ColliderFootprint> m_colliderFootprint;
        SweetsSpace::PickupQueryResult m_lastPickupResult;
        size_t m_lastCarryInputLoggedCount;

        float3 ComputeCarryAnchor() const;
        PickupOrigin ComputePickupOrigin() const;
        void UpdateCarriedSweets();
        void HandleCarryInput(Core::UpdateContext& ctx);
        void TryPickupNearest();
        void DropCarriedSweets();
        void ClampVerticalPosition();
        void ClampScreenBounds();

        void SetAnimationState(PlayerAnimationState state, bool forceRestart = false);
        void UpdateAnimationState(bool isMoving);
        void StartStabAnimation();

        PlayerAnimationState m_animationState{ PlayerAnimationState::Idle };
        std::shared_ptr<Animation> m_idleAnimation;
        std::shared_ptr<Animation> m_moveAnimation;
        std::shared_ptr<Animation> m_stabAnimation;
        float m_stabTimer;
    };
}
