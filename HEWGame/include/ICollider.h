// ICollider.h

#ifndef __ICOLLIDER_H__
#define __ICOLLIDER_H__


#include <cstdint>

#include "Transform.h"
#include "AABB.h"
#include "CollisionLayer.h"

namespace Collision
{
    enum class ColliderShape : std::uint8_t
    {
        Unknown,
        Box,
        Prism2D,
        Sphere,
        Capsule,
    };

    struct ContactManifold
    {
        bool intersects{ false };
        float3 normal{ 0.0f, 1.0f, 0.0f };
        float penetration{ 0.0f };
    };

    class ICollider
    {
    public:
        virtual ~ICollider() = default;

        void SetLayer(CollisionLayer layer) { m_layer = layer; }
        CollisionLayer GetLayer() const { return m_layer; }

        virtual ColliderShape GetShape() const = 0;
        virtual AABB GetAABB() const = 0;
        virtual void UpdateFromTransform(const Transform& transform) = 0;
        virtual ContactManifold ComputeContact(const Collision::ICollider& other) const = 0;

        bool CheckCollision(const Collision::ICollider& other) const
        {
            return ComputeContact(other).intersects;
        }

    protected:
        CollisionLayer m_layer{ CollisionLayer::Default };
    };
}
#endif __ICOLLIDER_H__
