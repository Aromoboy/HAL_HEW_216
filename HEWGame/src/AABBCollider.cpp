#include "AABBCollider.h"

namespace
{
}

Collision::AABBCollider::AABBCollider(Transform a_transform, bool a_isTrigger, float3 a_expand, float3 a_offset, CollisionLayer layer, float3 a_centroidOffset)
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
        constexpr float kPenetrationTolerance = 1e-4f;
        return aabb.Intersects(other.GetAABB(), kPenetrationTolerance);

}

void Collision::AABBCollider::UpdateBounds(const Transform& transform)
{
        const float3 halfExt{
                0.5f * expand.x * transform.scale.x,
                0.5f * expand.y * transform.scale.y,
        };

        const float halfZ = 0.5f * expand.z * transform.scale.z;

        const float3 scaledOffset{
                (offset.x + centroidOffset.x) * transform.scale.x,
                (offset.y + centroidOffset.y) * transform.scale.y,
                (offset.z + centroidOffset.z) * transform.scale.z
        };

        const float3 worldCenter{
                transform.position.x + scaledOffset.x,
                transform.position.y + scaledOffset.y,
                transform.position.z + scaledOffset.z
        };

        const float3 worldMin{
                worldCenter.x - halfExt.x,
                worldCenter.y - halfExt.y,
                worldCenter.z - halfZ
        };
        const float3 worldMax{
                worldCenter.x + halfExt.x,
                worldCenter.y + halfExt.y,
                worldCenter.z + halfZ
        };

        aabb = AABB(worldMax, worldMin);
}

bool Collision::AABBCollider::IsTrriger()
{
        return isTrigger;
}

AABB Collision::AABBCollider::GetAABB() const
{
        return aabb;
}
