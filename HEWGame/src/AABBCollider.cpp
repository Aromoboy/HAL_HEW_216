#include "AABBCollider.h"

#include <algorithm>

namespace
{
}

Collision::AABBCollider::AABBCollider(Transform a_transform ,bool a_isTrigger, float3 a_expand, float3 a_offset, CollisionLayer layer, float3 a_centroidOffset)
{
        isTrigger = a_isTrigger;
        expand = a_expand;
        offset = a_offset;
        centroidOffset = a_centroidOffset;
        SetLayer(layer);

        UpdateBounds(a_transform);
}

bool Collision::AABBCollider::CheckCollision(const Collision::ICollider& other)
{

        const AABB thisAABB = aabb;
        const AABB otherAABB = other.GetAABB();

        constexpr float kPenetrationTolerance = 1e-4f;

        const float overlapX = std::min(thisAABB.max.x, otherAABB.max.x)
                - std::max(thisAABB.min.x, otherAABB.min.x) - kPenetrationTolerance;
        const float overlapY = std::min(thisAABB.max.y, otherAABB.max.y)
                - std::max(thisAABB.min.y, otherAABB.min.y) - kPenetrationTolerance;
        const float overlapZ = std::min(thisAABB.max.z, otherAABB.max.z)
                - std::max(thisAABB.min.z, otherAABB.min.z) - kPenetrationTolerance;

        return overlapX >= 0.0f && overlapY >= 0.0f && overlapZ >= 0.0f;

}

void Collision::AABBCollider::UpdateBounds(const Transform& transform)
{
        const float3 colliderScale{
                transform.scale.x * 0.5f * expand.x,
                transform.scale.y * 0.5f * expand.y,
                transform.scale.z * 0.5f * expand.z
        };

        const float3 scaledOffset{
                (offset.x + centroidOffset.x) * transform.scale.x,
                (offset.y + centroidOffset.y) * transform.scale.y,
                (offset.z + centroidOffset.z) * transform.scale.z
        };

        const float3 center{
                transform.position.x + scaledOffset.x,
                transform.position.y + scaledOffset.y,
                transform.position.z + scaledOffset.z
        };

        const float3 max{
                center.x + colliderScale.x,
                center.y + colliderScale.y,
                center.z + colliderScale.z
        };
        const float3 min{
                center.x - colliderScale.x,
                center.y - colliderScale.y,
                center.z - colliderScale.z
        };

        aabb = AABB(max, min);
}

bool Collision::AABBCollider::IsTrriger()
{
        return isTrigger;
}

AABB Collision::AABBCollider::GetAABB() const
{
        return aabb;
}
