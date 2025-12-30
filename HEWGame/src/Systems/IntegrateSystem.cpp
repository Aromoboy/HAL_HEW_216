#include "Systems/IntegrateSystem.h"

#include "Entities/PhysicalEntity.h"

#include <algorithm>
#include <cmath>
#include <sstream>
#include <string>

#if defined(_WIN32)
#    ifndef NOMINMAX
#        define NOMINMAX
#    endif
#    include <windows.h>
#endif

namespace
{
#if defined(_WIN32) && defined(_DEBUG)
    void DebugTrace(const std::string& message)
    {
        ::OutputDebugStringA(message.c_str());
    }
#else
    void DebugTrace(const std::string& message)
    {
        (void)message;
    }
#endif

    constexpr const char* kLogPrefix = "[IntegrateSystem] ";
}

namespace Systems
{
    IntegrateSystem::IntegrateSystem()
        : m_translationEpsilon(1e-4f)
        , m_verboseLogging(true)
    {
    }

    void IntegrateSystem::SetVerboseLoggingEnabled(bool enabled)
    {
        m_verboseLogging = enabled;
    }

    bool IntegrateSystem::IsVerboseLoggingEnabled() const
    {
        return m_verboseLogging;
    }

float3 IntegrateSystem::Integrate(Entities::PhysicalEntity& entity, float dt)
    {
        float3 translation{ 0.0f, 0.0f, 0.0f };
        if (!(dt > 0.0f))
        {
            return translation;
        }

        const float3 before = entity.GetTransform().position;
        entity.SyncTransformFromBox2DBody();
        const float3 after = entity.GetTransform().position;

        translation.x = after.x - before.x;
        translation.y = after.y - before.y;
        translation.z = after.z - before.z;

        const bool hasTranslation = (std::fabs(translation.x) > m_translationEpsilon)
            || (std::fabs(translation.y) > m_translationEpsilon)
            || (std::fabs(translation.z) > m_translationEpsilon);
        return hasTranslation ? translation : float3{ 0.0f, 0.0f, 0.0f };
    }
}
