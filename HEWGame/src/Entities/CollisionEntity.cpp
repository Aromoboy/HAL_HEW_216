#include "Entities/CollisionEntity.h"

#include <cmath>
#include <sstream>
#include <utility>

#if defined(_WIN32)
#    ifndef NOMINMAX
#        define NOMINMAX
#    endif
#    include <windows.h>
#endif

namespace
{
#if defined(_WIN32) && defined(_DEBUG)
    void DebugTrace(const std::string& message)
    {
        ::OutputDebugStringA(message.c_str());
    }
#else
    void DebugTrace(const std::string& message)
    {
        (void)message;
    }
#endif

    constexpr float kSleepVelocityEpsilon = 0.01f;
    constexpr int kSleepFrameThreshold = 10;

    bool AreTransformsEqual(const Transform& a, const Transform& b)
    {
        const bool positionEqual = (a.position.x == b.position.x)
            && (a.position.y == b.position.y)
            && (a.position.z == b.position.z);
        const bool rotationEqual = (a.rotation.x == b.rotation.x)
            && (a.rotation.y == b.rotation.y)
            && (a.rotation.z == b.rotation.z);
        const bool scaleEqual = (a.scale.x == b.scale.x)
            && (a.scale.y == b.scale.y)
            && (a.scale.z == b.scale.z);

        return positionEqual && rotationEqual && scaleEqual;
    }
}

namespace Entities
{
    CollisionEntity::CollisionEntity(const Transform& transform, std::shared_ptr<Collision::ICollider> collider, Collision::CollisionLayer layer, std::shared_ptr<Render::Model> model, Systems::ContactLiteSystem* contactSystem, Core::UpdatePhase phase, int priority, std::string tag)
        : Entity(transform, std::move(model), phase, priority, std::move(tag))
        , m_collider(std::move(collider))
        , m_collisionLayer(layer)
        , m_contactSystem(contactSystem)
        , m_lastTransform(transform)
        , m_lastBoundsTransform(transform)
        , m_lastVelocity{ 0.0f, 0.0f, 0.0f }
    {
        if (m_collider)
        {
            m_collider->SetLayer(layer);
        }
        SyncColliderBounds();
    }

    CollisionEntity::CollisionEntity(const std::string& tag, const float3& pos, const float3& rot, const float3& scl, std::shared_ptr<Collision::ICollider> collider, Collision::CollisionLayer layer, Core::UpdatePhase phase, int priority)
        : CollisionEntity(Transform{ pos, rot, scl }, std::move(collider), layer, nullptr, nullptr, phase, priority, tag)
    {
    }

    Collision::ICollider* CollisionEntity::GetCollider()
    {
        return m_collider.get();
    }

    const Collision::ICollider* CollisionEntity::GetCollider() const
    {
        return m_collider.get();
    }

    Collision::CollisionLayer CollisionEntity::GetCollisionLayer() const
    {
        return m_collisionLayer;
    }

    void CollisionEntity::SetCollisionLayer(Collision::CollisionLayer layer)
    {
        m_collisionLayer = layer;
        if (m_collider)
        {
            m_collider->SetLayer(layer);
        }
    }

    void CollisionEntity::SyncColliderBounds()
    {
        if (!m_collider)
        {
#if defined(_DEBUG)
            std::ostringstream oss;
            oss << "[CollisionEntity] SyncColliderBounds skipped for entity '" << m_tag
                << "' because collider is null.\n";
            DebugTrace(oss.str());
#endif
            return;
        }

        if (!IsTransformDirty())
        {
            return;
        }

        m_collider->UpdateBounds(m_transform);
        m_lastBoundsTransform = m_transform;
        m_transformDirty = false;
    }

    bool CollisionEntity::IsTransformDirty() const
    {
        return m_transformDirty || !AreTransformsEqual(m_transform, m_lastBoundsTransform);
    }

    void CollisionEntity::ApplyTranslation(const float3& delta)
    {
        if (delta.x == 0.0f && delta.y == 0.0f && delta.z == 0.0f)
        {
            return;
        }

        Wake();
        m_transform.position.x += delta.x;
        m_transform.position.y += delta.y;
        m_transform.position.z += delta.z;
        m_transformDirty = true;
        SyncColliderBounds();
    }

    void CollisionEntity::UpdateSleepState(const float3& velocity)
    {
        const float dx = m_transform.position.x - m_lastTransform.position.x;
        const float dy = m_transform.position.y - m_lastTransform.position.y;
        const float dz = m_transform.position.z - m_lastTransform.position.z;
        const float displacementSq = dx * dx + dy * dy + dz * dz;

        const float vx = velocity.x;
        const float vy = velocity.y;
        const float vz = velocity.z;
        const float velocitySq = vx * vx + vy * vy + vz * vz;

        const float sleepThresholdSq = kSleepVelocityEpsilon * kSleepVelocityEpsilon;
        const bool moved = displacementSq > sleepThresholdSq;
        const bool velocityAwake = velocitySq > sleepThresholdSq;

        if (moved || velocityAwake)
        {
            Wake();
        }
        else if (m_stationaryFrameCount < kSleepFrameThreshold)
        {
            ++m_stationaryFrameCount;
            if (m_stationaryFrameCount >= kSleepFrameThreshold)
            {
                m_isSleeping = true;
            }
        }

        m_lastTransform = m_transform;
        m_lastVelocity = velocity;
    }

    void CollisionEntity::Wake()
    {
        m_isSleeping = false;
        m_stationaryFrameCount = 0;
    }

    Systems::ContactLiteSystem* CollisionEntity::GetContactSystem() const
    {
        return m_contactSystem;
    }
}

