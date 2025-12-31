#pragma once

namespace Core
{
    struct UpdateContext;
}

namespace Entities
{
    class Player;

    class PlayerMovement
    {
    public:
        void Update(Player& player, Core::UpdateContext& ctx);

        void SetMoveSpeed(float speed);
        void SetMoveSpeeds(float horizontal, float vertical);
        void SetBaseMoveSpeed(float horizontal, float vertical);
        void SetSpeedMultiplier(float multiplier);
        float GetMoveSpeed() const;
        float GetHorizontalMoveSpeed() const;
        float GetVerticalMoveSpeed() const;
        float GetFacingSign() const;

    private:
        float m_baseHorizontalMoveSpeed{ 0.0f };
        float m_baseVerticalMoveSpeed{ 0.0f };
        float m_speedMultiplier{ 1.0f };
        float m_facingSign{ 1.0f };
        bool m_missingBodyLogged{ false };
        float m_moveSoundHandle{ -1.0f };
        bool m_wasMovingLastFrame{ false };
        bool m_moveSoundFadingOut{ false };
        float m_moveSoundFadeTimer{ 0.0f };
        float m_moveSoundFadeDuration{ 0.4f };
    };
}
