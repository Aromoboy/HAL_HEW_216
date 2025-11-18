// ============================================================================
// Tv: Physical ɊւC^[tF[Xf[^\錾Ainclude ėpł悤ɂB
// ړI: ߂ẴW[ɐGJ҂łAӖƈˑ֌Wfcł悤ɍx̐t^ĂB
// Ag: include z̑t@CVXewƘA邽߁At@CԂ̊֌WǐՂۂ̖ڈƂėpłB
// ============================================================================

// l: Physical.h Ɋւ{ŕ⑫ĂB
#ifndef __PHYSICAL_H__
#define __PHYSICAL_H__

#include "Vector.h"

namespace Physics
{
    struct Physical
    {
        float3 velocity;  // ‘¬“x
        float3 force;     // —Í
        float weight;     // d‚³
        float damping;    // Œ¸Š

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