#include "Entities/PlayerPhysicsBinder.h"

#include "Entities/Player.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <sstream>

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
    constexpr float kScreenWrapEpsilon = 1e-4f;
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

    void PlayerPhysicsBinder::SetScreenBounds(float minX, float maxX, float minY, float maxY)
    {
        if (!std::isfinite(minX) || !std::isfinite(maxX) || !std::isfinite(minY) || !std::isfinite(maxY))
        {
            PlayerDebugTrace(std::string(kPlayerLogPrefix) + "非有限のスクリーン範囲を無視しました \n");
            return;
        }

        const float sanitizedMinX = std::min(minX, maxX);
        const float sanitizedMaxX = std::max(minX, maxX);
        const float sanitizedMinY = std::min(minY, maxY);
        const float sanitizedMaxY = std::max(minY, maxY);
        const bool hadBounds = m_hasScreenBounds;
        const bool boundsChanged = !hadBounds
            || std::fabs(m_minScreenX - sanitizedMinX) > std::numeric_limits<float>::epsilon()
            || std::fabs(m_maxScreenX - sanitizedMaxX) > std::numeric_limits<float>::epsilon()
            || std::fabs(m_minScreenY - sanitizedMinY) > std::numeric_limits<float>::epsilon()
            || std::fabs(m_maxScreenY - sanitizedMaxY) > std::numeric_limits<float>::epsilon();

        m_hasScreenBounds = true;
        m_minScreenX = sanitizedMinX;
        m_maxScreenX = sanitizedMaxX;
        m_minScreenY = sanitizedMinY;
        m_maxScreenY = sanitizedMaxY;

        if (!boundsChanged)
        {
            return;
        }

        std::ostringstream oss;
        oss << kPlayerLogPrefix << "スクリーン範囲を minX=" << m_minScreenX
            << " maxX=" << m_maxScreenX
            << " minY=" << m_minScreenY
            << " maxY=" << m_maxScreenY;
        oss << "\n";
        PlayerDebugTrace(oss.str());
    }

    void PlayerPhysicsBinder::ClampScreenBounds(Player& player)
    {
        if (!m_hasScreenBounds)
        {
            return;
        }

        const float spanX = m_maxScreenX - m_minScreenX;
        const float spanY = m_maxScreenY - m_minScreenY;
        if (spanX <= kScreenWrapEpsilon && spanY <= kScreenWrapEpsilon)
        {
            return;
        }

        player.SyncColliderBounds();
        const AABB bounds = player.GetAABB();

        float3 position = player.GetTransform().position;
        bool wrapped = false;

        if (spanX > kScreenWrapEpsilon)
        {
            if (bounds.min.x < m_minScreenX)
            {
                position.x += spanX;
                wrapped = true;
            }
            else if (bounds.max.x > m_maxScreenX)
            {
                position.x -= spanX;
                wrapped = true;
            }
        }

        if (spanY > kScreenWrapEpsilon)
        {
            if (bounds.min.y < m_minScreenY)
            {
                position.y += spanY;
                wrapped = true;
            }
            else if (bounds.max.y > m_maxScreenY)
            {
                position.y -= spanY;
                wrapped = true;
            }
        }

        if (wrapped)
        {
            player.SetPositionViaPhysics(position);
        }
    }

}
