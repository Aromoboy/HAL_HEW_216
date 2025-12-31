#pragma once

#include <optional>

namespace Entities
{
    class Player;

    class PlayerPhysicsBinder
    {
    public:
        explicit PlayerPhysicsBinder(bool isKinematic);

        bool IsKinematic() const;
        void SetMinimumY(float minY);
        float GetMinimumY() const;
        bool HasMinimumY() const;
        void SetScreenBounds(float minX, float maxX, std::optional<float> maxY);

        void ClampVerticalPosition(Player& player);
        void ClampScreenBounds(Player& player);

    private:
        bool m_isKinematic = false;
        bool m_hasMinimumY = false;
        float m_minimumY = 0.0f;
        bool m_hasScreenBounds = false;
        float m_minScreenX = 0.0f;
        float m_maxScreenX = 0.0f;
        bool m_hasMaximumY = false;
        float m_maximumY = 0.0f;
    };
}
