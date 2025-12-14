#include "Entities/CollisionEntity.h"

#include <memory>
#include <sstream>
#include <utility>
#include <vector>

#if defined(_WIN32)
#    ifndef NOMINMAX
#        define NOMINMAX
#    endif
#    include <windows.h>
#endif

#include "Collision2D.h"
#include "DebugColliderRenderer.h"
#include "Entities/PhysicalEntity.h"

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

    bool AreFloat3Equal(const float3& lhs, const float3& rhs)
    {
        return lhs.x == rhs.x && lhs.y == rhs.y && lhs.z == rhs.z;
    }

    bool AreTransformsEqual(const Transform& lhs, const Transform& rhs)
    {
        return AreFloat3Equal(lhs.position, rhs.position)
            && AreFloat3Equal(lhs.rotation, rhs.rotation)
            && AreFloat3Equal(lhs.scale, rhs.scale);
    }

    float2 ApplyBox2DToWorld(const b2Vec2& local, const Transform& transform)
    {
        const float3 worldOffset = Entities::PhysicalEntity::ToWorldVector(local);
        const float cosR = std::cos(transform.rotation.z);
        const float sinR = std::sin(transform.rotation.z);

        return float2{
            transform.position.x + (cosR * worldOffset.x - sinR * worldOffset.y),
            transform.position.y + (sinR * worldOffset.x + cosR * worldOffset.y),
        };
    }

    bool DebugDrawBox2DShapes(const Entities::PhysicalEntity& entity, const Color& circleColor, const Color& hullColor, const Color& aabbColor)
    {
        const auto& colliderComponent = entity.GetBoxColliderComponent();
        if (!colliderComponent || colliderComponent->shapes.empty())
        {
            return false;
        }

        const Transform& transform = entity.GetTransform();
        const float z = transform.position.z;

        DebugColliderRenderer::DrawAABB(entity.GetAABB(), aabbColor);

        for (const auto& shape : colliderComponent->shapes)
        {
            switch (shape.type)
            {
            case Physics::ColliderShapeType::Circle:
            {
                const b2Vec2 localCenter = shape.vertices.empty() ? b2Vec2{ 0.0f, 0.0f } : shape.vertices.front();
                const float2 center = ApplyBox2DToWorld(localCenter, transform);
                const float radius = Entities::PhysicalEntity::ToWorldLength(shape.radius);
                DebugColliderRenderer::DrawCircle(center, radius, z, circleColor);
                break;
            }
            case Physics::ColliderShapeType::Box:
            case Physics::ColliderShapeType::Polygon:
            default:
            {
                if (shape.vertices.empty())
                {
                    break;
                }

                std::vector<float2> hull;
                hull.reserve(shape.vertices.size());
                for (const auto& vertex : shape.vertices)
                {
                    hull.push_back(ApplyBox2DToWorld(vertex, transform));
                }

                DebugColliderRenderer::DrawHull(hull, z, hullColor);
                break;
            }
            }
        }

        return true;
    }
}

namespace Entities
{
    CollisionEntity::CollisionEntity(const Transform& transform, std::shared_ptr<Collision::ICollider> collider, Collision::CollisionLayer layer, std::shared_ptr<Render::Model> model, Core::UpdatePhase phase, int priority, std::string tag)
        : Entity(transform, std::move(model), phase, priority, std::move(tag))
        , m_collider(std::move(collider))
        , m_collisionLayer(layer)
    {
        if (m_collider)
        {
            m_collider->SetLayer(layer);
        }
        SyncColliderBounds();

        m_lastTransform = m_transform;
        m_lastBoundsTransform = m_transform;
    }

    CollisionEntity::CollisionEntity(const std::string& tag, const float3& pos, const float3& rot, const float3& scl, std::shared_ptr<Collision::ICollider> collider, Collision::CollisionLayer layer, Core::UpdatePhase phase, int priority)
        : CollisionEntity(Transform{ pos, rot, scl }, std::move(collider), layer, nullptr, phase, priority, tag)
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

    void CollisionEntity::SetTransform(const Transform& transform)
    {
        const bool positionChanged = !AreFloat3Equal(transform.position, m_transform.position);
        const bool rotationChanged = !AreFloat3Equal(transform.rotation, m_transform.rotation);
        const bool scaleChanged = !AreFloat3Equal(transform.scale, m_transform.scale);

        if (!positionChanged && !rotationChanged && !scaleChanged)
        {
            return;
        }

        MarkTransformDirty();
        m_transform = transform;
        SyncColliderBounds();
    }

    void CollisionEntity::SetPosition(const float3& position)
    {
        if (AreFloat3Equal(position, m_transform.position))
        {
            return;
        }

        MarkTransformDirty();
        m_transform.position = position;
        SyncColliderBounds();
    }

