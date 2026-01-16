#pragma once

#include "Vector.h"

namespace Core
{
    struct UpdateContext;
    class Input;
}

namespace Entities
{
    class Player;

    class PlayerMovement
    {
    public:
        void Update(Player& player, const Core::UpdateContext& ctx, Core::Input& input);

        void SetMoveSpeed(float speed);
        void SetMoveSpeeds(float horizontal, float vertical);
        void SetBaseMoveSpeed(float horizontal, float vertical);
        void SetSpeedMultiplier(float multiplier);
        void SetYawOffsetRadians(float yawOffsetRadians);
        float GetMoveSpeed() const;
        float GetHorizontalMoveSpeed() const;
        float GetVerticalMoveSpeed() const;
        float GetFacingSign() const;

    private:
        float m_baseHorizontalMoveSpeed{ 0.0f };
        float m_baseVerticalMoveSpeed{ 0.0f };
        float m_speedMultiplier{ 1.0f };
        float m_accel{ 35.0f };
        float m_brake{ 8.0f };
        float m_maxSpeed{ 25.0f };
        float m_facingSign{ 1.0f };
        float3 m_baseRotation{ 0.0f, 0.0f, 0.0f };
        bool m_hasBaseRotation{ false };
        float m_turnRate{ 4.0f };
        float m_currentHeading{ 0.0f };
        bool m_hasCurrentHeading{ false };
        float m_baseYaw{ 0.0f };
        bool m_useYawAxisY{ false };
        float m_yawOffsetRadians{ 0.0f };
        float m_rollMaxRadians{ 0.436332f };
        float m_rollInputRange{ 0.785398f };
        float m_rollResponse{ 6.0f };
        float m_currentRoll{ 0.0f };
        bool m_missingBodyLogged{ false };
        float m_moveSoundHandle{ -1.0f };
        bool m_wasMovingLastFrame{ false };
        bool m_moveSoundFadingOut{ false };
        float m_moveSoundFadeTimer{ 0.0f };
        float m_moveSoundFadeDuration{ 0.4f };
        float3 m_fallbackVelocity{ 0.0f, 0.0f, 0.0f };
    };
}
