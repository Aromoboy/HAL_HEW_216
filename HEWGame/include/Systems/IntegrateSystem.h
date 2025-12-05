#pragma once

#include "Entities/PhysicalEntity.h"

namespace Systems
{
    class IntegrateSystem
    {
    public:
        IntegrateSystem();
        ~IntegrateSystem() = default;

        float3 Integrate(Entities::PhysicalEntity& entity, float dt);

    private:
        float m_translationEpsilon;
    };
}
