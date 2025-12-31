#include "Entities/PlayerPhysicsBinder.h"

#include "Entities/Player.h"
#include "Systems/PhysicsMachine.h"

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
    constexpr float kDefaultClampReturnSpeed = 6.0f;

    bool AreSameBody(b2BodyId lhs, b2BodyId rhs)
    {
        return lhs.index1 == rhs.index1
            && lhs.world0 == rhs.world0
            && lhs.generation == rhs.generation;
    }

    struct StaticOverlapContext
    {
        b2BodyId selfBody{ b2_nullBodyId };
        bool hitStatic = false;
    };

    bool OverlapStaticBodyCallback(b2ShapeId shapeId, void* context)
    {
        if (!context)
        {
            return true;
        }

        auto* overlapContext = static_cast<StaticOverlapContext*>(context);
        const b2BodyId bodyId = b2Shape_GetBody(shapeId);
        if (!b2Body_IsValid(bodyId))
        {
            return true;
        }

        if (AreSameBody(bodyId, overlapContext->selfBody))
        {
            return true;
        }

        if (b2Body_GetType(bodyId) == b2_staticBody)
        {
            overlapContext->hitStatic = true;
            return false;
        }

        return true;
    }

    bool HasStaticOverlapAtTarget(Entities::Player& player, b2BodyId bodyId, const float3& targetPosition)
    {
        Systems::PhysicsMachine* physicsMachine = player.GetPhysicsMachine();
        if (!physicsMachine)
        {
            return false;
        }

        const b2WorldId worldId = physicsMachine->GetWorldId();
        if (!b2World_IsValid(worldId))
        {
            return false;
        }

        b2AABB currentAabb{};
        if (!player.GetBox2DBodyAABB(currentAabb))
        {
            return false;
        }

        const b2Vec2 currentPosition = b2Body_GetPosition(bodyId);
        const float3 worldCurrentPosition = Entities::Player::ToWorldPosition(currentPosition);
        const float3 delta = float3{ targetPosition.x - worldCurrentPosition.x, targetPosition.y - worldCurrentPosition.y, 0.0f };
        const b2Vec2 deltaBox = Entities::Player::ToBox2DVector(delta);

        b2AABB targetAabb{};
        targetAabb.lowerBound = b2Vec2{ currentAabb.lowerBound.x + deltaBox.x, currentAabb.lowerBound.y + deltaBox.y };
        targetAabb.upperBound = b2Vec2{ currentAabb.upperBound.x + deltaBox.x, currentAabb.upperBound.y + deltaBox.y };

        b2QueryFilter filter = b2DefaultQueryFilter();
        filter.categoryBits = std::numeric_limits<uint64_t>::max();
        filter.maskBits = std::numeric_limits<uint64_t>::max();

        StaticOverlapContext context{};
        context.selfBody = bodyId;

        b2World_OverlapAABB(worldId, targetAabb, filter, OverlapStaticBodyCallback, &context);
        return context.hitStatic;
    }

    float SelectClampSpeed(float configuredSpeed, float fallbackSpeed)
    {
        if (std::isfinite(configuredSpeed) && configuredSpeed > 0.0f)
        {
            return configuredSpeed;
        }

        return fallbackSpeed;
    }
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
        const float3 targetPosition{ bodyPosition.x, m_minimumY, 0.0f };
        const float3 bodyVelocity = player.GetLinearVelocity();
        if (HasStaticOverlapAtTarget(player, bodyId, targetPosition))
        {
            float3 clampedVelocity = bodyVelocity;
            clampedVelocity.y = std::max(0.0f, clampedVelocity.y);
            player.SetLinearVelocity(clampedVelocity);

            std::ostringstream blocked;
            blocked << kPlayerLogPrefix << "Y クランプを回避しました（静的コライダーと重なるため） prevY="
                << previousY << " targetY=" << m_minimumY << "\n";
            PlayerDebugTrace(blocked.str());
            return;
        }

        const float clampSpeed = SelectClampSpeed(player.GetVerticalMoveSpeed(), kDefaultClampReturnSpeed);
        float3 clampedVelocity = bodyVelocity;
        clampedVelocity.y = std::max(clampedVelocity.y, clampSpeed);
        player.SetLinearVelocity(clampedVelocity);

        std::ostringstream oss;
        oss << kPlayerLogPrefix << "Y を " << previousY << " から最小 "
            << m_minimumY << " に向けて制限しました（速度補正）\n";
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
        const float3 bodyVelocity = player.GetLinearVelocity();

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

        const float3 targetPosition{ clampedX, clampedY, 0.0f };
        float3 adjustedVelocity = bodyVelocity;
        const bool blockedByStatic = HasStaticOverlapAtTarget(player, bodyId, targetPosition);
        const float horizontalClampSpeed = SelectClampSpeed(player.GetHorizontalMoveSpeed(), kDefaultClampReturnSpeed);
        const float verticalClampSpeed = SelectClampSpeed(player.GetVerticalMoveSpeed(), kDefaultClampReturnSpeed);

        if (position.x < m_minScreenX)
        {
            adjustedVelocity.x = blockedByStatic
                ? std::max(0.0f, adjustedVelocity.x)
                : std::max(adjustedVelocity.x, horizontalClampSpeed);
        }
        else if (position.x > m_maxScreenX)
        {
            adjustedVelocity.x = blockedByStatic
                ? std::min(0.0f, adjustedVelocity.x)
                : std::min(adjustedVelocity.x, -horizontalClampSpeed);
        }

        if (m_hasMaximumY && position.y > m_maximumY)
        {
            adjustedVelocity.y = blockedByStatic
                ? std::min(0.0f, adjustedVelocity.y)
                : std::min(adjustedVelocity.y, -verticalClampSpeed);
        }

        player.SetLinearVelocity(adjustedVelocity);

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
        if (blockedByStatic)
        {
            oss << " (static overlap: velocity-only)";
        }
        oss << "\n";
        PlayerDebugTrace(oss.str());
    }
}
