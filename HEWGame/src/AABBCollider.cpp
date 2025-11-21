#include "AABBCollider.h"

#include <sstream>
#include <string>
#include <algorithm>

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
}

Collision::AABBCollider::AABBCollider(Transform a_transform ,bool a_isTrigger, float3 a_expand, float3 a_offset, CollisionLayer layer)
{
        isTrigger = a_isTrigger;
        expand = a_expand;
        offset = a_offset;
        SetLayer(layer);

        UpdateFromTransform(a_transform);
}

void Collision::AABBCollider::UpdateFromTransform(const Transform& transform)
{
        float3 max;
        float3 min;

        float3 colliderScale;
        colliderScale.x = transform.scale.x / 2 * expand.x;
        colliderScale.y = transform.scale.y / 2 * expand.y;
        colliderScale.z = transform.scale.z / 2 * expand.z;


        max.x = transform.position.x + offset.x + colliderScale.x;
        min.x = transform.position.x + offset.x - colliderScale.x;

        max.y = transform.position.y + offset.y + colliderScale.y;
        min.y = transform.position.y + offset.y - colliderScale.y;

        max.z = transform.position.z + offset.z + colliderScale.z;
        min.z = transform.position.z + offset.z - colliderScale.z;

        aabb = AABB(max, min);

#if defined(_DEBUG)
        std::ostringstream oss;
        oss << "[AABBCollider] UpdateFromTransform this=" << this
                << " min=(" << min.x << ',' << min.y << ',' << min.z << ')'
                << " max=(" << max.x << ',' << max.y << ',' << max.z << ")\n";
        DebugTrace(oss.str());
#endif
}

bool Collision::AABBCollider::IsTrriger()
{
        return isTrigger;
}

AABB Collision::AABBCollider::GetAABB() const
{
        return aabb;
}

Collision::ContactManifold Collision::AABBCollider::ComputeContact(const Collision::ICollider& other) const
{
        ContactManifold manifold{};

        const AABB thisAABB = aabb;
        const AABB otherAABB = other.GetAABB();

        const float overlapX = std::min(thisAABB.max.x, otherAABB.max.x) - std::max(thisAABB.min.x, otherAABB.min.x);
        const float overlapY = std::min(thisAABB.max.y, otherAABB.max.y) - std::max(thisAABB.min.y, otherAABB.min.y);
        const float overlapZ = std::min(thisAABB.max.z, otherAABB.max.z) - std::max(thisAABB.min.z, otherAABB.min.z);

        if (overlapX <= 0.0f || overlapY <= 0.0f || overlapZ <= 0.0f)
        {
                return manifold;
        }

        manifold.intersects = true;
        manifold.hasContact = true;
        manifold.penetration = overlapX;
        manifold.normal = float3{ (thisAABB.min.x + thisAABB.max.x) < (otherAABB.min.x + otherAABB.max.x) ? -1.0f : 1.0f, 0.0f, 0.0f };

        if (overlapY < manifold.penetration)
        {
                manifold.penetration = overlapY;
                manifold.normal = float3{ 0.0f, (thisAABB.min.y + thisAABB.max.y) < (otherAABB.min.y + otherAABB.max.y) ? -1.0f : 1.0f, 0.0f };
        }

        if (overlapZ < manifold.penetration)
        {
                manifold.penetration = overlapZ;
                manifold.normal = float3{ 0.0f, 0.0f, (thisAABB.min.z + thisAABB.max.z) < (otherAABB.min.z + otherAABB.max.z) ? -1.0f : 1.0f };
        }

        return manifold;
}
