#include "Systems/IntegrateSystem.h"

#include <algorithm>
#include <cmath>

namespace Systems
{
    IntegrateSystem::IntegrateSystem()
        : m_translationEpsilon(1e-4f)
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
        if (physical.velocity.z != 0.0f)
        {
            physical.velocity.z = 0.0f;
        }
        const float safeDt = std::max(dt, 0.0f);
        const float damping = std::clamp(physical.damping, 0.0f, 1.0f);
        const float translationScale = safeDt * damping;
        translation.x = physical.velocity.x * translationScale;
        translation.y = physical.velocity.y * translationScale;
        translation.z = 0.0f;

        const bool hasTranslation = (std::fabs(translation.x) > m_translationEpsilon)
            || (std::fabs(translation.y) > m_translationEpsilon)
            || (std::fabs(translation.z) > m_translationEpsilon);
        if (!hasTranslation)
        {
            return float3{ 0.0f, 0.0f, 0.0f };
        }

        Entities::TranslatePhysicalEntity(entity, translation);

        return translation;
    }
}
