#pragma once

namespace Entities
{
    class Player;

    class PlayerPhysicsBinder
    {
    public:
        explicit PlayerPhysicsBinder(bool isKinematic);

        bool IsKinematic() const;
        void SetScreenBounds(float minX, float maxX);
        void ClampScreenBounds(Player& player);

    private:
        bool m_isKinematic = false;
        bool m_hasScreenBounds = false;
        float m_minScreenX = 0.0f;
        float m_maxScreenX = 0.0f;
    };
}
