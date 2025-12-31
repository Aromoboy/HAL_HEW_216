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

    void PlayerPhysicsBinder::SetMinimumY(float minY)
    {
        if (!std::isfinite(minY))
        {
            PlayerDebugTrace(std::string(kPlayerLogPrefix) + "非有限の最小 Y 指定を無視しました \n");
            return;
        }

        if (m_hasMinimumY && std::fabs(m_minimumY - minY) < std::numeric_limits<float>::epsilon())
        {
            return;
        }

        m_hasMinimumY = true;
        m_minimumY = minY;

        std::ostringstream oss;
        oss << kPlayerLogPrefix << "最小 Y を " << m_minimumY << " 単位に設定しました \n";
        PlayerDebugTrace(oss.str());
    }

    float PlayerPhysicsBinder::GetMinimumY() const
    {
        return m_minimumY;
    }

    bool PlayerPhysicsBinder::HasMinimumY() const
    {
        return m_hasMinimumY;
    }

    void PlayerPhysicsBinder::SetScreenBounds(float minX, float maxX, std::optional<float> maxY)
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
            || std::fabs(m_maxScreenX - sanitizedMaxX) > std::numeric_limits<float>::epsilon()
            || (m_hasMaximumY != maxY.has_value())
            || (maxY.has_value() && std::fabs(m_maximumY - *maxY) > std::numeric_limits<float>::epsilon());

        m_hasScreenBounds = true;
        m_minScreenX = sanitizedMinX;
        m_maxScreenX = sanitizedMaxX;
        m_hasMaximumY = maxY.has_value() && std::isfinite(*maxY);
        if (m_hasMaximumY)
        {
            m_maximumY = *maxY;
        }

        if (!boundsChanged)
        {
            return;
        }

        std::ostringstream oss;
        oss << kPlayerLogPrefix << "スクリーン範囲を minX=" << m_minScreenX
            << " maxX=" << m_maxScreenX;
        if (m_hasMaximumY)
        {
            oss << " maxY=" << m_maximumY;
        }
        oss << "\n";
        PlayerDebugTrace(oss.str());
    }

    void PlayerPhysicsBinder::ClampVerticalPosition(Player& player)
    {
        if (!m_hasMinimumY)
        {
            return;
        }

        if (!player.HasBox2DBody())
        {
            return;
        }

        const b2BodyId bodyId = player.GetBox2DBodyId();
        const float3 bodyPosition = Player::ToWorldPosition(b2Body_GetPosition(bodyId));
        if (bodyPosition.y >= m_minimumY)
        {
            return;
        }

        const float previousY = bodyPosition.y;
        const float3 bodyVelocity = player.GetLinearVelocity();
        b2Body_SetTransform(
            bodyId,
            Player::ToBox2DPosition(float3{ bodyPosition.x, m_minimumY, 0.0f }),
            b2Body_GetRotation(bodyId));
        float3 clampedVelocity = bodyVelocity;
        clampedVelocity.y = 0.0f;
        player.SetLinearVelocity(clampedVelocity);

        std::ostringstream oss;
        oss << kPlayerLogPrefix << "Y を " << previousY << " から最小 "
            << m_minimumY << " に制限しました（プラットフォーム境界） \n";
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
        const float originalY = position.y;

        const float clampedX = std::clamp(position.x, m_minScreenX, m_maxScreenX);
        float clampedY = position.y;
        if (m_hasMaximumY)
        {
            clampedY = std::min(clampedY, m_maximumY);
        }

        if (clampedX == position.x && clampedY == position.y)
        {
            return;
        }

        b2Body_SetTransform(
            bodyId,
            Player::ToBox2DPosition(float3{ clampedX, clampedY, 0.0f }),
            b2Body_GetRotation(bodyId));

        std::ostringstream oss;
        oss << kPlayerLogPrefix << "スクリーンクランプを適用しました prevX=" << originalX
            << " prevY=" << originalY
            << " newX=" << clampedX
            << " newY=" << clampedY
            << " boundsX=[" << m_minScreenX << ',' << m_maxScreenX << ']';
        if (m_hasMaximumY)
        {
            oss << " maxY=" << m_maximumY;
        }
        oss << "\n";
        PlayerDebugTrace(oss.str());
    }
}
