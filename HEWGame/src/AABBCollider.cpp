#include "AABBCollider.h"

#include <algorithm>

namespace
{
}

Collision::AABBCollider::AABBCollider(Transform a_transform ,bool a_isTrigger, float3 a_expand, float3 a_offset, CollisionLayer layer)
{
        isTrigger = a_isTrigger;
        expand = a_expand;
        offset = a_offset;
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
}

bool Collision::AABBCollider::IsTrriger()
{
        return isTrigger;
}

AABB Collision::AABBCollider::GetAABB() const
{
        return aabb;
}
