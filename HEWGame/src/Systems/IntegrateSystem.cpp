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
        if (!(dt > 0.0f))
        {
            return translation;
        }

        Physics::Physical& physical = entity.GetPhysical();

        if (!(physical.weight > 0.0f))
        {
            physical.force = float3{ 0.0f, 0.0f, 0.0f };
            return translation;
        }

        if (entity.IsSleeping())
        {
            physical.force = float3{ 0.0f, 0.0f, 0.0f };
            return translation;
        }

        const float invMass = 1.0f / physical.weight;

        const float3 acceleration{
            physical.force.x * invMass,
            physical.force.y * invMass,
            physical.force.z * invMass
        };

        physical.velocity.x += acceleration.x * dt;
        physical.velocity.y += acceleration.y * dt;
        physical.velocity.z += acceleration.z * dt;

        physical.force = float3{ 0.0f, 0.0f, 0.0f };

        const float damping = std::clamp(physical.damping, 0.0f, 0.999f);
        physical.damping = damping;

        physical.velocity.x *= damping;
        physical.velocity.y *= damping;
        physical.velocity.z *= damping;

        const float speedSq =
            physical.velocity.x * physical.velocity.x +
            physical.velocity.y * physical.velocity.y +
            physical.velocity.z * physical.velocity.z;

        const float thresholdSq = m_velocityThreshold * m_velocityThreshold;
        if (speedSq < thresholdSq)
        {
            physical.velocity = float3{ 0.0f, 0.0f, 0.0f };
        }

        translation.x = physical.velocity.x * dt;
        translation.y = physical.velocity.y * dt;
        translation.z = physical.velocity.z * dt;

        if ((translation.x != 0.0f) || (translation.y != 0.0f) || (translation.z != 0.0f))
        {
            Entities::TranslatePhysicalEntity(entity, translation);
        }

        return translation;
    }
}
