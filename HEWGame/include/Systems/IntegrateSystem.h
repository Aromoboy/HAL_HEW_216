#pragma once

#include "PhysicalEntity.h"

#include <cstddef>

namespace Systems
{
    class IntegrateSystem
    {
    public:
        IntegrateSystem();
        ~IntegrateSystem() = default;

        void SetVerboseLoggingEnabled(bool enabled);
        bool IsVerboseLoggingEnabled() const;

        float3 Integrate(SceneSpace::PhysicalEntity& entity, float dt);

    private:
        float m_translationEpsilon;
        bool m_verboseLogging;
    };
}
