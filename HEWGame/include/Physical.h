// Physical.h
#ifndef __PHYSICAL_H__
#define __PHYSICAL_H__

#include "Vector.h"

namespace Physics
{
    // Default values (weight=1.0f, damping=0.98f) target a pseudo-2D space where
    // planar velocity is gently reduced each step after external forces are applied.
    struct Physical
    {
        float3 velocity;  // Planar velocity; Z is zeroed each step to keep motion 2D.
        float3 force;     // Accumulated external force/impulse consumed during the next integration step.
        float weight;     // Mass-like scalar; force is divided by this before integrating velocity.
        float damping;    // Per-step damping multiplier applied to planar velocity after forces.

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
