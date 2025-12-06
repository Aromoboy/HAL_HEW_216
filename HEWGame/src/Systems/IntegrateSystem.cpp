#include "Systems/IntegrateSystem.h"

#include <algorithm>

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
            physical.velocity.z = 0.0f;
            return translation;
        }

        const float safeDt = std::max(dt, 0.0f);
        const float weight = std::max(physical.weight, 0.0f);
        const float inverseWeight = (weight > 0.0f) ? 1.0f / weight : 0.0f;
        physical.velocity.x += physical.force.x * inverseWeight * safeDt;
        physical.velocity.y += physical.force.y * inverseWeight * safeDt;
        physical.velocity.z += physical.force.z * inverseWeight * safeDt;
        physical.force = float3{ 0.0f, 0.0f, 0.0f };

        const float damping = std::clamp(physical.damping, 0.0f, 1.0f);
        physical.velocity.x *= damping;
        physical.velocity.y *= damping;
        physical.velocity.z *= damping;

        if (physical.velocity.z != 0.0f)
        {
            physical.velocity.z = 0.0f;
        }

        translation.x = physical.velocity.x * safeDt;
        translation.y = physical.velocity.y * safeDt;
        translation.z = 0.0f;

        const float planarSpeedSq = (physical.velocity.x * physical.velocity.x) + (physical.velocity.y * physical.velocity.y);
        if (planarSpeedSq < (m_velocityThreshold * m_velocityThreshold))
        {
            translation = float3{ 0.0f, 0.0f, 0.0f };
            physical.velocity = translation;
        }

        if ((translation.x == 0.0f) && (translation.y == 0.0f))
        {
            return translation;
        }

        Entities::TranslatePhysicalEntity(entity, translation);

        return translation;
    }
}