    void CollisionEntity::SetRotation(const float3& rotation)
    {
        if (AreFloat3Equal(rotation, m_transform.rotation))
        {
            return;
        }

        MarkTransformDirty();
        m_transform.rotation = rotation;
        SyncColliderBounds();
    }

    void CollisionEntity::SetScale(const float3& scale)
    {
        if (AreFloat3Equal(scale, m_transform.scale))
        {
            return;
        }

        MarkTransformDirty();
        m_transform.scale = scale;
        SyncColliderBounds();
    }

    void CollisionEntity::SyncColliderBounds()
    {
        if (!m_collider)
            return;

        m_collider->UpdateBounds(m_transform);
        m_aabb = m_collider->GetAABB();
        m_lastBoundsTransform = m_transform;
        m_transformDirty = false;
    }

    void CollisionEntity::MarkTransformDirty()
    {
        m_lastTransform = m_transform;
        m_transformDirty = true;
    }

    bool CollisionEntity::IsTransformDirty() const
    {
        return m_transformDirty;
    }

    void CollisionEntity::ApplyTranslation(const float3& delta)
    {
        if (delta.x == 0.0f && delta.y == 0.0f && delta.z == 0.0f)
        {
            return;
        }

        MarkTransformDirty();
        m_transform.position.x += delta.x;
        m_transform.position.y += delta.y;
        m_transform.position.z += delta.z;
        SyncColliderBounds();
    }

    void CollisionEntity::OnUpdate(Core::UpdateContext& ctx)
    {
        Entity::OnUpdate(ctx);

        if (m_transformDirty || !AreTransformsEqual(m_transform, m_lastBoundsTransform))
        {
            SyncColliderBounds();
        }
    }

    void CollisionEntity::DebugDraw()
    {
        const Color aabbColor{ 1.0f, 1.0f, 0.0f, 1.0f };
        const Color circleColor{ 0.0f, 1.0f, 0.0f, 1.0f };
        const Color hullColor{ 0.0f, 0.75f, 1.0f, 1.0f };

        if (!m_collider)
        {
            if (auto* physical = dynamic_cast<PhysicalEntity*>(this))
            {
                if (DebugDrawBox2DShapes(*physical, circleColor, hullColor, aabbColor))
                {
                    m_lastBoundsTransform = m_transform;
                    m_transformDirty = false;
                }
            }
            return;
        }

        if (m_transformDirty)
        {
            SyncColliderBounds();
        }

        std::vector<const Collision::ICollider*> colliders;
        colliders.push_back(m_collider.get());

        while (!colliders.empty())
        {
            const Collision::ICollider* collider = colliders.back();
            colliders.pop_back();
            if (!collider)
            {
                continue;
            }

            const AABB bounds = collider->GetAABB();
            const float z = (bounds.min.z + bounds.max.z) * 0.5f;
            DebugColliderRenderer::DrawAABB(bounds, aabbColor);

            if (const auto* circle = dynamic_cast<const Collision::CircleCollider2D*>(collider))
            {
                DebugColliderRenderer::DrawCircle(circle->GetCenter(), circle->GetRadius(), z, circleColor);
                continue;
            }

            if (const auto* hull = dynamic_cast<const Collision::ConvexHullCollider2D*>(collider))
            {
                DebugColliderRenderer::DrawHull(hull->GetWorldPoints(), z, hullColor);
                continue;
            }

            if (const auto* compound = dynamic_cast<const Collision::CompoundCollider2D*>(collider))
            {
                const auto& children = compound->GetChildren();
                for (auto it = children.rbegin(); it != children.rend(); ++it)
                {
                    colliders.push_back(it->get());
                }
            }
        }

        m_aabb = m_collider->GetAABB();
        m_lastBoundsTransform = m_transform;
        m_transformDirty = false;
    }

    void CollisionEntity::UpdateSleepState(const float3& velocity)
    {
        constexpr float kSleepVelocityThresholdSq = 1e-4f;
        constexpr int kSleepFrameThreshold = 10;

        const float speedSq = velocity.x * velocity.x + velocity.y * velocity.y + velocity.z * velocity.z;
        if (speedSq <= kSleepVelocityThresholdSq)
        {
            ++m_stationaryFrameCount;
            if (m_stationaryFrameCount >= kSleepFrameThreshold)
            {
                m_isSleeping = true;
            }
        }
        else
        {
            m_stationaryFrameCount = 0;
            m_isSleeping = false;
        }

        m_lastVelocity = velocity;
    }

    void CollisionEntity::Wake()
    {
        m_isSleeping = false;
        m_stationaryFrameCount = 0;
    }
}

