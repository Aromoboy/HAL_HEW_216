// 物理の速度と位置を時間で進める計算を行う
// 物理更新フェーズで使われる

#pragma once

#include <cstddef>

#include "Vector.h"

namespace Entities
{
    class PhysicalEntity;
}

namespace Systems
{
    class IntegrateSystem
    {
    public:
        IntegrateSystem();
        ~IntegrateSystem() = default;

        void SetVerboseLoggingEnabled(bool enabled);
        bool IsVerboseLoggingEnabled() const;

        float3 Integrate(Entities::PhysicalEntity& entity, float dt);

    private:
        float m_translationEpsilon;
        bool m_verboseLogging;
    };
}
