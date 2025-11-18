#include "Systems/IntegrateSystem.h"

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

    float3 IntegrateSystem::Integrate(SceneSpace::PhysicalEntity& entity, float dt)
    {
        float3 translation{ 0.0f, 0.0f, 0.0f };
        if (!(dt > 0.0f))
        {
            return translation;
        }

        Physics::Physical& physical = entity.GetPhysical();
        if (physical.velocity.z != 0.0f)
        {
#if defined(_DEBUG)
            if (m_verboseLogging)
            {
                std::ostringstream oss;
                oss << kLogPrefix << "Entity '" << entity.GetTag()
                    << "' velocity.z=" << physical.velocity.z
                    << " reset before integration to enforce planar motion.\n";
                DebugTrace(oss.str());
            }
#endif
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

        SceneSpace::TranslatePhysicalEntity(entity, translation);

#if defined(_DEBUG)
        if (m_verboseLogging)
        {
            std::ostringstream oss;
            oss << kLogPrefix << "Entity '" << entity.GetTag()
                << "' velocity=(" << physical.velocity.x << ',' << physical.velocity.y << ',' << physical.velocity.z << ')'
                << " dt=" << safeDt
                << " damping=" << damping
                << " weight=" << physical.weight
                << " translation=(" << translation.x << ',' << translation.y << ',' << translation.z << ")\n";
            DebugTrace(oss.str());
        }
#endif

        return translation;
    }
}
