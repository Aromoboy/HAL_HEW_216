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


        // X軸方向で重なっているかを確認
        if (thisAABB.max.x < otherAABB.min.x ||           // X軸方向で分離している
                thisAABB.min.x > otherAABB.max.x) return false;

        // Y軸方向で重なっているかを確認
        if (thisAABB.max.y < otherAABB.min.y ||           // Y軸方向で分離している
                thisAABB.min.y > otherAABB.max.y) return false;

        return true;

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
