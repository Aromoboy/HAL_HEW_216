#include "Entities/PlayerPhysicsBinder.h"

#include "Entities/Player.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <sstream>

#include <box2d/box2d.h>

#if defined(_WIN32)
#    ifndef NOMINMAX
#        define NOMINMAX
#    endif
#    include <windows.h>
#endif

namespace
{
#if defined(_WIN32) && defined(_DEBUG)
    inline void PlayerDebugTrace(const std::string& message)
    {
        ::OutputDebugStringA(message.c_str());
    }
#else
    inline void PlayerDebugTrace(const std::string& message)
    {
        (void)message;
    }
#endif

    constexpr const char* kPlayerLogPrefix = "[Player] ";
}

namespace Entities
{
    PlayerPhysicsBinder::PlayerPhysicsBinder(bool isKinematic)
        : m_isKinematic(isKinematic)
    {
    }

    bool PlayerPhysicsBinder::IsKinematic() const
    {
        return m_isKinematic;
    }

    void PlayerPhysicsBinder::SetScreenBounds(float minX, float maxX)
    {
        if (!std::isfinite(minX) || !std::isfinite(maxX))
        {
            PlayerDebugTrace(std::string(kPlayerLogPrefix) + "非有限のスクリーン範囲を無視しました \n");
            return;
        }

        const float sanitizedMinX = std::min(minX, maxX);
        const float sanitizedMaxX = std::max(minX, maxX);
        const bool hadBounds = m_hasScreenBounds;
        const bool boundsChanged = !hadBounds
            || std::fabs(m_minScreenX - sanitizedMinX) > std::numeric_limits<float>::epsilon()
            || std::fabs(m_maxScreenX - sanitizedMaxX) > std::numeric_limits<float>::epsilon();

        m_hasScreenBounds = true;
        m_minScreenX = sanitizedMinX;
        m_maxScreenX = sanitizedMaxX;

        if (!boundsChanged)
        {
            return;
        }

        std::ostringstream oss;
        oss << kPlayerLogPrefix << "スクリーン範囲を minX=" << m_minScreenX
            << " maxX=" << m_maxScreenX;
        oss << "\n";
        PlayerDebugTrace(oss.str());
    }

    void PlayerPhysicsBinder::ClampScreenBounds(Player& player)
    {
        if (!m_hasScreenBounds)
        {
            return;
        }

        if (!player.HasBox2DBody())
        {
            return;
        }

        const b2BodyId bodyId = player.GetBox2DBodyId();
        const float3 position = Player::ToWorldPosition(b2Body_GetPosition(bodyId));
        const float originalX = position.x;

        float wrappedX = position.x;
        if (position.x < m_minScreenX)
        {
            wrappedX = m_maxScreenX;
        }
        else if (position.x > m_maxScreenX)
        {
            wrappedX = m_minScreenX;
        }

        if (wrappedX == position.x)
        {
            return;
        }

        b2Body_SetTransform(
            bodyId,
            Player::ToBox2DPosition(float3{ wrappedX, position.y, 0.0f }),
            b2Body_GetRotation(bodyId));

        std::ostringstream oss;
        oss << kPlayerLogPrefix << "スクリーンラップを適用しました prevX=" << originalX
            << " newX=" << wrappedX
            << " boundsX=[" << m_minScreenX << ',' << m_maxScreenX << ']';
        oss << "\n";
        PlayerDebugTrace(oss.str());
    }
}
