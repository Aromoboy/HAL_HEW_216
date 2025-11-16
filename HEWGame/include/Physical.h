// Physical.h
#ifndef __PHYSICAL_H__
#define __PHYSICAL_H__

#include "Vector.h"

namespace Physics
{
    struct Physical
    {
        float3 velocity;  // ë¨ìx
        float3 force;     // óÕ
        float weight;     // èdÇ≥
        float damping;    // å∏êä

        Physical()
            : velocity{ 0.0f, 0.0f, 0.0f }
            , force{ 0.0f, 0.0f, 0.0f }
            , weight(1.0f)
            , damping(0.98f)
        {
        }

        Physical(float3 a_velocity, float3 a_force, float a_weight, float a_damping)
            : velocity(a_velocity)
            , force(a_force)
            , weight(a_weight)
            , damping(a_damping)
        {
        }
    };
}
#endif