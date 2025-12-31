#ifndef ___AABB_H___
#define ___AABB_H___

#include <algorithm>

#include "Vector.h"

struct AABB
{
    float3 max;
    float3 min;

    AABB(float3 a_max, float3 a_min)
    {
        max = a_max;
        min = a_min;
    };

    AABB()
    {
        AABB::max = float3{ 0.0f,0.0f,0.0f };
        AABB::min = float3{ 0.0f,0.0f,0.0f };
    };

    float3 Center() const
    {
        return float3
        {
            (min.x + max.x) * 0.5f,
            (min.y + max.y) * 0.5f,
            (min.z + max.z) * 0.5f,
        };
    }

    float3 Extents() const
    {
        return float3
        {
            (max.x - min.x) * 0.5f,
            (max.y - min.y) * 0.5f,
            (max.z - min.z) * 0.5f,
        };
    }

    bool Intersects(const AABB& other, float tolerance = 0.0f) const
    {
        const float overlapX = std::min(max.x, other.max.x) - std::max(min.x, other.min.x) - tolerance;
        const float overlapY = std::min(max.y, other.max.y) - std::max(min.y, other.min.y) - tolerance;
        const float overlapZ = std::min(max.z, other.max.z) - std::max(min.z, other.min.z) - tolerance;

        return overlapX >= 0.0f && overlapY >= 0.0f && overlapZ >= 0.0f;
    }
};

#endif
