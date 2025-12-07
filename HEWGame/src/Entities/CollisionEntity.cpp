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

#if __has_include("CapsuleCollider2D.h")
#    include "CapsuleCollider2D.h"
#    define HAS_CAPSULE_COLLIDER2D 1
#else
#    define HAS_CAPSULE_COLLIDER2D 0
#endif

#include "Collision2D.h"
#include "DebugColliderRenderer.h"

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

    constexpr Color kAABBColor{ 1.0f, 1.0f, 1.0f, 0.4f };

    Color GetLayerColor(Collision::CollisionLayer layer)
    {
        switch (layer)
        {
        case Collision::CollisionLayer::Player:
            return Color{ 0.0f, 1.0f, 1.0f, 1.0f };
        case Collision::CollisionLayer::Sweets:
            return Color{ 1.0f, 1.0f, 0.0f, 1.0f };
        case Collision::CollisionLayer::Floor:
            return Color{ 1.0f, 0.0f, 1.0f, 1.0f };
        default:
            return Color{ 1.0f, 1.0f, 1.0f, 1.0f };
        }
    }

    float ComputeZMid(const AABB& bounds)
    {
        return (bounds.min.z + bounds.max.z) * 0.5f;
    }

    void DrawColliderShape(const Collision::ICollider& collider, const Transform& transform, const Color& shapeColor)
    {
        (void)transform;

        const AABB bounds = collider.GetAABB();
        DebugColliderRenderer::DrawAABB(bounds, kAABBColor);
        const float z = ComputeZMid(bounds);

        if (const auto* collider2D = dynamic_cast<const Collision::ICollider2D*>(&collider))
        {
            if (const auto* compound = dynamic_cast<const Collision::CompoundCollider2D*>(collider2D))
            {
                for (const auto& child : compound->GetChildren())
                {
                    if (!child)
                    {
                        continue;
                    }

                    DrawColliderShape(*child, transform, shapeColor);
                }
                return;
            }

#if HAS_CAPSULE_COLLIDER2D
            if (const auto* capsule = dynamic_cast<const Collision::CapsuleCollider2D*>(collider2D))
            {
                DebugColliderRenderer::DrawCapsule(capsule->GetP0(), capsule->GetP1(), capsule->GetRadius(), z, shapeColor);
                return;
            }
#endif

            if (const auto* circle = dynamic_cast<const Collision::CircleCollider2D*>(collider2D))
            {
                DebugColliderRenderer::DrawCircle(circle->GetCenter(), circle->GetRadius(), z, shapeColor);
                return;
            }

            if (const auto* hull = dynamic_cast<const Collision::ConvexHullCollider2D*>(collider2D))
            {
                DebugColliderRenderer::DrawHull(hull->GetWorldPoints(), z, shapeColor);
                return;
            }
        }

        DebugColliderRenderer::DrawAABB(bounds, shapeColor);
    }

    bool TransformApproxEqual(const Transform& a, const Transform& b, float epsilon = 1e-6f)
    {
        const auto AlmostEqual = [epsilon](float lhs, float rhs)
        {
            return std::fabs(lhs - rhs) <= epsilon;
        };

        const bool positionEqual = AlmostEqual(a.position.x, b.position.x)
            && AlmostEqual(a.position.y, b.position.y)
            && AlmostEqual(a.position.z, b.position.z);
        const bool rotationEqual = AlmostEqual(a.rotation.x, b.rotation.x)
            && AlmostEqual(a.rotation.y, b.rotation.y)
            && AlmostEqual(a.rotation.z, b.rotation.z);
        const bool scaleEqual = AlmostEqual(a.scale.x, b.scale.x)
            && AlmostEqual(a.scale.y, b.scale.y)
            && AlmostEqual(a.scale.z, b.scale.z);

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
        return m_transformDirty || !TransformApproxEqual(m_transform, m_lastBoundsTransform);
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

    void CollisionEntity::DebugDraw()
    {
        if (!m_collider)
        {
            return;
        }

        if (IsTransformDirty())
        {
            SyncColliderBounds();
        }

        const Color layerColor = GetLayerColor(m_collisionLayer);
        DrawColliderShape(*m_collider, m_transform, layerColor);
    }

    void CollisionEntity::UpdateSleepState(const float3& velocity)
    {
        const bool isPlayer = m_tag == "player";

        const float dx = m_transform.position.x - m_lastTransform.position.x;
        const float dy = m_transform.position.y - m_lastTransform.position.y;
        const float dz = m_transform.position.z - m_lastTransform.position.z;
        const float displacementSq = dx * dx + dy * dy + dz * dz;

        const float vx = velocity.x;
        const float vy = velocity.y;
        const float vz = velocity.z;
        const float velocitySq = vx * vx + vy * vy + vz * vz;

        const float dvx = velocity.x - m_lastVelocity.x;
        const float dvy = velocity.y - m_lastVelocity.y;
        const float dvz = velocity.z - m_lastVelocity.z;
        const float velocityDeltaSq = dvx * dvx + dvy * dvy + dvz * dvz;

        const float sleepThresholdSq = kSleepVelocityEpsilon * kSleepVelocityEpsilon;
        const bool allowSleeping = (m_contactSystem == nullptr);
        const bool positionChanged = displacementSq > sleepThresholdSq || !TransformApproxEqual(m_transform, m_lastTransform, kSleepVelocityEpsilon);
        const bool velocityAwake = velocitySq > sleepThresholdSq;
        const bool impulseAwake = velocityDeltaSq > sleepThresholdSq;

        if (isPlayer)
        {
            Wake();
        }
        else if (positionChanged || velocityAwake || impulseAwake)
        {
            Wake();
        }
        else if (allowSleeping && m_stationaryFrameCount < kSleepFrameThreshold)
        {
            ++m_stationaryFrameCount;
            if (m_stationaryFrameCount >= kSleepFrameThreshold)
            {
                m_isSleeping = true;
            }
        }
        else
        {
            m_isSleeping = false;
            m_stationaryFrameCount = 0;
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

