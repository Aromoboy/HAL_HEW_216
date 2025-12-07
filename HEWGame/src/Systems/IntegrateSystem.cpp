#include "Systems/IntegrateSystem.h"

#include <algorithm>
#include <cmath>

namespace Systems
{
    IntegrateSystem::IntegrateSystem()
        : m_velocityThreshold(0.01f)
    {
    }

    float3 IntegrateSystem::Integrate(Entities::PhysicalEntity& entity, float dt)
    {
        float3 translation{ 0.0f, 0.0f, 0.0f };
        Physics::Physical& physical = entity.GetPhysical();
        if (!(dt > 0.0f))
        {
            physical.force = float3{ 0.0f, 0.0f, 0.0f };
            return translation;
        }

        const float safeDt = std::max(dt, 0.0f);
        const float weight = std::max(physical.weight, 0.0f);
        const float inverseWeight = (weight > 0.0f) ? 1.0f / weight : 0.0f;
        const float3 gravity
        {
            physical.force.x * inverseWeight,
            physical.force.y * inverseWeight,
            physical.force.z * inverseWeight
        };
        physical.force = float3{ 0.0f, 0.0f, 0.0f };

        physical.velocity.x += gravity.x * safeDt;
        physical.velocity.y += gravity.y * safeDt;
        physical.velocity.z += gravity.z * safeDt;

        const float damping = std::clamp(physical.damping, 0.0f, 1.0f);
        const float damp = std::pow(damping, safeDt * 60.0f);
        physical.velocity.x *= damp;
        physical.velocity.y *= damp;
        physical.velocity.z *= damp;

        translation.x = physical.velocity.x * safeDt;
        translation.y = physical.velocity.y * safeDt;
        translation.z = physical.velocity.z * safeDt;

        const float speedSq = (physical.velocity.x * physical.velocity.x)
            + (physical.velocity.y * physical.velocity.y)
            + (physical.velocity.z * physical.velocity.z);
        const float displacementSq = (translation.x * translation.x)
            + (translation.y * translation.y)
            + (translation.z * translation.z);
        if ((displacementSq < (0.003f * 0.003f)) && (speedSq < (m_velocityThreshold * m_velocityThreshold)))
        {
            translation = float3{ 0.0f, 0.0f, 0.0f };
            physical.velocity.x = 0.0f;
            physical.velocity.y = 0.0f;
            physical.velocity.z = 0.0f;
        }

        if ((translation.x != 0.0f) || (translation.y != 0.0f) || (translation.z != 0.0f))
        {
            Entities::TranslatePhysicalEntity(entity, translation);
        }

        return translation;
    }
}
