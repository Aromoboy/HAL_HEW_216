#pragma once

namespace Entities
{
    class Player;

    class PlayerPhysicsBinder
    {
    public:
        explicit PlayerPhysicsBinder(bool isKinematic);

        bool IsKinematic() const;
        void SetScreenBounds(float minX, float maxX, float minY, float maxY);
        void SetWrapMargin(float margin);
        void SetWrapEnabled(bool wrapX, bool wrapY);
        void ClampScreenBounds(Player& player);

    private:
        bool m_isKinematic = false;
        bool m_hasScreenBounds = false;
        float m_minScreenX = 0.0f;
        float m_maxScreenX = 0.0f;
        float m_minScreenY = 0.0f;
        float m_maxScreenY = 0.0f;
        float m_wrapMargin = 0.0f;
        bool m_wrapXEnabled = true;
        bool m_wrapYEnabled = true;
    };
}
